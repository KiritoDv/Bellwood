#ifdef ACPORT_AUDIO

#include "importers/NeosAudioResources.h"

#include <bridge/resourcebridge.h>
#include <ship/resource/ResourceManager.h>
#include <spdlog/spdlog.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <future>
#include <mutex>
#include <vector>

extern "C" {
#include "jaudio_NES/naport.h"
}

namespace {

constexpr const char* kSeqTablePath = "audio/audio_seq_table";
constexpr const char* kFontTablePath = "audio/audio_soundfont_table";
constexpr double kSlowLoadMs = 5.0;

std::mutex sQueueMutex;
std::condition_variable sQueueSignal;

std::vector<std::shared_future<std::shared_ptr<Ship::IResource>>> sPreload;
size_t sPreloadNext = 0;
std::chrono::steady_clock::time_point sPreloadStart;
std::atomic<bool> sPreloaded = true;

std::shared_ptr<NeosAudio::AudioTable> LoadTable(const char* path) {
    auto manager = ResourceGetResourceManager();
    if (manager == nullptr) {
        return nullptr;
    }
    return std::static_pointer_cast<NeosAudio::AudioTable>(manager->LoadResource(path));
}

std::shared_ptr<Ship::IResource> LoadTableEntry(const char* tablePath, s32 index) {
    const auto start = std::chrono::steady_clock::now();
    auto table = LoadTable(tablePath);
    if (table == nullptr || index < 0 || (size_t)index >= table->mEntries.size()) {
        return nullptr;
    }

    uint64_t crc = table->mEntries[index].crc;
    if (crc == 0) {
        return nullptr;
    }

    auto resource = ResourceGetResourceManager()->LoadResource(crc);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    if (ms >= kSlowLoadMs) {
        SPDLOG_DEBUG("audio: {} {} took {:.1f} ms to load", tablePath == kSeqTablePath ? "sequence" : "font", index,
                     ms);
    }
    return resource;
}

} // namespace

extern "C" BOOL NaPort_GetSeq(s32 seq_id, const u8** data, u32* size) {
    auto sequence = std::static_pointer_cast<NeosAudio::Sequence>(LoadTableEntry(kSeqTablePath, seq_id));
    if (sequence == nullptr || sequence->mData.empty()) {
        return FALSE;
    }

    *data = sequence->mData.data();
    *size = (u32)sequence->mData.size();
    return TRUE;
}

extern "C" BOOL NaPort_GetFont(s32 bank_id, voiceinfo* info) {
    auto font = std::static_pointer_cast<NeosAudio::SoundFont>(LoadTableEntry(kFontTablePath, bank_id));
    if (font == nullptr) {
        return FALSE;
    }

    const voiceinfo& loaded = font->mInfo;
    info->num_instruments = loaded.num_instruments;
    info->num_drums = loaded.num_drums;
    info->num_sfx = loaded.num_sfx;
    info->instruments = loaded.instruments;
    info->percussion = loaded.percussion;
    info->effects = loaded.effects;
    return TRUE;
}

extern "C" void NaPort_StartPreload(void) {
    auto manager = ResourceGetResourceManager();
    if (manager == nullptr) {
        return;
    }

    sPreload.clear();
    sPreloadNext = 0;
    sPreloadStart = std::chrono::steady_clock::now();

    for (const char* tablePath : { kSeqTablePath, kFontTablePath }) {
        auto table = LoadTable(tablePath);
        if (table == nullptr) {
            continue;
        }

        for (const auto& entry : table->mEntries) {
            const char* path = entry.crc != 0 ? ResourceGetNameByCrc(entry.crc) : nullptr;
            if (path != nullptr) {
                sPreload.push_back(manager->LoadResourceAsync(path));
            }
        }
    }

    sPreloaded = sPreload.empty();
}

extern "C" BOOL NaPort_PollPreload(void) {
    if (sPreloaded) {
        return TRUE;
    }

    while (sPreloadNext < sPreload.size() &&
           sPreload[sPreloadNext].wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        sPreloadNext++;
    }
    if (sPreloadNext < sPreload.size()) {
        return FALSE;
    }

    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - sPreloadStart).count();
    SPDLOG_INFO("audio: preloaded {} sequences and fonts in {:.0f} ms", sPreload.size(), ms);
    sPreload.clear();
    sPreloaded = true;
    return TRUE;
}

extern "C" BOOL NaPort_PreloadDone(void) {
    return sPreloaded ? TRUE : FALSE;
}

extern "C" void NaPort_LockQueues(void) {
    sQueueMutex.lock();
}

extern "C" void NaPort_UnlockQueues(void) {
    sQueueMutex.unlock();
}

extern "C" BOOL NaPort_WaitQueues(u32 timeout_ms) {
    std::unique_lock<std::mutex> lock(sQueueMutex, std::adopt_lock);
    bool signalled = sQueueSignal.wait_for(lock, std::chrono::milliseconds(timeout_ms)) == std::cv_status::no_timeout;
    lock.release();
    return signalled ? TRUE : FALSE;
}

extern "C" void NaPort_SignalQueues(void) {
    sQueueSignal.notify_all();
}

#endif
