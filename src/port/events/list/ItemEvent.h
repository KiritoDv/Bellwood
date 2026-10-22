#pragma once

#include <stdint.h>

struct C_ItemDef_s;

// Items

// Fires when the game needs data for an item number in the custom item range.
// Set definition to the definition that owns the item.
DEFINE_EVENT(ItemLookup,
    uint16_t item;                           // item number
    const struct C_ItemDef_s* definition;   // set by the listener that owns the item
);

// Fires whenever the player obtains an item and the game records it as collected for the catalog.
DEFINE_EVENT(ItemCollected,
    uint16_t item;  // item number
);

// Fires when "Use" is picked for a custom item in the pockets. The pocket menu is already closing.
// Fires when the museum checks an item it doesn't know, e.g. a custom fish or insect.
// Set donator to make it a museum item.
DEFINE_EVENT(MuseumDonatorLookup,
    uint16_t item;
    int donator;  // set by the listener that owns the item: 0 for nobody, 1-4 for a player, 5 for a removed player
);

// Fires when the player donates an item the museum doesn't know. Set accepted when it takes the item.
DEFINE_EVENT(MuseumDonate,
    uint16_t item;
    int donator;  // player donating it, 1-4
    bool accepted;
);

// Fires when a player is removed from the town, after the museum marks their donations as from a removed player.
DEFINE_EVENT(MuseumDonatorRemoved,
    int donator;  // 1-4
);

DEFINE_EVENT(ItemUse,
    uint16_t item;  // item number
    int32_t slot;   // pocket slot holding it
);
