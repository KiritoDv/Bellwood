#pragma once

#include <stdint.h>

// Field

// Why an item on the ground changed. The player's causes are told apart by what they were doing on or next to the
// tile.
enum {
    FIELD_CHANGE_GAME,      // not the player: growth, villagers, the day changing, events, mods
    FIELD_CHANGE_DROP,      // put down or thrown
    FIELD_CHANGE_PICK_UP,   // picked up, including digging something up
    FIELD_CHANGE_BURY,      // buried or planted in a hole
    FIELD_CHANGE_DIG,       // hole dug
    FIELD_CHANGE_FILL,      // hole filled
    FIELD_CHANGE_CHOP,      // hit with the axe
    FIELD_CHANGE_SHAKE,     // tree shaken, so something fell
    FIELD_CHANGE_WEED,      // weed pulled
    FIELD_CHANGE_FURNITURE, // furniture pushed, pulled, turned or picked up
};

// Fires when the item on a tile of the scene changes: in town, on the island or in a room (polled). block_x and
// block_z are the scene's blocks, block 0, 0 in rooms; ut_x and ut_z the tile in the block.
DEFINE_EVENT(FieldItemChanged,
    int32_t block_x;
    int32_t block_z;
    int32_t ut_x;
    int32_t ut_z;
    uint16_t prev;
    uint16_t now;
    int32_t cause; // FIELD_CHANGE_*
);
