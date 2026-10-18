#pragma once

#include <stdint.h>

struct C_FurnitureDef_s;

// Furniture

// Fires when the game needs a furniture index past the built-in table.
// Set furniture to the definition that owns the index.
DEFINE_EVENT(FurnitureLookup,
    int32_t index;                              // furniture index (mRmTp_FtrItemNo2FtrIdx)
    const struct C_FurnitureDef_s* furniture;  // set by the listener that owns the index
);
