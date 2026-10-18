#pragma once

#include <stdint.h>

struct C_VillagerDef_s;

// Villagers

// Fires when the game needs data for a villager id in the custom range. Set definition, name and catchphrase to
// the villager that owns it.
DEFINE_EVENT(VillagerLookup,
    uint16_t npcId;                           // villager id
    const struct C_VillagerDef_s* definition; // set by the listener that owns it
    const uint8_t* name;                      // set by the listener: ANIMAL_NAME_LEN bytes in the game's encoding
    const uint8_t* catchphrase;               // set by the listener: ANIMAL_CATCHPHRASE_LEN bytes in the game's encoding
);
