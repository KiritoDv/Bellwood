#ifdef ENABLE_SCRIPTING

#include "port/ShipCompat.h"
#include "port/ShipInit.hpp"
#include "port/api/ModOwner.hpp"
#include "port/audio/ModSound.h"
#include "port/events/Events.h"

#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <spdlog/spdlog.h>

extern "C" {
#include "port/api/sound.h"
}

#include <string>
#include <unordered_map>

namespace {

struct Replacement {
    int sound;
    int32_t loopStart;
    std::string owner;
};

std::unordered_map<int, std::string> sSounds;
std::unordered_map<int, Replacement> sReplacements;
int sReplacing = -1;

int Register(const uint8_t* data, size_t size, const char* what) {
    std::vector<int16_t> samples;
    if (!ModSound::Decode(data, size, samples)) {
        SPDLOG_ERROR("Sound: {} is not a WAV, MP3 or FLAC file", what);
        return -1;
    }
    const int sound = ModSound::Add(std::move(samples));
    sSounds[sound] = ModOwner::Current();
    return sound;
}

void InitSoundApi() {
    REGISTER_LISTENER(PlayBgmEvent, EVENT_PRIORITY_LOW, [](IEvent* event) {
        auto* play = reinterpret_cast<PlayBgmEvent*>(event);
        if (event->Cancelled) {
            return;
        }
        if (auto it = sReplacements.find(*play->id); it != sReplacements.end()) {
            ModSound::PlayMusic(it->second.sound, it->second.loopStart);
            sReplacing = *play->id;
        } else if (sReplacing >= 0) {
            ModSound::StopMusic();
            sReplacing = -1;
        }
    });
    REGISTER_LISTENER(StopBgmEvent, EVENT_PRIORITY_LOW, [](IEvent* event) {
        if (!event->Cancelled && sReplacing >= 0) {
            ModSound::StopMusic();
            sReplacing = -1;
        }
    });
    ModOwner::AddReleaser([](const std::string& mod) {
        std::erase_if(sReplacements, [&](const auto& entry) { return entry.second.owner == mod; });
        std::vector<int> sounds;
        for (const auto& [sound, owner] : sSounds) {
            if (owner == mod) {
                sounds.push_back(sound);
            }
        }
        for (int sound : sounds) {
            C_UnregisterSound(sound);
        }
    });
}

RegisterShipInitFunc sInitSoundApi(InitSoundApi);

} // namespace

extern "C" int C_RegisterSound(const char* path) {
    if (path == nullptr) {
        return -1;
    }
    const auto file = ShipCompat::GetResourceManager()->GetArchiveManager()->LoadFile(path);
    if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
        SPDLOG_ERROR("Sound: could not read {}", path);
        return -1;
    }
    return Register((const uint8_t*)file->Buffer->data(), file->Buffer->size(), path);
}

extern "C" int C_RegisterSoundData(const uint8_t* data, uint32_t size) {
    return Register(data, size, "the data");
}

extern "C" void C_UnregisterSound(int sound) {
    if (sSounds.erase(sound) == 0) {
        return;
    }
    std::erase_if(sReplacements, [sound](const auto& entry) { return entry.second.sound == sound; });
    ModSound::Remove(sound);
}

extern "C" uint32_t C_SoundLength(int sound) {
    return (uint32_t)ModSound::Length(sound);
}

extern "C" int C_PlaySound(int sound, float volume, float pan) {
    return ModSound::Play(sound, volume, pan);
}

extern "C" void C_StopSound(int voice) {
    ModSound::Stop(voice);
}

extern "C" int C_IsSoundPlaying(int voice) {
    return ModSound::Playing(voice);
}

extern "C" void C_PlayMusic(int sound, int32_t loop_start) {
    sReplacing = -1;
    ModSound::PlayMusic(sound, loop_start);
}

extern "C" void C_StopMusic(void) {
    sReplacing = -1;
    ModSound::StopMusic();
}

extern "C" void C_ReplaceBgm(uint8_t bgm, int sound, int32_t loop_start) {
    if (sound < 0 || !sSounds.contains(sound)) {
        sReplacements.erase(bgm);
        return;
    }
    sReplacements[bgm] = { sound, loop_start, ModOwner::Current() };
}

#endif
