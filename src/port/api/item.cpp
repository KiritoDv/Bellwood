#ifdef ENABLE_SCRIPTING

#include "port/ShipInit.hpp"
#include "port/api/StableIds.hpp"
#include "port/events/Events.h"

#define this this_
extern "C" {
#include "port/api/item.h"
}
#undef this

#include <array>
#include <string>

namespace {

constexpr int kCustomCount = C_ITEM_LAST - C_ITEM_FIRST + 1;

struct CustomItem {
    bool used = false;
    std::string id;
    std::string name;
    C_ItemDef def{};
};

std::array<CustomItem, kCustomCount> sItems;
StableIds sNumbers("custom_items.json", C_ITEM_FIRST, C_ITEM_LAST);

CustomItem* FindItem(int item) {
    const int slot = item - C_ITEM_FIRST;
    if (slot < 0 || slot >= kCustomCount || !sItems[slot].used) {
        return nullptr;
    }
    return &sItems[slot];
}

uint8_t CheckShape(uint8_t shape) {
    switch (shape) {
        case mNT_ITEM_TYPE_NONE:
        case mNT_ITEM_TYPE_CAGE:
        case mNT_ITEM_TYPE_LETTER:
        case mNT_ITEM_TYPE_OMIKUJI:
            return mNT_ITEM_TYPE_ETC;
        default:
            return shape < mNT_ITEM_TYPE_NUM ? shape : mNT_ITEM_TYPE_ETC;
    }
}

void InitItemApi() {
    REGISTER_LISTENER(ItemLookup, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        auto* lookup = reinterpret_cast<ItemLookup*>(event);
        if (CustomItem* item = FindItem(lookup->item)) {
            lookup->definition = &item->def;
        }
    });
    REGISTER_LISTENER(ItemUse, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        auto* use = reinterpret_cast<ItemUse*>(event);
        CustomItem* item = FindItem(use->item);
        if (item != nullptr && item->def.on_use != nullptr) {
            item->def.on_use(use->item, use->slot);
        }
    });
}

RegisterShipInitFunc sInitItemApi(InitItemApi);

} // namespace

extern "C" mActor_name_t C_RegisterItem(const C_ItemDef* def) {
    if (def == nullptr || def->id == nullptr || def->id[0] == '\0') {
        return EMPTY_NO;
    }
    const int number = sNumbers.Get(def->id);
    if (number < 0) {
        return EMPTY_NO;
    }
    CustomItem& item = sItems[number - C_ITEM_FIRST];
    item.used = true;
    item.id = def->id;
    item.name = def->name != nullptr ? std::string(def->name).substr(0, C_ITEM_NAME_LEN) : "";
    item.def = *def;
    item.def.id = item.id.c_str();
    item.def.name = item.name.c_str();
    item.def.shape = CheckShape(def->shape);
    return (mActor_name_t)number;
}

extern "C" void C_UnregisterItem(mActor_name_t item) {
    if (CustomItem* custom = FindItem(item)) {
        custom->used = false;
    }
}

#endif
