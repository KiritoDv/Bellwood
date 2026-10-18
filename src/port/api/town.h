#pragma once

#include <stdint.h>

#include "api.h"
#include "m_name_table.h"

// The town being played. These keep their signatures across API versions, unlike the game's own functions.

typedef struct C_Time_s {
    uint16_t year;
    uint8_t month;   // 1 to 12
    uint8_t day;     // 1 to 31
    uint8_t weekday; // 0 is Sunday
    uint8_t hour;    // 0 to 23
    uint8_t minute;
    uint8_t second;
} C_Time;

// The scene being played (SCENE_* in m_scene_table.h), or -1 outside of gameplay.
BW_API int C_GetScene(void);
// TRUE when the player is outside in town, not inside a building or on the island.
BW_API int C_InTown(void);
// The game's date and time.
#define C_GetTime(time) C_GetTimeSized((time), sizeof(*(time)))
BW_API void C_GetTimeSized(C_Time* time, uint32_t size);
// Writes the ids of the villagers living in town to ids, up to capacity, and returns how many live there.
BW_API int C_GetTownVillagers(mActor_name_t* ids, int capacity);
