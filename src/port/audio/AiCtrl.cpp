extern "C" {
#include "jaudio_NES/aictrl.h"
#include "jaudio_NES/rate.h"
}

#include <mutex>

namespace {

constexpr int kMixMin = -0x8000;
constexpr int kMixMax = 0x7FFF;

std::recursive_mutex sMixMutex;
MixCallback sMixCallback = nullptr;
u8 sMixMode = MixMode_Mono;

s16 MixClamp(int mix) {
    if (mix < kMixMin) {
        return (s16)(kMixMin + 1);
    }
    if (mix > kMixMax) {
        return (s16)kMixMax;
    }
    return (s16)mix;
}

void MixMonoTrack(s16* track, s32 nSamples, const s16* mono) {
    for (s32 i = 0; i < nSamples; i++) {
        track[i * 2 + 0] = MixClamp(track[i * 2 + 0] + mono[i]);
        track[i * 2 + 1] = MixClamp(track[i * 2 + 1] + mono[i]);
    }
}

void MixMonoTrackWide(s16* track, s32 nSamples, const s16* mono) {
    for (s32 i = 0; i < nSamples; i++) {
        track[i * 2 + 0] = MixClamp(track[i * 2 + 0] + mono[i]);
        track[i * 2 + 1] = MixClamp(track[i * 2 + 1] - mono[i]);
    }
}

void MixExtraTrack(s16* track, s32 nSamples, const s16* extra) {
    const s16* src0 = extra + JAC_FRAMESAMPLES;
    const s16* src1 = extra;

    for (s32 i = 0; i < nSamples; i++) {
        track[i * 2 + 0] = MixClamp(track[i * 2 + 0] + src0[i]);
        track[i * 2 + 1] = MixClamp(track[i * 2 + 1] + src1[i]);
    }
}

void MixInterleaveTrack(s16* track, s32 nSamples, const s16* interleave) {
    for (s32 i = 0; i < nSamples * 2; i++) {
        track[i] = MixClamp(track[i] + interleave[i]);
    }
}

} // namespace

extern "C" void Jac_RegisterMixcallback(MixCallback mixcallback, u8 mixmode) {
    std::lock_guard<std::recursive_mutex> lock(sMixMutex);
    sMixCallback = mixcallback;
    sMixMode = mixmode;
}

extern "C" MixCallback Jac_GetMixcallback(u8* mixmode) {
    std::lock_guard<std::recursive_mutex> lock(sMixMutex);
    *mixmode = sMixMode;
    return sMixCallback;
}

extern "C" void Jac_LockMix(void) {
    sMixMutex.lock();
}

extern "C" void Jac_UnlockMix(void) {
    sMixMutex.unlock();
}

extern "C" void Jac_MixExtTrack(s16* dac, s32 nSamples) {
    std::lock_guard<std::recursive_mutex> lock(sMixMutex);
    if (sMixCallback == nullptr) {
        return;
    }

    const u8 mode = sMixMode;
    const s16* src = sMixCallback(nSamples);
    if (src == nullptr) {
        return;
    }

    switch (mode) {
        case MixMode_Mono:
            MixMonoTrack(dac, nSamples, src);
            break;
        case MixMode_MonoWide:
            MixMonoTrackWide(dac, nSamples, src);
            break;
        case MixMode_Extra:
            MixExtraTrack(dac, nSamples, src);
            break;
        case MixMode_Interleave:
            MixInterleaveTrack(dac, nSamples, src);
            break;
        default:
            break;
    }
}
