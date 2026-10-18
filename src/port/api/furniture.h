#pragma once

#include <stdint.h>

#include "api.h"
#include "ac_furniture.h"
#include "m_name_table.h"
#include "port/events/Events.h"

#define C_FURNITURE_NAME_LEN 16

typedef struct C_FurnitureDef_s {
    const char* id;       // stable unique key, e.g. "mymod.lamp", keeps the same item numbers across sessions
    const char* name;     // shown in game, up to 16 characters
    uint8_t article;      // mIN_ARTICLE_*
    uint32_t price;       // in Bells
    uint8_t set_type;     // aFTR_SET_TYPE_*: normal, surface (other furniture can sit on it) or on surface
    uint8_t huusui_type;  // mHsRm_HUUSUI_*
    uint8_t has_face;     // counts as facing a direction for the Happy Room Academy
    uint8_t is_lucky;     // counts as a lucky item for the Happy Room Academy
    aFTR_PROFILE profile; // models, size and callbacks, same as the built-in furniture
} C_FurnitureDef;

// Registers a furniture item. Returns its south-facing item number (the other facings follow it), or EMPTY_NO.
BW_API mActor_name_t C_RegisterFurniture(const C_FurnitureDef* def);
// Unregisters a furniture item. Placed copies draw as the placeholder until it is registered again.
BW_API void C_UnregisterFurniture(mActor_name_t item);

static inline const C_FurnitureDef* C_FindFurniture(int index) {
    CALL_EVENT(FurnitureLookup, index, NULL);
    return FurnitureLookup_.furniture;
}
