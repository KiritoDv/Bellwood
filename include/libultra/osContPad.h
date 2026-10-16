/* This file is fabriacted. These structs are in a unified file in N64 SDK. */

#ifndef OS_CONT_PAD
#define OS_CONT_PAD

#include "types.h"
#include <libultraship/libultra/os.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef CONT_NO_ERROR
#define CONT_NO_ERROR 0
#endif
#ifndef CONT_NONE
#define CONT_NONE 0x0000
#endif

typedef struct {
    OSContPad pad;
    s8 substickX;
    s8 substickY;
    u8 triggerR;
    u8 triggerL;
} OSContPadEx;

extern s32 osContStartQuery(OSMesgQueue* mq);
extern void osContGetQuery(OSContStatus* status);
extern s32 osContSetCh(u8 num_controllers);
extern void osContGetReadDataEx(OSContPadEx* pad);

#ifdef __cplusplus
}
#endif

#endif
