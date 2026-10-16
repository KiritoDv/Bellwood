#include "jaudio_NES/game64.h"
#include "jaudio_NES/kappa.h"
#include "audio.h"

#include <string.h>

static int l_sound_frame_counter;
static u8 l_message_speed;

void Na_GameFrame(void) {
    l_sound_frame_counter++;
}

int Na_GetSoundFrameCounter(void) {
    return l_sound_frame_counter;
}

u8 Na_CheckRestartReady(void) {
    return 1;
}

void Na_RestartPrepare(void) {
}

void Na_Restart(void) {
}

void Na_SoftReset(void) {
}

u8 Na_BgmFadeoutCheck(void) {
    return 1;
}

u8 Na_SeFadeoutCheck(void) {
    return 1;
}

void Na_MessageStatus(u8 messageStatus) {
}

void Na_MessageSpeed(u8 messageSpeed) {
    l_message_speed = messageSpeed;
}

u8 Na_MessageSpeedGet(void) {
    return l_message_speed;
}

u8 Na_SubGameOK(void) {
    return 1;
}

void Na_SubGameStart(void) {
}

void Na_SubGameEnd(void) {
}

s8 Na_InstCountGet(void) {
    return 0;
}

u8 Na_RoomIncectPos(uintptr_t insectID, u8 index, u16 angle, f32 distance) {
    return 0;
}

int Na_GetKappaCounter(kappa* k) {
    if (k != NULL) {
        memset(k, 0, sizeof(*k));
    }
    return 0;
}

int Na_GetRadioCounter(Radio_c* radio) {
    if (radio != NULL) {
        memset(radio, 0, sizeof(*radio));
    }
    return 0;
}

f32 Na_GetRhythmAnimCounter(uintptr_t idx) {
    return 0.0f;
}

s8 Na_GetRhythmDelay(uintptr_t idx) {
    return 0;
}

void Na_GetRhythmInfo(TempoBeat_c* tempo) {
    if (tempo != NULL) {
        memset(tempo, 0, sizeof(*tempo));
    }
}

void Na_SetRhythmInfo(TempoBeat_c* tempo) {
}

void Na_GetStaffRollInfo(StaffRollInfo_c* info) {
    if (info != NULL) {
        memset(info, 0, sizeof(*info));
    }
}

void Na_BGMVolume(f32 volumeScale, u16 b) {
}

void Na_BgmCrossfadeStart(u8 a) {
}

void Na_BgmStart(u8 a) {
}

void Na_BgmStop(u16 a) {
}

void Na_FloorTrgStart(u8 index, u16 angle, f32 distance) {
}

void Na_FurnitureInst(uintptr_t id, u8 inst, u8* melody, u16 angle, f32 dist) {
}

void Na_FurnitureInstPos(uintptr_t id, u16 angle, f32 distance) {
}

void Na_Inst(u16 inst, u8* pData) {
}

void Na_KishaStatusLevel(f32 speed, uintptr_t ongenNum1, u16 angle1, f32 distance1, uintptr_t ongenNum2, u16 angle2,
                         f32 distance2) {
}

void Na_KishaStatusTrg(u8 a) {
}

void Na_LevSeEcho(u8 echo) {
}

void Na_MDPlayerPos(u16 angle, f32 distance, u16 b, u16 c, uintptr_t id) {
}

void Na_Museum(u8 museumType) {
}

void Na_NpcWalkSe(u16 id, u16 angle, f32 distance) {
}

void Na_NpcWalkSeRoom(u8 index, u16 angle, f32 distance) {
}

void Na_OngenPos(uintptr_t id, u8 index, u16 angle, f32 distance) {
}

void Na_OngenTrgStart(u16 a, u16 angle, f32 distance) {
}

void Na_OngenTrgStartSpeed(u16 num, u16 angle, f32 distance, f32 speed) {
}

void Na_Pause(u8 pauseFlag) {
}

void Na_PlayerStatusLevel(u8 playerDash, f32 playerSpeed) {
}

void Na_PlyWalkSe(u16 id, u16 angle, f32 distance) {
}

void Na_PlyWalkSeRoom(u8 a, u16 angle, f32 distance) {
}

void Na_RhythmAllStop(void) {
}

void Na_RhythmPos(uintptr_t index, u8 unused, u16 angle, f32 distance) {
}

void Na_RhythmStart(uintptr_t idx, s8 arg1, s8 arg2) {
}

void Na_RhythmStop(uintptr_t idx) {
}

void Na_RoomType(u8 roomType) {
}

void Na_SceneMode(u8 a) {
}

void Na_SetOutMode(u8 outMode) {
}

void Na_SetVoiceMode(u8 a) {
}

void Na_SpecChange(int a) {
}

void Na_SysLevStart(u8 a) {
}

void Na_SysLevStop(u8 a) {
}

void Na_SysTrgStart(u16 a) {
}

void Na_TTKK_ARM(u8 a) {
}

void Na_Tenki(u8 a) {
}

void Na_TrgSeEcho(u8 echo) {
}

void Na_VoiceSe(u8 a, u8 b, u8 c, s16 d, u8 e, u8 f) {
}

void Na_kazagurumaLevel(f32 kazagurumaSpeed) {
}
