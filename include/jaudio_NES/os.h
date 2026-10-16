#ifndef JAUDIO_NES_OS_H
#define JAUDIO_NES_OS_H

#include "types.h"
#include "libultra/libultra.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NaIoMesg {
    OSIoMesgHdr hdr;
    void* dramAddr;
    uintptr_t devAddr;
    u32 size;
} NaIoMesg;

extern void Z_osWritebackDCacheAll();
extern void osInvalDCache2(void* src, s32 size);
extern void osWritebackDCache2(void* src, s32 size);
extern void Z_osCreateMesgQueue(OSMesgQueue* mq, OSMesg* msg, s32 count);
extern s32 Z_osSendMesg(OSMesgQueue* mq, OSMesg msg, s32 flags);
extern s32 Z_osRecvMesg(OSMesgQueue* mq, OSMesg* msg, s32 flags);
extern s32 Z_osEPiStartDma(OSPiHandle* handler, NaIoMesg* msg, s32 dir);
extern void Z_bcopy(void* src, void* dst, size_t size);

static inline s32 Z_osRecvMesg32(OSMesgQueue* mq, u32* msg, s32 flags) {
    OSMesg m;
    s32 res = Z_osRecvMesg(mq, &m, flags);

    if (msg != NULL) {
        *msg = m.data32;
    }
    return res;
}

static inline s32 Z_osRecvMesgPtr(OSMesgQueue* mq, void** msg, s32 flags) {
    OSMesg m;
    s32 res = Z_osRecvMesg(mq, &m, flags);

    if (msg != NULL) {
        *msg = m.ptr;
    }
    return res;
}

#ifdef __cplusplus
}
#endif

#endif
