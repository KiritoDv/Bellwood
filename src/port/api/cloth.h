#pragma once

#include <stdint.h>

#include "api.h"
#include "m_name_table.h"
#include "port/events/Events.h"

#define C_CLOTH_FIRST (ITM_DIARY_START + 0x10)
#define C_CLOTH_LAST (ITM_DIARY_START + 0xFF)
#define C_CLOTH_TEX_FIRST 264
#define C_CLOTH_NAME_LEN 16

typedef struct C_ClothDef_s {
    const char* id;       // stable unique key, e.g. "mymod.stripes", keeps the same item number across sessions
    const char* name;     // shown in game, up to 16 characters (UTF-8)
    uint8_t article;      // mIN_ARTICLE_*
    uint32_t price;       // in Bells
    const void* texture;  // 32x32 CI4 shirt texture, laid out like the built-in shirts
    const void* palette;  // 16 colour palette for the texture
    uint8_t not_for_sale; // shows "Not for Sale" in the catalog
} C_ClothDef;

// Registers a shirt. Returns its item number, or EMPTY_NO.
#define C_RegisterCloth(def) C_RegisterClothSized((def), sizeof(*(def)))
BW_API mActor_name_t C_RegisterClothSized(const C_ClothDef* def, uint32_t size);
// Unregisters a shirt. Copies in the pockets show as unknown until it is registered again.
BW_API void C_UnregisterCloth(mActor_name_t item);

static inline const C_ClothDef* C_FindCloth(mActor_name_t item) {
    if (item < C_CLOTH_FIRST || item > C_CLOTH_LAST) {
        return NULL;
    }
    {
        CALL_EVENT(ClothLookup, item, -1, NULL, EMPTY_NO, -1);
        return ClothLookup_.definition;
    }
}

static inline int C_IsCloth(mActor_name_t item) {
    return ITEM_IS_CLOTH(item) || C_FindCloth(item) != NULL;
}

static inline int C_IsNpcCloth(mActor_name_t item) {
    return ITEM_IS_NPC_CLOTH(item) || C_FindCloth(item) != NULL;
}

// Texture index used by the player, villagers and mannequins for a shirt item.
static inline int C_ClothTexIdx(mActor_name_t item) {
    if (ITEM_IS_CLOTH(item)) {
        return item - ITM_CLOTH_START;
    }
    if (C_FindCloth(item) != NULL) {
        return C_CLOTH_TEX_FIRST + (item - C_CLOTH_FIRST);
    }
    return 0;
}

// Item category, with custom shirts counting as clothing.
static inline int C_ItemCategory(mActor_name_t item) {
    return C_FindCloth(item) != NULL ? ITEM1_CAT_CLOTH : ITEM_NAME_GET_CAT(item);
}

// Mannequin furniture item number for a custom shirt, or EMPTY_NO.
static inline mActor_name_t C_ClothFurniture(mActor_name_t item) {
    if (item < C_CLOTH_FIRST || item > C_CLOTH_LAST) {
        return EMPTY_NO;
    }
    {
        CALL_EVENT(ClothLookup, item, -1, NULL, EMPTY_NO, -1);
        if (ClothLookup_.foundFurniture < 0) {
            return EMPTY_NO;
        }
        return (mActor_name_t)(FTR1_START + ((ClothLookup_.foundFurniture - 0x400) << 2));
    }
}

// Custom shirt shown by mannequin furniture index ftr_idx, or EMPTY_NO.
static inline mActor_name_t C_FurnitureCloth(int ftr_idx) {
    if (ftr_idx < FTR_NUM) {
        return EMPTY_NO;
    }
    {
        CALL_EVENT(ClothLookup, EMPTY_NO, ftr_idx, NULL, EMPTY_NO, -1);
        return ClothLookup_.definition != NULL ? ClothLookup_.foundItem : EMPTY_NO;
    }
}
