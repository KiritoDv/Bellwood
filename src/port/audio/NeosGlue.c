#ifdef ACPORT_AUDIO

#include "port/audio/NeosGlue.h"

#include "jaudio_NES/aictrl.h"
#include "jaudio_NES/audioconst.h"
#include "jaudio_NES/audiowork.h"
#include "jaudio_NES/dummyrom.h"
#include "jaudio_NES/effect.h"
#include "jaudio_NES/naport.h"
#include "jaudio_NES/neosthread.h"
#include "jaudio_NES/os.h"
#include "jaudio_NES/rate.h"
#include "jaudio_NES/rspsim.h"
#include "jaudio_NES/sub_sys.h"
#include "jaudio_NES/system.h"

#include <stdlib.h>
#include <string.h>

#define NA_PORT_HEAP_SCALE 2

static BOOL sNeosReady = FALSE;
static u64* sNeosHeap = NULL;
static NaAcmd* sNeosCmds = NULL;
static s32 sNeosCmdCount = 0;
static s16 sNeosFrame[NA_PORT_FRAME_SAMPLES * 2];

BOOL Neos_CheckBoot(void) {
    return sNeosReady && NaPort_PreloadDone();
}

u32 SetPreCopy_NeosRom(u8* load_addr, u32 load_size, BOOL cut_flag) {
    return 0;
}

BOOL ARAMStartDMAmesg(u32 dir, void* dram, uintptr_t devAddr, u32 size, s32 unused, OSMesgQueue* mq) {
    if (dir == DUMMYROM_ARAM_TO_DRAM && dram != NULL) {
        if (devAddr != 0) {
            memcpy(dram, (const void*)devAddr, size);
        } else {
            memset(dram, 0, size);
        }
    }

    if (mq != NULL) {
        Z_osSendMesg(mq, OS_MESG_PTR(NULL), OS_MESG_NOBLOCK);
    }

    return FALSE;
}

int NaPort_Boot(void) {
    s32 heap_size = AGC.acmdBufSize * NA_PORT_HEAP_SCALE;

    if (sNeosReady) {
        return TRUE;
    }

    sNeosHeap = (u64*)calloc(heap_size / sizeof(u64), sizeof(u64));
    if (sNeosHeap == NULL) {
        return FALSE;
    }

    Nas_InitAudio(sNeosHeap, heap_size);

    sNeosCmdCount = AG.max_audio_cmds * 2;
    sNeosCmds = (NaAcmd*)calloc(sNeosCmdCount, sizeof(NaAcmd));
    if (sNeosCmds == NULL) {
        return FALSE;
    }

    sNeosReady = TRUE;
    return TRUE;
}

int NaPort_RenderFrame(int16_t* out) {
    s32 n = 0;
    s32 i;

    if (sNeosReady) {
        n = CreateAudioTask(sNeosCmds, sNeosFrame, JAC_FRAMESAMPLES, 0);
    }

    if (n > 0) {
        RspStart2(sNeosCmds, n, RSPSIM_MODE_INIT);
    } else {
        memset(sNeosFrame, 0, sizeof(sNeosFrame));
    }

    Jac_MixExtTrack(sNeosFrame, NA_PORT_FRAME_SAMPLES);

    for (i = 0; i < NA_PORT_FRAME_SAMPLES; i++) {
        out[i * 2 + 0] = sNeosFrame[i * 2 + 1];
        out[i * 2 + 1] = sNeosFrame[i * 2 + 0];
    }

    return NA_PORT_FRAME_SAMPLES;
}

void NaPort_SetVolumes(float music, float sfx) {
    Nas_SetPortGroupVolume(NA_GROUP0, sfx);
    Nas_SetPortGroupVolume(NA_GROUP1, music);
    Nas_SetPortGroupVolume(NA_GROUP2, sfx);
    Nas_SetPortGroupVolume(NA_GROUP3, music);
    Nas_SetPortGroupVolume(NA_GROUP4, sfx);
}

void NaPort_Shutdown(void) {
    sNeosReady = FALSE;
}

#endif
