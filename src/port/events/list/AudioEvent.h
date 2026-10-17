#pragma once

#include <stdint.h>

// Audio

// Fires before background music starts. Cancellable.
// Write *id to play a different track. Cancel — the music does not start.
DEFINE_EVENT(PlayBgmEvent,
    uint8_t* id;  // mutable BGM id
);

// Fires before background music stops. Cancellable.
// Cancel — the music keeps playing.
DEFINE_EVENT(StopBgmEvent,
    uint16_t* id;  // mutable BGM id
);

// Fires before a system (menu / UI) sound effect plays. Cancellable.
// Write *id to play a different sound. Cancel — the sound does not play.
DEFINE_EVENT(PlaySystemSfxEvent,
    uint16_t* id;  // mutable sound effect id (NA_SE_*)
);

// Fires before a positional sound effect plays. Cancellable.
// Write *id to play a different sound. Cancel — the sound does not play.
DEFINE_EVENT(PlaySfxEvent,
    uint16_t* id;      // mutable sound effect id (NA_SE_*)
    const float* pos;  // world position, x/y/z
);
