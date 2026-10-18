#pragma once

#include <stdint.h>

#include "api.h"
#include "m_name_table.h"
#include "port/events/Events.h"

#define C_ITEM_FIRST (ITM_ETC_START + ETC_NUM)
#define C_ITEM_LAST (ITM_ETC_START + 0xFF)
#define C_ITEM_NAME_LEN 16

typedef struct C_ItemDef_s {
    const char* id;       // stable unique key, e.g. "mymod.gem", keeps the same item number across sessions
    const char* name;     // shown in game, up to 16 characters (UTF-8)
    uint8_t article;      // mIN_ARTICLE_*
    uint32_t price;       // in Bells, 0 means Nook takes it for free
    uint8_t shape;        // mNT_ITEM_TYPE_*: model used on the ground and in the player's hand
    const void* icon_tex; // 32x32 CI4 inventory icon, NULL for the default one
    const void* icon_pal; // 16 colour palette for the icon
    void (*on_use)(mActor_name_t item, int slot); // adds "Use" to the pocket menu
    uint8_t not_for_sale; // shows "Not for Sale" in the catalog
    const void* umbrella_tex; // 32x32 CI4 canopy laid out like an Able Sisters umbrella design: the item is an umbrella
                              // the player can carry. NULL for an ordinary item
    const void* umbrella_pal; // 16 colour palette for the canopy
} C_ItemDef;

// Registers an item. Returns its item number, or EMPTY_NO.
#define C_RegisterItem(def) C_RegisterItemSized((def), sizeof(*(def)))
BW_API mActor_name_t C_RegisterItemSized(const C_ItemDef* def, uint32_t size);
// Unregisters an item. Copies in the pockets or on the ground show as unknown until it is registered again.
BW_API void C_UnregisterItem(mActor_name_t item);

static inline const C_ItemDef* C_FindItem(mActor_name_t item);

// TRUE for a custom item that is an umbrella.
static inline int C_IsCustomUmbrella(mActor_name_t item) {
    const C_ItemDef* def = C_FindItem(item);
    return def != NULL && def->umbrella_tex != NULL;
}

static inline const C_ItemDef* C_FindItem(mActor_name_t item) {
    if (item < C_ITEM_FIRST || item > C_ITEM_LAST) {
        return NULL;
    }
    {
        CALL_EVENT(ItemLookup, item, NULL);
        return ItemLookup_.definition;
    }
}
