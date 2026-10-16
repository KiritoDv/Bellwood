#pragma once

#include <libultraship.h>

typedef struct {
    int index;
    Gfx instruction;
} GfxPatch;

#ifdef __cplusplus
extern "C" {
#endif

void ResourceMgr_PatchGfxByName(const char* path, const char* patchName, GfxPatch* patches, int patchCount);
uint8_t ResourceMgr_FileExists(const char* filePath);
void gSPSegment(void* value, int segNum, uintptr_t target);
void gSPSegmentLoadRes(void* value, int segNum, uintptr_t target);
void gSPDisplayList(Gfx* pkt, Gfx* dl);
void gSPDisplayListOffset(Gfx* pkt, Gfx* dl, int offset);
void gSPVertex(Gfx* pkt, uintptr_t v, int n, int v0);
void gSPInvalidateTexCache(Gfx* pkt, uintptr_t texAddr);

#ifdef __cplusplus
}
#endif
