#ifndef DUMMYROM_H
#define DUMMYROM_H

#include "types.h"
#include "jaudio_NES/audiostruct.h"
#include "libultra/libultra.h"

#define DUMMYROM_DRAM_TO_ARAM 0
#define DUMMYROM_ARAM_TO_DRAM 1

extern u32 SetPreCopy_NeosRom(u8* load_addr, u32 load_size, BOOL cut_flag);
extern BOOL ARAMStartDMAmesg(u32 dir, void* dram, uintptr_t devAddr, u32 size, s32 unused, OSMesgQueue* mq);

#endif
