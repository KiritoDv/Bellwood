#ifndef NAPORT_H
#define NAPORT_H

#include "types.h"
#include "jaudio_NES/audiostruct.h"

#ifdef __cplusplus
extern "C" {
#endif

extern BOOL NaPort_GetSeq(s32 seq_id, const u8** data, u32* size);
extern BOOL NaPort_GetFont(s32 bank_id, voiceinfo* info);

extern void NaPort_StartPreload(void);
extern BOOL NaPort_PollPreload(void);
extern BOOL NaPort_PreloadDone(void);

extern void NaPort_LockQueues(void);
extern void NaPort_UnlockQueues(void);
extern BOOL NaPort_WaitQueues(u32 timeout_ms);
extern void NaPort_SignalQueues(void);

#ifdef __cplusplus
}
#endif

#endif
