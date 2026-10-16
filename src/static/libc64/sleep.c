#include "libc64/sleep.h"
#include "libultra/libultra.h"
#include "libultra/os_timer.h"

void csleep(OSTime c) {
    OSMesg msg;
    OSMesgQueue mq;
    OSTimer timer;

    osCreateMesgQueue(&mq, &msg, 1);
    osSetTimer(&timer, c, 0, &mq, OS_MESG_PTR(NULL));
    osRecvMesg(&mq, NULL, OS_MESG_BLOCK);
}

void msleep(u32 ms) {
    csleep(OS_USEC_TO_CYCLES((u64)ms * 1000));
}
