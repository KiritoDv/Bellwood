#pragma once

#include <stdint.h>

#include "api.h"
#include "ac_furniture.h"

#define C_FAMICOM_FIRST 64
#define C_FAMICOM_LAST 127

// Registers an NES game from iNES (.nes) data, which is copied. id names the game's save file, keep it the same
// across versions. Returns the ROM number, or -1.
BW_API int C_RegisterFamicomGame(const char* id, const uint8_t* rom, uint32_t size);
// Same as C_RegisterFamicomGame, reading the ROM from a file inside a mod archive, e.g. "famicom/mygame.nes".
BW_API int C_RegisterFamicomGameFile(const char* id, const char* path);
// Unregisters a game registered with C_RegisterFamicomGame.
BW_API void C_UnregisterFamicomGame(int rom_no);
// Call from a furniture's move callback to make it an NES that plays rom_no when switched on.
BW_API void C_FamicomFurnitureMove(FTR_ACTOR* ftr_actor, ACTOR* my_room, GAME* game, int rom_no);
