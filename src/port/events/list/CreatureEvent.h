#pragma once

#include <stdint.h>

struct C_CreatureDef_s;
struct gyoei_spawn_info_weight_f_s;
struct insect_spawn_info_f_s;

// Fish and insects

// Fires when the game needs data for a fish or insect past the built-in ones, either by its index or by the
// furniture index of its placed version. Set definition and the found fields to the creature that owns it.
DEFINE_EVENT(CreatureLookup,
    uint8_t insect;                           // TRUE for insects, FALSE for fish, set by the listener by furniture
    int32_t index;                            // low byte of the item number, or -1 to look up by furniture
    int32_t furniture;                        // furniture index of the placed version, or -1 to look up by index
    const struct C_CreatureDef_s* definition; // set by the listener that owns it
    int32_t foundIndex;                       // set by the listener
    int32_t foundFurniture;                   // set by the listener: furniture index of the placed version
    int32_t catchMessage;                     // set by the listener: message shown when it is caught
);

// Fires after the game lists the fish that can appear in an acre. Append entries to add more.
DEFINE_EVENT(FishSpawnList,
    int32_t water;                                      // aSOG_RANGE_PROC_*: river, sea, offing, pond, tourney, island
    struct gyoei_spawn_info_weight_f_s* entries;        // aSOG_gyoei_spawn_info_weight_f_c, mutable
    int32_t* count;                                     // mutable number of entries
    int32_t capacity;                                   // size of entries
);

// Fires after the game lists the insects that can appear in town or on the island. Append entries to add more.
DEFINE_EVENT(InsectSpawnList,
    uint8_t island;                             // TRUE on the island
    struct insect_spawn_info_f_s* entries;      // aSOI_insect_spawn_info_f_c, mutable
    int32_t* count;                             // mutable number of entries
    int32_t capacity;                           // size of entries
);
