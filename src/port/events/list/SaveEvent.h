#pragma once

#include <stdint.h>

// Saves

// Fires after the home save slot has been read into memory.
DEFINE_EVENT(SaveLoaded,
    bool hasTown;  // true when a valid town was loaded
);

// Fires after the town has been written to disk.
DEFINE_EVENT(SaveWritten,
    int32_t mode;  // save mode from the game, 0 ends the session
);

// Fires after a GameCube memory card save has been imported.
DEFINE_EVENT(SaveImported,
    const char* path;  // imported file
);
