#ifdef ENABLE_SCRIPTING

#include "port/ShipCompat.h"
#include "port/ShipInit.hpp"
#include "port/api/ModOwner.hpp"
#include "port/save/SaveMods.h"
#include "port/events/Events.h"

#define this this_
extern "C" {
#include "port/api/shop.h"
#include "port/api/actor.h"
#include "port/api/furniture.h"
#include "port/api/interior.h"
#include "port/api/cloth.h"
#include "port/api/item.h"
#include "m_catalog_ovl.h"
#include "m_common_data.h"
#include "m_private.h"
#include "m_room_type.h"
#include "m_scene_table.h"
#include "m_shop.h"
#include "sys_math.h"
}
#undef this

#include <ship/Context.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr const char* kCollectionFile = "custom_collection.json";

struct ShopStock {
    mActor_name_t item;
    int chance;
    std::string owner;
};

std::vector<ShopStock> sStock;
bool sLegacyCollectionRead = false;

mActor_name_t SouthFacing(mActor_name_t item) {
    return mRmTp_FtrIdx2FtrItemNo(mRmTp_FtrItemNo2FtrIdx(item), mRmTp_DIRECT_SOUTH);
}

const char* CustomId(mActor_name_t item) {
    if (ITEM_IS_FTR(item)) {
        const int index = mRmTp_FtrItemNo2FtrIdx(item);
        if (index < FTR_NUM) {
            return nullptr;
        }
        const C_FurnitureDef* furniture = C_FindFurniture(index);
        return furniture != nullptr ? furniture->id : nullptr;
    }
    if (const C_InteriorDef* interior = C_FindInterior(item)) {
        return interior->id;
    }
    if (const C_ClothDef* cloth = C_FindCloth(item)) {
        return cloth->id;
    }
    const C_ItemDef* custom = C_FindItem(item);
    return custom != nullptr ? custom->id : nullptr;
}

int StockKind(mActor_name_t item) {
    if (ITEM_IS_FTR(item)) {
        return -1;
    }
    if (C_IsCloth(item)) {
        return ITEM1_CAT_CLOTH;
    }
    if (ITEM_NAME_GET_TYPE(item) == NAME_TYPE_ITEM1 &&
        (ITEM_NAME_GET_CAT(item) == ITEM1_CAT_CARPET || ITEM_NAME_GET_CAT(item) == ITEM1_CAT_WALL)) {
        return ITEM_NAME_GET_CAT(item);
    }
    return -2;
}

bool IsCustom(mActor_name_t item) {
    if (ITEM_IS_FTR(item)) {
        return mRmTp_FtrItemNo2FtrIdx(item) >= FTR_NUM;
    }
    if (item >= C_CLOTH_FIRST && item <= C_CLOTH_LAST) {
        return true;
    }
    return ITEM_NAME_GET_INDEX(item) >=
           (ITEM_NAME_GET_CAT(item) == ITEM1_CAT_CARPET ? C_CARPET_FIRST : C_WALLPAPER_FIRST);
}

bool HasPlayer() {
    Private_c* priv = Common_Get(now_private);
    return priv != nullptr && priv->exists;
}

std::string PlayerKey() {
    return HasPlayer() ? SaveMods::PlayerKey(Common_Get(now_private)->player_ID) : std::string();
}

void ReadLegacyCollection() {
    if (sLegacyCollectionRead) {
        return;
    }
    sLegacyCollectionRead = true;
    std::ifstream in(Ship::Context::GetPathRelativeToAppDirectory(kCollectionFile));
    if (!in) {
        return;
    }
    const auto json = nlohmann::json::parse(in, nullptr, false);
    if (!json.is_object()) {
        return;
    }
    for (const auto& [player, ids] : json.items()) {
        if (!ids.is_array()) {
            continue;
        }
        for (const auto& id : ids) {
            if (id.is_string()) {
                SaveMods::Collections()[player].insert(id.get<std::string>());
            }
        }
    }
}

const std::set<std::string>* PlayerCollection() {
    const std::string key = PlayerKey();
    if (key.empty()) {
        return nullptr;
    }
    ReadLegacyCollection();
    auto it = SaveMods::Collections().find(key);
    return it != SaveMods::Collections().end() ? &it->second : nullptr;
}

void Collect(mActor_name_t item) {
    const char* id = CustomId(item);
    const std::string key = PlayerKey();
    if (id == nullptr || key.empty()) {
        return;
    }
    ReadLegacyCollection();
    SaveMods::Collections()[key].insert(id);
}

std::vector<int> StockSlots(const ShopStockGenerated* stock, int kind) {
    std::vector<int> slots;
    if (kind == -1) {
        for (int i = 0; i < stock->furnitureCount; i++) {
            slots.push_back(i);
        }
        return slots;
    }
    for (int i = stock->furnitureCount; i < stock->count; i++) {
        if (StockKind(stock->items[i]) == kind) {
            slots.push_back(i);
        }
    }
    return slots;
}

void FillStock(ShopStockGenerated* stock) {
    std::map<int, std::vector<int>> free;
    std::vector<ShopStock> entries = sStock;
    for (size_t i = entries.size(); i > 1; i--) {
        std::swap(entries[i - 1], entries[RANDOM((int)i)]);
    }
    for (const ShopStock& entry : entries) {
        const int kind = StockKind(entry.item);
        if (!free.contains(kind)) {
            free[kind] = StockSlots(stock, kind);
        }
        std::vector<int>& slots = free[kind];
        if (slots.empty()) {
            continue;
        }
        if (RANDOM(100) >= entry.chance) {
            continue;
        }
        if (IsCustom(entry.item) && CustomId(entry.item) == nullptr) {
            continue;
        }
        if (std::find(stock->items, stock->items + stock->count, entry.item) != stock->items + stock->count ||
            (stock->rareItem != nullptr && *stock->rareItem == entry.item)) {
            continue;
        }
        const int pick = RANDOM((int)slots.size());
        stock->items[slots[pick]] = entry.item;
        slots.erase(slots.begin() + pick);
    }
}

void AddCatalogEntry(CatalogPageBuild* page, mActor_name_t item) {
    if (*page->count < page->capacity) {
        page->items[(*page->count)++] = item;
    }
}

void FillCatalogPage(CatalogPageBuild* page) {
    if (page->page != mCL_MENU_FTR && page->page != mCL_MENU_UMBRELLA && page->page != mCL_MENU_CARPET &&
        page->page != mCL_MENU_WALL && page->page != mCL_MENU_CLOTH) {
        return;
    }
    const std::set<std::string>* collected = PlayerCollection();
    if (collected == nullptr || collected->empty()) {
        return;
    }
    if (page->page == mCL_MENU_FTR) {
        for (int index = FTR_NUM; index < mRmTp_FTR_INDEX_MAX; index++) {
            const C_FurnitureDef* furniture = C_FindFurniture(index);
            if (furniture != nullptr && collected->contains(furniture->id)) {
                AddCatalogEntry(page, mRmTp_FtrIdx2FtrItemNo(index, mRmTp_DIRECT_SOUTH));
            }
        }
    } else if (page->page == mCL_MENU_CLOTH) {
        for (int item = C_CLOTH_FIRST; item <= C_CLOTH_LAST; item++) {
            const C_ClothDef* cloth = C_FindCloth((mActor_name_t)item);
            if (cloth != nullptr && collected->contains(cloth->id)) {
                AddCatalogEntry(page, C_ClothFurniture((mActor_name_t)item));
            }
        }
    } else if (page->page == mCL_MENU_CARPET || page->page == mCL_MENU_WALL) {
        const bool wall = page->page == mCL_MENU_WALL;
        for (int index = wall ? C_WALLPAPER_FIRST : C_CARPET_FIRST; index <= C_INTERIOR_LAST; index++) {
            const C_InteriorDef* interior = C_FindInteriorIndex(index, wall);
            if (interior != nullptr && collected->contains(interior->id)) {
                AddCatalogEntry(page, (mActor_name_t)((wall ? ITM_WALL_START : ITM_CARPET_START) + index));
            }
        }
    } else {
        for (int item = C_ITEM_FIRST; item <= C_ITEM_LAST; item++) {
            const C_ItemDef* custom = C_FindItem((mActor_name_t)item);
            if (custom != nullptr && collected->contains(custom->id)) {
                AddCatalogEntry(page, (mActor_name_t)item);
            }
        }
    }
}

void InitShopApi() {
    REGISTER_LISTENER(ShopStockGenerated, EVENT_PRIORITY_NORMAL,
                      [](IEvent* event) { FillStock(reinterpret_cast<ShopStockGenerated*>(event)); });
    REGISTER_LISTENER(CatalogPageBuild, EVENT_PRIORITY_NORMAL,
                      [](IEvent* event) { FillCatalogPage(reinterpret_cast<CatalogPageBuild*>(event)); });
    REGISTER_LISTENER(ItemCollected, EVENT_PRIORITY_NORMAL,
                      [](IEvent* event) { Collect(reinterpret_cast<ItemCollected*>(event)->item); });
    ModOwner::AddReleaser([](const std::string& mod) {
        std::erase_if(sStock, [&](const ShopStock& entry) { return entry.owner == mod; });
    });
}

RegisterShipInitFunc sInitShopApi(InitShopApi);

} // namespace

extern "C" int C_AddShopStock(mActor_name_t item, int chance) {
    if (StockKind(item) == -2) {
        return FALSE;
    }
    if (ITEM_IS_FTR(item)) {
        item = SouthFacing(item);
    }
    chance = std::clamp(chance, 0, 100);
    for (ShopStock& entry : sStock) {
        if (entry.item == item) {
            entry.chance = chance;
            return TRUE;
        }
    }
    sStock.push_back({ item, chance, ModOwner::Current() });
    return TRUE;
}

extern "C" void C_RemoveShopStock(mActor_name_t item) {
    if (ITEM_IS_FTR(item)) {
        item = SouthFacing(item);
    }
    std::erase_if(sStock, [item](const ShopStock& entry) { return entry.item == item; });
}

extern "C" int C_RestockShop(void) {
    if (C_GetPlay() == nullptr || !HasPlayer()) {
        return FALSE;
    }
    switch (Save_Get(scene_no)) {
        case SCENE_SHOP0:
        case SCENE_CONVENI:
        case SCENE_SUPER:
        case SCENE_DEPART:
        case SCENE_DEPART_2:
            return FALSE;
        default:
            break;
    }
    mSP_ExchangeLineUp_ZeldaMalloc();
    return TRUE;
}

extern "C" void C_AddToCatalog(mActor_name_t item) {
    if (HasPlayer()) {
        mPr_SetItemCollectBit(item);
    }
}

extern "C" int C_IsInCatalog(mActor_name_t item) {
    if (!HasPlayer()) {
        return FALSE;
    }
    if (const char* id = CustomId(item)) {
        const std::set<std::string>* collected = PlayerCollection();
        return collected != nullptr && collected->contains(id);
    }
    return mSP_CollectCheck(item);
}

#endif
