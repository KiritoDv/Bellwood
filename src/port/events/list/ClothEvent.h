#pragma once

#include <stdint.h>

struct C_ClothDef_s;

// Clothes

// Fires when the game needs data for a shirt in the custom clothing range, either by its item number or by the
// furniture index of its mannequin. Set definition and the found fields to the shirt that owns it.
DEFINE_EVENT(ClothLookup,
    uint16_t item;                        // shirt item number, or EMPTY_NO to look up by furniture
    int32_t furniture;                    // mannequin furniture index, or -1 to look up by item
    const struct C_ClothDef_s* definition; // set by the listener that owns it
    uint16_t foundItem;                   // set by the listener
    int32_t foundFurniture;               // set by the listener: furniture index of its mannequin
);
