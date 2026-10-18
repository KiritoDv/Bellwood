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

// Fires when the player catches a fish with the rod, as the catch is shown.
DEFINE_EVENT(FishCaught,
    uint16_t item;  // fish item number, built-in or custom
);

// Fires when the player catches an insect with the net, as the catch is shown.
DEFINE_EVENT(InsectCaught,
    uint16_t item;  // insect item number, built-in or custom
);

// Fires when the player digs a hole with the shovel.
DEFINE_EVENT(PlayerDig,
    const float* pos;  // world position of the hole, x/y/z
    uint16_t item;     // item dug up, EMPTY_NO for none
);

// Fires when the player starts shaking a tree.
DEFINE_EVENT(PlayerShakeTree,
    uint16_t tree;  // tree item on the tile
    int32_t utX;    // tile, in town units
    int32_t utZ;
);

// Fires when a letter lands in a player's mailbox.
DEFINE_EVENT(LetterReceived,
    int32_t player;  // player of the town, 0-3
    int32_t slot;    // mailbox slot, 0-9
);

// Fires when a player's loan changes: a payment, or a new loan for a bigger house.
DEFINE_EVENT(LoanChanged,
    int32_t player;
    uint32_t previous;
    uint32_t loan;
);

// Fires when a player's house changes size.
DEFINE_EVENT(HouseSizeChanged,
    int32_t player;
    int32_t previous;  // mHm_HOMESIZE_*
    int32_t size;
);
