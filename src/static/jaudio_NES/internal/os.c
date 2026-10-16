#include "jaudio_NES/os.h"
#include "jaudio_NES/dummyrom.h"
#include "jaudio_NES/naport.h"
#include "jaudio_NES/sample.h"
#include "gc_compat.h"

#define Z_OS_RECV_TIMEOUT_MS 250

extern void Z_osWritebackDCacheAll() {
}

extern void osInvalDCache2(void* src, s32 size) {
}

extern void osWritebackDCache2(void* src, s32 size) {
}

extern void Z_osCreateMesgQueue(OSMesgQueue* mq, OSMesg* msg, s32 count) {
    NaPort_LockQueues();
    mq->msg = msg;
    mq->msgCount = count;
    mq->validCount = 0;
    mq->first = 0;
    NaPort_UnlockQueues();
}

extern s32 Z_osSendMesg(OSMesgQueue* mq, OSMesg msg, s32 flags) {
    int count;

    NaPort_LockQueues();
    if (mq->validCount == mq->msgCount) {
        NaPort_UnlockQueues();
        return -1;
    }

    count = mq->first + mq->validCount;

    if (count >= mq->msgCount) {
        count -= mq->msgCount;
    }

    mq->msg[count] = msg;

    mq->validCount++;
    NaPort_UnlockQueues();
    NaPort_SignalQueues();

    return 0;
}

extern s32 Z_osRecvMesg(OSMesgQueue* mq, OSMesg* msg, s32 flags) {
    NaPort_LockQueues();
    if (flags == OS_MESG_BLOCK) {
        while (!mq->validCount) {
            if (!NaPort_WaitQueues(Z_OS_RECV_TIMEOUT_MS)) {
                OSReport("Z_osRecvMesg: timed out waiting for a message\n");
                break;
            }
        }
    }

    if (mq->validCount == 0) {
        NaPort_UnlockQueues();
        if (msg != NULL) {
            msg->ptr = NULL;
        }
        return -1;
    }

    mq->validCount -= 1;

    if (msg != NULL) {
        *msg = mq->msg[mq->first];
    }

    mq->first++;

    if (mq->first == mq->msgCount) {
        mq->first = 0;
    }

    NaPort_UnlockQueues();
    return 0;
}

extern s32 Z_osEPiStartDma(OSPiHandle* handler, NaIoMesg* msg, s32 dir) {
    ARAMStartDMAmesg(1, msg->dramAddr, msg->devAddr, msg->size, 0, msg->hdr.retQueue);
    return 0;
}

void Z_bcopy(void* src, void* dst, size_t size) {
    Jac_bcopy(src, dst, size);
}
