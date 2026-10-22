#pragma once

#include <stdint.h>

struct C_VillagerDef_s;

// Villagers

// Fires when the game needs data for a villager id in the custom range. Set definition, name and catchphrase to
// the villager that owns it, and room and roomTop when it has its own room layout.
DEFINE_EVENT(VillagerLookup,
    uint16_t npcId;                           // villager id
    const struct C_VillagerDef_s* definition; // set by the listener that owns it
    const uint8_t* name;                      // set by the listener: ANIMAL_NAME_LEN bytes in the game's encoding
    const uint8_t* catchphrase;               // set by the listener: ANIMAL_CATCHPHRASE_LEN bytes in the game's encoding
    const uint16_t* room;                     // set by the listener: 16x16 item numbers on the floor, or NULL
    const uint16_t* roomTop;                  // set by the listener: 16x16 item numbers on top of furniture, or NULL
);

// Fires when a villager starts living in town: a new one moving in, one replacing another, or one arriving from
// another town.
DEFINE_EVENT(VillagerMovedIn,
    uint16_t npcId;
    int32_t slot;  // index in the town's villager list
);

// Fires when a villager stops living in town.
DEFINE_EVENT(VillagerMovedOut,
    uint16_t npcId;
    int32_t slot;  // index in the town's villager list it had
);

// Fires every frame an NPC is ready to be talked to. Cancellable.
// Cancel — the player can't start talking to it this frame.
DEFINE_EVENT(NpcTalkRequest,
    struct actor_s* npc;
    uint16_t npcId;
);

// Fires when the player starts talking to an NPC.
DEFINE_EVENT(NpcTalkStart,
    struct actor_s* npc;
    uint16_t npcId;
);
