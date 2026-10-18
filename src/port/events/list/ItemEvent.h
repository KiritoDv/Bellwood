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

// Fires when "Use" is picked for a custom item in the pockets. The pocket menu is already closing.
DEFINE_EVENT(ItemUse,
    uint16_t item;  // item number
    int32_t slot;   // pocket slot holding it
);
