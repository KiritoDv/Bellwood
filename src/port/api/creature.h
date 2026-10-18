#pragma once

#include <stdint.h>

#include "api.h"
#include "ac_furniture.h"
#include "m_name_table.h"
#include "port/events/Events.h"

#define C_CREATURE_FIRST 0x2D
#define C_CREATURE_LAST 0xFF
#define C_CREATURE_NAME_LEN 16

typedef struct C_CreatureDef_s {
    const char* id;         // stable unique key, e.g. "mymod.goldfish", keeps the same item number across sessions
    const char* name;       // shown in game, up to 16 characters (UTF-8)
    uint8_t article;        // mIN_ARTICLE_*
    uint32_t price;         // in Bells
    uint8_t base;           // aGYO_TYPE_* or aINS_INSECT_TYPE_* it moves, sounds and is kept at home like
    uint8_t place;          // aSOG_SPAWN_AREA_* for fish, aSOI_SPAWN_AREA_* for insects
    uint8_t weight;         // how often it appears, on the same scale as the built-in ones (about 1 to 80)
    uint16_t months;        // bit 0 = January ... bit 11 = December, 0 for all year
    uint32_t hours;         // bit 0 = midnight ... bit 23 = 11 PM, 0 for all day
    uint8_t island;         // also appears on the island
    const char* catch_text; // shown when it is caught, NULL for "I caught <name>!"
    const void* icon_tex;   // 32x32 CI4 pocket icon, NULL for the base's
    const void* icon_pal;   // 16 colour palette for the icon
    Gfx* const* model;      // display lists replacing the base's model, in the same order, NULL for the base's
} C_CreatureDef;

// Registers a fish. Returns its item number, or EMPTY_NO.
#define C_RegisterFish(def) C_RegisterFishSized((def), sizeof(*(def)))
BW_API mActor_name_t C_RegisterFishSized(const C_CreatureDef* def, uint32_t size);
// Registers an insect. Returns its item number, or EMPTY_NO.
#define C_RegisterInsect(def) C_RegisterInsectSized((def), sizeof(*(def)))
BW_API mActor_name_t C_RegisterInsectSized(const C_CreatureDef* def, uint32_t size);
// Unregisters a fish. Caught copies show as unknown until it is registered again.
BW_API void C_UnregisterFish(mActor_name_t item);
// Unregisters an insect. Caught copies show as unknown until it is registered again.
BW_API void C_UnregisterInsect(mActor_name_t item);
// Who gave a fish or insect, built-in or custom, to the museum: 0 for nobody yet, 1 to 4 for a player of the town, 5
// for a player who was removed. -1 when the item isn't a fish or insect.
BW_API int C_GetMuseumDonator(mActor_name_t item);

static inline const C_CreatureDef* C_FindCreatureIndex(int index, int insect) {
    if (index < C_CREATURE_FIRST || index > C_CREATURE_LAST) {
        return NULL;
    }
    {
        CALL_EVENT(CreatureLookup, (uint8_t)(insect != 0), index, -1, NULL, -1, -1, -1);
        return CreatureLookup_.definition;
    }
}

static inline const C_CreatureDef* C_FindFish(int type) {
    return C_FindCreatureIndex(type, 0);
}

static inline const C_CreatureDef* C_FindInsect(int type) {
    return C_FindCreatureIndex(type, 1);
}

static inline const C_CreatureDef* C_FindCreature(mActor_name_t item) {
    if (ITEM_NAME_GET_TYPE(item) != NAME_TYPE_ITEM1) {
        return NULL;
    }
    if (ITEM_NAME_GET_CAT(item) == ITEM1_CAT_FISH) {
        return C_FindFish(ITEM_NAME_GET_INDEX(item));
    }
    if (ITEM_NAME_GET_CAT(item) == ITEM1_CAT_INSECT) {
        return C_FindInsect(ITEM_NAME_GET_INDEX(item));
    }
    return NULL;
}

static inline int C_FishBase(int type) {
    const C_CreatureDef* fish;

    if (type < C_CREATURE_FIRST) {
        return type;
    }
    fish = C_FindFish(type);
    return fish != NULL ? fish->base : 0;
}

static inline int C_InsectBase(int type) {
    const C_CreatureDef* insect;

    if (type < C_CREATURE_FIRST) {
        return type;
    }
    insect = C_FindInsect(type);
    return insect != NULL ? insect->base : 0;
}

static inline int C_CreatureCatchMessage(mActor_name_t item) {
    if (C_FindCreature(item) == NULL) {
        return -1;
    }
    {
        CALL_EVENT(CreatureLookup, (uint8_t)(ITEM_NAME_GET_CAT(item) == ITEM1_CAT_INSECT), ITEM_NAME_GET_INDEX(item),
                   -1, NULL, -1, -1, -1);
        return CreatureLookup_.catchMessage;
    }
}

// Furniture item number of a custom fish or insect's placed version, or EMPTY_NO.
static inline mActor_name_t C_CreatureFurniture(mActor_name_t item) {
    if (C_FindCreature(item) == NULL) {
        return EMPTY_NO;
    }
    {
        CALL_EVENT(CreatureLookup, (uint8_t)(ITEM_NAME_GET_CAT(item) == ITEM1_CAT_INSECT), ITEM_NAME_GET_INDEX(item),
                   -1, NULL, -1, -1, -1);
        if (CreatureLookup_.foundFurniture < 0) {
            return EMPTY_NO;
        }
        return (mActor_name_t)(FTR1_START + ((CreatureLookup_.foundFurniture - 0x400) << 2));
    }
}

// Custom fish or insect item number placed as furniture index ftr_idx, or EMPTY_NO.
static inline mActor_name_t C_FurnitureCreature(int ftr_idx) {
    if (ftr_idx < FTR_NUM) {
        return EMPTY_NO;
    }
    {
        CALL_EVENT(CreatureLookup, 0, -1, ftr_idx, NULL, -1, -1, -1);
        if (CreatureLookup_.definition == NULL) {
            return EMPTY_NO;
        }
        return (mActor_name_t)((CreatureLookup_.insect ? ITM_INSECT_START : ITM_FISH_START) +
                               CreatureLookup_.foundIndex);
    }
}
