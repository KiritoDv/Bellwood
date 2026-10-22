#pragma once

#include <stdint.h>

#include "api.h"
#include "m_actor.h"
#include "m_name_table.h"

// The player being played. These keep their signatures across API versions, unlike the game's own functions.

// The player's actor, or NULL outside of gameplay.
BW_API ACTOR* C_GetPlayer(void);
// Writes the player's position. Returns FALSE outside of gameplay.
BW_API int C_GetPlayerPosition(float* x, float* y, float* z);

// Puts the item in the first free pocket. Returns the pocket, or -1 when the pockets are full or no player is loaded.
BW_API int C_GiveItem(mActor_name_t item);
// Takes one of the item out of the pockets. Returns FALSE when the player doesn't carry it.
BW_API int C_TakeItem(mActor_name_t item);
// How many of the item the player carries.
BW_API int C_CountItem(mActor_name_t item);

// Bells in the player's wallet.
BW_API uint32_t C_GetWallet(void);
// Sets the bells in the player's wallet, up to the most it holds.
BW_API void C_SetWallet(uint32_t bells);
