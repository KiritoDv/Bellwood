#ifdef ENABLE_SCRIPTING

#include "port/ShipInit.hpp"
#include "port/api/GameText.hpp"
#include "port/api/ModOwner.hpp"
#include "port/api/SizedDef.hpp"
#include "port/api/StableIds.hpp"
#include "port/api/TextureData.hpp"
#include "port/events/Events.h"

#define this this_
extern "C" {
#include "port/api/item.h"
}
#undef this

#include <array>
#include <string>
#include <vector>

namespace {

constexpr int kCustomCount = C_ITEM_LAST - C_ITEM_FIRST + 1;

struct CustomItem {
    bool used = false;
    std::string id;
    std::string name;
    std::string owner;
    C_ItemDef def{};
    std::vector<uint8_t> umbrellaTex;
    std::vector<uint8_t> umbrellaPal;
};

std::array<CustomItem, kCustomCount> sItems;
StableIds sNumbers("items", C_ITEM_FIRST, C_ITEM_LAST);

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
            ModOwner::Scope scope(item->owner);
            item->def.on_use(use->item, use->slot);
        }
    });
    ModOwner::AddReleaser([](const std::string& mod) {
        for (int slot = 0; slot < kCustomCount; slot++) {
            if (sItems[slot].used && sItems[slot].owner == mod) {
                C_UnregisterItem((mActor_name_t)(C_ITEM_FIRST + slot));
            }
        }
    });
}

RegisterShipInitFunc sInitItemApi(InitItemApi);

} // namespace

extern "C" mActor_name_t C_RegisterItemSized(const C_ItemDef* item_def, uint32_t size) {
    const SizedDef<C_ItemDef> sized(item_def, size);
    const C_ItemDef* def = sized.get();
    if (def == nullptr || def->id == nullptr || def->id[0] == '\0') {
        return EMPTY_NO;
    }
    std::vector<uint8_t> umbrellaTex;
    std::vector<uint8_t> umbrellaPal;
    if (def->umbrella_tex != nullptr && def->umbrella_pal != nullptr) {
        umbrellaTex.resize(32 * 32 / 2);
        umbrellaPal.resize(TextureData::kPaletteSize);
        if (!TextureData::ReadCI4(def->umbrella_tex, 32, 32, umbrellaTex.data()) ||
            !TextureData::ReadPalette16(def->umbrella_pal, umbrellaPal.data())) {
            SPDLOG_ERROR("Item {}: could not load the umbrella texture", def->id);
            return EMPTY_NO;
        }
    }
    const std::string owner = ModOwner::Current();
    const int number = sNumbers.Register(def->id, owner);
    if (number < 0) {
        return EMPTY_NO;
    }
    CustomItem& item = sItems[number - C_ITEM_FIRST];
    item.used = true;
    item.id = def->id;
    item.owner = owner;
    item.name = GameText_Encode(def->name != nullptr ? def->name : "", false);
    item.name.resize(C_ITEM_NAME_LEN, ' ');
    item.def = *def;
    item.def.id = item.id.c_str();
    item.def.name = item.name.c_str();
    item.def.shape = CheckShape(def->shape);
    item.umbrellaTex = std::move(umbrellaTex);
    item.umbrellaPal = std::move(umbrellaPal);
    item.def.umbrella_tex = item.umbrellaTex.empty() ? nullptr : item.umbrellaTex.data();
    item.def.umbrella_pal = item.umbrellaPal.empty() ? nullptr : item.umbrellaPal.data();
    return (mActor_name_t)number;
}

extern "C" void C_UnregisterItem(mActor_name_t item) {
    if (CustomItem* custom = FindItem(item)) {
        custom->used = false;
        sNumbers.Unregister(item);
    }
}

#endif
