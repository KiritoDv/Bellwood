#ifndef _JAUDIO_AICTRL_H
#define _JAUDIO_AICTRL_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif // ifdef __cplusplus

typedef s16* (*MixCallback)(s32);

typedef enum _MixMode {
	MixMode_Mono       = 0,
	MixMode_MonoWide   = 1,
	MixMode_Extra      = 2,
	MixMode_Interleave = 3,
	MixMode_Num, // 4
} MixMode;

void Jac_RegisterMixcallback(MixCallback mixcallback, u8 mixmode);
extern MixCallback Jac_GetMixcallback(u8* mixmode);

void Jac_LockMix(void);
void Jac_UnlockMix(void);
void Jac_MixExtTrack(s16* dac, s32 nSamples);

#ifdef __cplusplus
};
#endif // ifdef __cplusplus

#endif
