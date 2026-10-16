#ifndef LIBULTRA_H
#define LIBULTRA_H

#include "types.h"
#include <string.h>
#if !defined(_WIN32)
#include <strings.h>
#endif
#include <libultraship/libultra/os.h>
#include <libultraship/libultra/convert.h>
#include "libultra/gu.h"
#include "libultra/osMesg.h"
#include "libultra/shutdown.h"
#include "libultra/os_timer.h"
#include "libultra/os_thread.h"
#include "libultra/os_pi.h"
#include "libultra/initialize.h"
#include "libc64/math64.h" /* TODO: sins and coss belong in libultra */

#define N64_SCREEN_HEIGHT 240
#define N64_SCREEN_WIDTH 320

#ifdef __cplusplus
extern "C" {
#endif

typedef u64 Z_OSTime;

#undef bcmp
#undef bcopy
#undef bzero
#define bcmp(v1, v2, size) memcmp((v1), (v2), (size))
#define bcopy(src, dst, n) memmove((dst), (src), (n))
#define bzero(ptr, size) memset((ptr), 0, (size))

void osSyncPrintf(const char* fmt, ...);

extern s32 osAppNMIBuffer[16];
extern int osShutdown;
extern u8 __osResetSwitchPressed;

#ifdef __cplusplus
}
#endif

#endif
