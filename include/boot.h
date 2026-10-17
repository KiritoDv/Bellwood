#ifndef BOOT_H
#define BOOT_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

extern void HotResetIplMenu();

extern_s void* boot_copyDate;

#if VERSION == VER_GAFU01_00
extern_s u8 SoftResetEnable;
#endif

#ifdef __cplusplus
}
#endif

#endif
