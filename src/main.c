#include "main.h"

#include "boot.h"
#include "graph.h"
#include "libultra/osMesg.h"
#include "libultra/libultra.h"
#include "libultra/initialize.h"
#include "libc64/malloc.h"
#include "m_card.h"
#include "_mem.h"
#include "padmgr.h"
#include "m_msg.h"
#include "Famicom/famicom.h"
#include "m_debug.h"
#include "m_land.h"
#include "ac_port.h"
#include "gc_compat.h"
#include "jaudio_NES/game64.h"

#include <stdlib.h>

u8 SegmentBaseAddress[0x40];

int ScreenWidth = SCREEN_WIDTH;
int ScreenHeight = SCREEN_HEIGHT;

static OSMesg serialMsgBuf;
static OSMesgQueue l_serialMsgQ;
static u8* l_game_heap;

static void audioFatalCallback(void) {
    OSReport("audio: fatal error reported by Neos\n");
}

void AC_BootInit(void) {
    __osInitialize_common();

    Na_InitAudio_PC(audioFatalCallback);

    l_game_heap = (u8*)malloc(AC_GAME_HEAP_SIZE);
    MallocInit(l_game_heap, AC_GAME_HEAP_SIZE);

    new_Debug_mode();
    SETREG(SREG, 0, 0);

    ScreenWidth = SCREEN_WIDTH;
    ScreenHeight = SCREEN_HEIGHT;

    mCD_init_card();

    osCreateMesgQueue(&l_serialMsgQ, &serialMsgBuf, 1);
    padmgr_Init(&l_serialMsgQ);

    mMsg_aram_init2();
    mLd_StartFlagOn();
    famicom_mount_archive();
}
