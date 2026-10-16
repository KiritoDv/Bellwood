#ifndef _GBI_H_
#define _GBI_H_

#include <libultraship/libultra/gbi.h>

#ifdef __cplusplus
extern "C" {
#endif

void gSPSegment(void* value, int segNum, uintptr_t target);
void gSPSegmentLoadRes(void* value, int segNum, uintptr_t target);
void gSPDisplayList(Gfx* pkt, Gfx* dl);
void gSPDisplayListOffset(Gfx* pkt, Gfx* dl, int offset);
void gSPVertex(Gfx* pkt, uintptr_t v, int n, int v0);
void gSPInvalidateTexCache(Gfx* pkt, uintptr_t texAddr);

#ifdef __cplusplus
}
#endif

#define gSPSegment(pkt, segment, base) (gSPSegment)((pkt), (segment), (uintptr_t)(base))
#define gSPSegmentLoadRes(pkt, segment, base) (gSPSegmentLoadRes)((pkt), (segment), (uintptr_t)(base))
#define gSPDisplayList(pkt, dl) (gSPDisplayList)((pkt), (Gfx*)(uintptr_t)(dl))
#define gSPVertex(pkt, v, n, v0) (gSPVertex)((pkt), (uintptr_t)(v), (n), (v0))
#define gSPInvalidateTexCache(pkt, addr) (gSPInvalidateTexCache)((pkt), (uintptr_t)(addr))

#define G_UCODE_LOAD_REMOVED(name) sizeof(struct { _Static_assert(0, #name ": microcode loads are not supported"); int unused; })

#undef gSPLoadUcode
#undef gSPLoadUcodeL
#undef gSPLoadUcodeEx
#undef gsSPLoadUcode
#undef gsSPLoadUcodeL
#undef gsSPLoadUcodeEx
#define gSPLoadUcode(pkt, uc_start, uc_dstart) do { (void)G_UCODE_LOAD_REMOVED(gSPLoadUcode); } while (0)
#define gSPLoadUcodeL(pkt, ucode) do { (void)G_UCODE_LOAD_REMOVED(gSPLoadUcodeL); } while (0)
#define gSPLoadUcodeEx(pkt, uc_start, uc_dstart, uc_dsize) do { (void)G_UCODE_LOAD_REMOVED(gSPLoadUcodeEx); } while (0)
#define gsSPLoadUcode(uc_start, uc_dstart) {{ G_UCODE_LOAD_REMOVED(gsSPLoadUcode), 0 }}
#define gsSPLoadUcodeL(ucode) {{ G_UCODE_LOAD_REMOVED(gsSPLoadUcodeL), 0 }}
#define gsSPLoadUcodeEx(uc_start, uc_dstart, uc_dsize) {{ G_UCODE_LOAD_REMOVED(gsSPLoadUcodeEx), 0 }}

#endif
