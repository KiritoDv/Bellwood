#pragma once

#include <stdint.h>

struct player_actor_s;

// Player state machine

// Fires when something requests a new player main state. Cancellable.
// Write *index to request a different state. Cancel — the request is dropped.
DEFINE_EVENT(PlayerStateRequest,
    struct player_actor_s* player;
    int32_t* index;    // mutable requested main index (mPlayer_INDEX_*)
    int32_t priority;  // request priority
);

// Fires after the player switches to a new main state.
DEFINE_EVENT(PlayerStateChanged,
    struct player_actor_s* player;
    int32_t prev;  // previous main index
    int32_t now;   // new main index
);

// Player data — polled once per play frame

// Fires when the amount of Bells in the player's wallet changes.
DEFINE_EVENT(PlayerMoneyChanged,
    uint32_t prev;
    uint32_t now;
);

// Fires once for every pocket slot whose item changed.
DEFINE_EVENT(PlayerInventoryChanged,
    int32_t slot;   // pocket slot index
    uint16_t prev;  // previous item name id
    uint16_t now;   // new item name id
);

// Fires when the player walks into another acre.
DEFINE_EVENT(PlayerAcreChanged,
    int32_t prevX;  // previous acre column, -1 after a scene load
    int32_t prevZ;  // previous acre row, -1 after a scene load
    int32_t x;
    int32_t z;
);
