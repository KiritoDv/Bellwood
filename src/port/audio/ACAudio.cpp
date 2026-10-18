#include "ACAudio.h"
#include "ModSound.h"
#include "NeosGlue.h"
#include "importers/NeosAudioResources.h"

#include <libultraship/bridge/audiobridge.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <libultraship/bridge/resourcebridge.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#ifdef __APPLE__
#include <pthread.h>
#endif

extern "C" {
#include "jaudio_NES/naport.h"
}

namespace {

using Clock = std::chrono::steady_clock;

constexpr int kSampleRate = 32000;
constexpr auto kIdleWait = std::chrono::milliseconds(4);
constexpr int kClockLeadFrames = 4;
constexpr auto kPlayerStallGrace = std::chrono::milliseconds(250);

struct AudioThreadState {
    std::thread thread;
    std::mutex mutex;
    std::condition_variable wake;
    bool running = false;
    bool kicked = false;
    int gameFrame = 1;
};

AudioThreadState sAudio;

float VolumeSetting(const char* cvar) {
    return std::clamp(CVarGetInteger(cvar, 100), 0, 100) / 100.0f;
}

bool HavePlayer() {
    return AudioGetAudioComponent() != nullptr && AudioPlayerGetDesiredBuffered() > 0;
}

void SubmitFrame(int16_t* stereo) {
    const float volume = VolumeSetting("gSettings.Volume.Master");
    const int channels = std::max(2, (int)GetNumAudioChannels());
    static thread_local std::vector<int16_t> out;
    out.assign((size_t)NA_PORT_FRAME_SAMPLES * channels, 0);

    for (int i = 0; i < NA_PORT_FRAME_SAMPLES; i++) {
        out[(size_t)i * channels + 0] = (int16_t)(stereo[i * 2 + 0] * volume);
        out[(size_t)i * channels + 1] = (int16_t)(stereo[i * 2 + 1] * volume);
    }

    AudioPlayerPlayFrame(reinterpret_cast<const uint8_t*>(out.data()), out.size() * sizeof(int16_t));
}

void RenderFrame(int16_t* frame, bool submit) {
    const float music = VolumeSetting("gSettings.Volume.Music");
    const float sfx = VolumeSetting("gSettings.Volume.SFX");
    NaPort_SetVolumes(ModSound::MusicActive() ? 0.0f : music, sfx);
    NaPort_RenderFrame(frame);
    ModSound::Mix(frame, NA_PORT_FRAME_SAMPLES, music, sfx);

    if (submit) {
        SubmitFrame(frame);
    }
}

void AudioThreadMain() {
#ifdef __APPLE__
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
#endif

    int16_t frame[NA_PORT_FRAME_SAMPLES * 2];
    auto clockBase = Clock::now();
    int64_t clockFrames = 0;
    bool pacedByPlayer = HavePlayer();
    bool preloaded = false;

    std::unique_lock<std::mutex> lock(sAudio.mutex);
    while (sAudio.running) {
        sAudio.wake.wait_for(lock, kIdleWait, [] { return sAudio.kicked || !sAudio.running; });
        if (!sAudio.running) {
            break;
        }
        sAudio.kicked = false;
        const int maxFrames = std::max(1, sAudio.gameFrame) + 1;
        lock.unlock();

        if (!preloaded) {
            preloaded = NaPort_PollPreload();
            if (!preloaded) {
                lock.lock();
                continue;
            }
            clockBase = Clock::now();
            clockFrames = 0;
        }

        const bool havePlayer = HavePlayer();
        if (havePlayer != pacedByPlayer) {
            pacedByPlayer = havePlayer;
            clockBase = Clock::now();
            clockFrames = 0;
        }

        for (int i = 0; i < maxFrames; i++) {
            bool submit = pacedByPlayer;
            if (pacedByPlayer) {
                if (AudioPlayerBuffered() < AudioPlayerGetDesiredBuffered()) {
                    clockBase = Clock::now();
                    clockFrames = 0;
                } else {
                    const auto stalled =
                        std::chrono::duration<double>(Clock::now() - clockBase - kPlayerStallGrace).count();
                    if (stalled <= 0.0 || clockFrames >= (int64_t)(stalled * kSampleRate / NA_PORT_FRAME_SAMPLES)) {
                        break;
                    }
                    submit = false;
                }
            } else {
                const auto elapsed = std::chrono::duration<double>(Clock::now() - clockBase).count();
                const int64_t allowed = (int64_t)(elapsed * kSampleRate / NA_PORT_FRAME_SAMPLES) + kClockLeadFrames;
                if (clockFrames >= allowed) {
                    break;
                }
            }

            RenderFrame(frame, submit);
            clockFrames++;
        }

        lock.lock();
    }
}

} // namespace

extern "C" void AC_AudioInit(void) {
    if (sAudio.running) {
        return;
    }

    if (ResourceGetResourceManager() == nullptr) {
        SPDLOG_ERROR("AC_AudioInit: no resource manager; audio disabled");
        return;
    }

    NeosAudio::RegisterImporters();
    NaPort_StartPreload();

    if (!NaPort_Boot()) {
        SPDLOG_ERROR("AC_AudioInit: Neos failed to boot");
        return;
    }

    sAudio.running = true;
    sAudio.thread = std::thread(AudioThreadMain);
}

extern "C" void AC_AudioFrame(int gameFrame) {
    {
        std::lock_guard<std::mutex> lock(sAudio.mutex);
        sAudio.gameFrame = gameFrame;
        sAudio.kicked = true;
    }
    sAudio.wake.notify_one();
}

extern "C" void AC_AudioShutdown(void) {
    {
        std::lock_guard<std::mutex> lock(sAudio.mutex);
        if (!sAudio.running) {
            return;
        }
        sAudio.running = false;
    }
    sAudio.wake.notify_all();

    if (sAudio.thread.joinable()) {
        sAudio.thread.join();
    }
    NaPort_Shutdown();
}
