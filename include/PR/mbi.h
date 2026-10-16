#ifndef _MBI_H_
#define _MBI_H_

#include <PR/gbi.h>
#include <libultraship/libultra/mbi.h>

#define M_GFXTASK 1
#define M_AUDTASK 2
#define M_VIDTASK 3
#define M_HVQTASK 6
#define M_HVQMTASK 7

#undef SEGMENT_OFFSET
#undef SEGMENT_ADDR
#define SEGMENT_OFFSET(a) ((u32)(a) & 0x00FFFFFE)
#define SEGMENT_ADDR(num, off) (((num) << 24) + (off) + 1)

#endif
