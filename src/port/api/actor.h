#pragma once

#include <stdint.h>

#include "api.h"
#include "m_actor.h"
#include "m_play.h"

// Registers an actor profile and returns the profile number it was given, or -1.
// The profile is copied, its id field is set to the new number.
BW_API int16_t C_RegisterActor(const ACTOR_PROFILE* profile);
// Destroys every live actor using the profile and frees its number. Call it from MOD_EXIT.
BW_API void C_UnregisterActor(int16_t profile);
// Spawns an actor in the current scene. Returns NULL outside of gameplay or when the actor can't be created.
BW_API ACTOR* C_SpawnActor(int16_t profile, float x, float y, float z, int16_t angle_y, int16_t arg);
// The scene being played, or NULL outside of gameplay.
BW_API GAME_PLAY* C_GetPlay(void);
