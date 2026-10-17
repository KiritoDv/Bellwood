#pragma once

#include <stdint.h>

struct private_s;

// Saves

// Fires after the home save slot has been read into memory.
DEFINE_EVENT(SaveLoaded,
    bool hasTown;  // true when a valid town was loaded
);

// Fires after the town has been written to disk.
DEFINE_EVENT(SaveWritten,
    int32_t mode;  // save mode from the game, 0 ends the session
);

// Fires when the town loads and the player last quit or reset without saving. Cancellable.
// Cancel — Mr. Resetti does not visit and the reset is not counted.
DEFINE_EVENT(ResetDetected,
    struct private_s* player;
);

// Fires after a GameCube memory card save has been imported.
DEFINE_EVENT(SaveImported,
    const char* path;  // imported file
);
