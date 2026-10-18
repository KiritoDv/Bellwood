#pragma once

#include <stdint.h>

struct C_InteriorDef_s;

// Carpets and wallpapers

// Fires when the game needs data for a floor or wall index past the built-in ones (texture, name, price).
// Set definition and data to the carpet or wallpaper that owns the index.
DEFINE_EVENT(InteriorLookup,
    int32_t index;                              // room floor or wall index, also the item number's low byte
    uint8_t wall;                               // TRUE for a wallpaper, FALSE for a carpet
    const struct C_InteriorDef_s* definition;   // set by the listener that owns the index
    const uint8_t* data;                        // set by the listener: 16 colour palette, then the 64x64 CI4 tiles
);
