#pragma once

#include <stdint.h>

#include "api.h"

// Visiting another player's town through the game's own train trips. One player exports their town and sends it
// (in pieces: room messages carry up to 64 KB); the other makes it their second town, and the train station takes
// them there as a visitor, as with a second memory card. What the visitor changes is written to the received copy,
// not to the other player's file; SaveRequest lets a mod keep that copy from being saved at all.

// The home town file as last saved, packed. Returns its size; with buffer NULL or capacity too small, only the
// size, so call it twice. 0 when there is no saved town.
BW_API uint32_t C_ExportTown(void* buffer, uint32_t capacity);
// Makes a town from C_ExportTown the second town. The town chosen in the settings comes back with C_EndVisit.
// Returns FALSE for a broken file or this player's own town.
BW_API int C_VisitTown(const void* data, uint32_t size);
// Puts back the second town chosen in the settings. Returns FALSE while the player is in the visited town.
BW_API int C_EndVisit(void);
// TRUE while the player is a visitor in another town.
BW_API int C_IsVisiting(void);
