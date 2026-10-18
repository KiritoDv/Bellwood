#pragma once

#include <stdint.h>

struct C_StructureDef_s;

// Structures

// Fires whenever the game needs data for a structure item in the custom range. Set definition and profile to the
// structure that owns it, and the room fields when it has a room.
DEFINE_EVENT(StructureLookup,
    uint16_t item;                              // structure item
    const struct C_StructureDef_s* definition;  // set by the listener that owns it
    int16_t profile;                            // set by the listener: actor profile that draws it
    const uint16_t* room;                       // set by the listener: 16x16 item numbers on the floor of its room
    const uint16_t* roomTop;                    // set by the listener: 16x16 item numbers on top of furniture
    const uint32_t* roomCollision;              // set by the listener: 16x16 collision of a room with its own model
    int16_t wall;                               // set by the listener: wallpaper index of its room
    int16_t floor;                              // set by the listener: carpet index of its room
    const uint8_t* name;                        // set by the listener: 8 bytes in the game's encoding, or NULL
    uint16_t resident;                          // set by the listener: villager found in its room, or EMPTY_NO
    int16_t entryX;                             // set by the listener: where the player enters its room, x
    int16_t entryZ;                             // set by the listener: where the player enters its room, z
);
