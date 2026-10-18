#include "port/save/SaveMods.h"
#include "port/save/SaveNames.gen.h"

#include "port/api/StableIds.hpp"
#include "port/ui/Notification.h"

#include <spdlog/spdlog.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <utility>

#define this this_
extern "C" {
#include "port/api/cloth.h"
#include "port/api/creature.h"
#include "port/api/furniture.h"
#include "port/api/interior.h"
#include "port/api/item.h"
#include "port/api/structure.h"
#include "port/api/villager.h"
#include "m_event.h"
#include "m_quest.h"
#include "m_room_type.h"
}
#undef this

namespace {

enum Category {
    kItems,
    kFurniture,
    kCarpets,
    kWallpapers,
    kClothes,
    kFish,
    kInsects,
    kVillagers,
    kStructures,
    kCategoryCount
};

struct CategoryInfo {
    const char* key;
    int first;
    int last;
};

const CategoryInfo kCategories[kCategoryCount] = {
    { "items", C_ITEM_FIRST, C_ITEM_LAST },
    { "furniture", FTR_NUM, mRmTp_FTR_INDEX_MAX - 1 },
    { "carpets", C_CARPET_FIRST, C_INTERIOR_LAST },
    { "wallpapers", C_WALLPAPER_FIRST, C_INTERIOR_LAST },
    { "clothes", C_CLOTH_FIRST, C_CLOTH_LAST },
    { "fish", C_CREATURE_FIRST, C_CREATURE_LAST },
    { "insects", C_CREATURE_FIRST, C_CREATURE_LAST },
    { "villagers", C_VILLAGER_FIRST, C_VILLAGER_LAST },
    { "structures", C_STRUCTURE_FIRST, C_STRUCTURE_LAST },
};

constexpr size_t kNameCount = 0x10000;

SaveJson sTownData = SaveJson::object();
SaveJson sTownIds = SaveJson::object();
SaveJson sTownMuseum = SaveJson::object();
std::map<std::string, std::set<std::string>> sCollections;

// Item names a save stores a number of the category as.
std::vector<mActor_name_t> NamesOf(int category, int number) {
    switch (category) {
        case kFurniture: {
            const int name = FTR1_START + ((number - 0x400) << 2);
            return { (mActor_name_t)name, (mActor_name_t)(name + 1), (mActor_name_t)(name + 2),
                     (mActor_name_t)(name + 3) };
        }
        case kCarpets:
            return { (mActor_name_t)(ITM_CARPET_START + number) };
        case kWallpapers:
            return { (mActor_name_t)(ITM_WALL_START + number) };
        case kFish:
            return { (mActor_name_t)(ITM_FISH_START + number) };
        case kInsects:
            return { (mActor_name_t)(ITM_INSECT_START + number) };
        case kVillagers:
            return { (mActor_name_t)number, (mActor_name_t)NPC_ID_TO_NPC_HOUSE_ID(number),
                     (mActor_name_t)(C_VILLAGER_DUMMY_FIRST + number - C_VILLAGER_FIRST) };
        default:
            return { (mActor_name_t)number };
    }
}

struct NameOwner {
    int category = -1;
    int number = 0;
};

const std::vector<NameOwner>& NameOwners() {
    static const std::vector<NameOwner> owners = [] {
        std::vector<NameOwner> table(kNameCount);
        for (int category = 0; category < kCategoryCount; category++) {
            for (int number = kCategories[category].first; number <= kCategories[category].last; number++) {
                for (mActor_name_t name : NamesOf(category, number)) {
                    table[name] = { category, number };
                }
            }
        }
        return table;
    }();
    return owners;
}

int UsedVillagerBit(int npc_id) {
    return npc_id & 0xFFF;
}

bool UsedVillager(const u8* table, int npc_id) {
    const int bit = UsedVillagerBit(npc_id);
    return bit / 8 < (int)sizeof(Save_t::npc_used_tbl) && (table[bit / 8] & (1 << (bit & 7))) != 0;
}

void SetUsedVillager(u8* table, int npc_id, bool used) {
    const int bit = UsedVillagerBit(npc_id);
    if (bit / 8 >= (int)sizeof(Save_t::npc_used_tbl)) {
        return;
    }
    if (used) {
        table[bit / 8] |= (u8)(1 << (bit & 7));
    } else {
        table[bit / 8] &= (u8) ~(1 << (bit & 7));
    }
}

template <typename V> void WalkIds(V& v, AnmPersonalID_c* ids, int count) {
    for (int i = 0; i < count; i++) {
        SaveVisitNames(v, ids[i]);
        v(ids[i]);
    }
}

template <typename V> void WalkNames(V& v, mActor_name_t* names, int count) {
    for (int i = 0; i < count; i++) {
        v(names[i]);
    }
}

template <typename V> void WalkUnions(V& v, Private_c& priv) {
    for (mQst_errand_c& errand : priv.errands) {
        if (errand.errand_type == mQst_ERRAND_TYPE_CHAIN) {
            WalkIds(v, errand.info.chain.used_ids, mQst_ERRAND_CHAIN_ANIMAL_NUM);
        } else if (errand.errand_type == mQst_ERRAND_TYPE_FIRST_JOB) {
            WalkIds(v, errand.info.first_job.used_ids, mQst_ERRAND_FIRST_JOB_ANIMAL_NUM);
        }
    }
}

template <typename V> void WalkUnions(V& v, Animal_c& animal) {
    mQst_contest_c& contest = animal.contest_quest;
    if (contest.base.quest_type == mQst_QUEST_TYPE_CONTEST && contest.base.quest_kind == mQst_CONTEST_KIND_LETTER) {
        v(contest.info.letter_data.present);
    }
}

template <typename V> void WalkUnions(V& v, mEv_special_c& special) {
    switch (special.kind) {
        case mEv_SPNPC_SHOP:
            WalkNames(v, special.event.bargin.items, mEv_BARGIN_ITEM_NUM);
            break;
        case mEv_SPNPC_DESIGNER:
            WalkNames(v, special.event.designer.gifted_cloths, mEv_DESGINER_NUM);
            break;
        case mEv_SPNPC_BROKER:
            WalkNames(v, special.event.broker.sold_items, mEv_BROKER_ITEM_NUM - 1);
            WalkNames(v, special.event.broker.items, mEv_BROKER_ITEM_NUM);
            break;
        case mEv_SPNPC_ARTIST:
            WalkNames(v, special.event.artist.walls, mEv_ARTIST_ENTRY_SAVE_NUM);
            break;
        case mEv_SPNPC_ARABIAN:
            v(special.event.arabian.carpet);
            break;
        default:
            break;
    }
}

template <typename V>
void Walk(V& v, Save_t* land, mCD_keep_mail_c* mail, const std::vector<mCD_foreigner_c*>& passports) {
    if (land != nullptr) {
        SaveVisitNames(v, *land);
        WalkNames(v, land->island.animal.anmuni.island_ftr, mNpc_ISLAND_FTR_SAVE_NUM);
        for (Private_c& priv : land->private_data) {
            WalkUnions(v, priv);
        }
        for (Animal_c& animal : land->animals) {
            WalkUnions(v, animal);
        }
        WalkUnions(v, land->island.animal);
        WalkUnions(v, land->event_save_data.special);
    }
    if (mail != nullptr) {
        SaveVisitNames(v, *mail);
    }
    for (mCD_foreigner_c* passport : passports) {
        SaveVisitNames(v, *passport);
        WalkUnions(v, passport->priv);
        WalkUnions(v, passport->remove_animal);
    }
}

struct Collector {
    std::set<std::pair<int, int>>& used;

    void operator()(mActor_name_t& name) {
        const NameOwner& owner = NameOwners()[name];
        if (owner.category >= 0) {
            used.insert({ owner.category, owner.number });
        }
    }

    void operator()(AnmPersonalID_c&) {
    }

    void operator()(mPr_cloth_c&) {
    }

    void operator()(mHm_wf_c& wall_floor) {
        if (wall_floor.flooring_idx >= C_CARPET_FIRST) {
            used.insert({ kCarpets, wall_floor.flooring_idx });
        }
        if (wall_floor.wallpaper_idx >= C_WALLPAPER_FIRST) {
            used.insert({ kWallpapers, wall_floor.wallpaper_idx });
        }
    }
};

struct Remapper {
    const std::vector<mActor_name_t>& names;
    const std::array<u8, 256>& floors;
    const std::array<u8, 256>& walls;
    const std::map<int, int>& villagers;

    void operator()(mActor_name_t& name) {
        name = names[name];
    }

    void operator()(AnmPersonalID_c& id) {
        if (!C_IsCustomVillager(id.npc_id)) {
            return;
        }
        if (auto it = villagers.find(NPC_START | id.name_id); it != villagers.end()) {
            id.name_id = (u8)it->second;
        }
    }

    void operator()(mPr_cloth_c& cloth) {
        if (cloth.item >= C_CLOTH_FIRST && cloth.item <= C_CLOTH_LAST) {
            cloth.idx = (u16)(C_CLOTH_TEX_FIRST + cloth.item - C_CLOTH_FIRST);
        }
    }

    void operator()(mHm_wf_c& wall_floor) {
        wall_floor.flooring_idx = floors[wall_floor.flooring_idx];
        wall_floor.wallpaper_idx = walls[wall_floor.wallpaper_idx];
    }
};

const SaveJson* FindMods(const SaveJson& ship, const char* key) {
    auto mods = ship.find("mods");
    if (mods == ship.end() || !mods->is_object()) {
        return nullptr;
    }
    auto value = mods->find(key);
    return value != mods->end() && value->is_object() ? &*value : nullptr;
}

SaveJson& ModsObject(SaveJson& ship) {
    if (!ship.contains("mods") || !ship["mods"].is_object()) {
        ship["mods"] = SaveJson::object();
    }
    return ship["mods"];
}

void PutMods(SaveJson& ship, const char* key, SaveJson value) {
    SaveJson& mods = ModsObject(ship);
    if (value.empty()) {
        mods.erase(key);
    } else {
        mods[key] = std::move(value);
    }
    if (mods.empty()) {
        ship.erase("mods");
    }
}

} // namespace

bool SaveMods::Remap(const SaveJson& ship, Save_t* land, mCD_keep_mail_c* mail,
                     const std::vector<mCD_foreigner_c*>& passports) {
    const SaveJson* ids = FindMods(ship, "ids");
    if (ids == nullptr) {
        return false;
    }
    std::array<std::map<int, int>, kCategoryCount> moved;
    bool any = false;
    for (int category = 0; category < kCategoryCount; category++) {
        const CategoryInfo& info = kCategories[category];
        StableIds* table = StableIds::ForKey(info.key);
        auto entries = ids->find(info.key);
        if (table == nullptr || entries == ids->end() || !entries->is_object()) {
            continue;
        }
        for (const auto& [id, entry] : entries->items()) {
            if (!entry.is_object()) {
                continue;
            }
            const int saved = entry.value("number", -1);
            if (saved < info.first || saved > info.last) {
                continue;
            }
            const int number = table->Get(id, saved, entry.value("mod", std::string()));
            if (number < 0) {
                SPDLOG_ERROR("save: no {} number left for {}, it keeps {}", info.key, id, saved);
            } else if (number != saved) {
                moved[category][saved] = number;
                any = true;
            }
        }
    }
    if (!any) {
        return false;
    }

    std::vector<mActor_name_t> names(kNameCount);
    for (size_t name = 0; name < kNameCount; name++) {
        names[name] = (mActor_name_t)name;
    }
    std::array<u8, 256> floors;
    std::array<u8, 256> walls;
    for (int index = 0; index < 256; index++) {
        floors[index] = (u8)index;
        walls[index] = (u8)index;
    }
    for (int category = 0; category < kCategoryCount; category++) {
        for (const auto& [from, to] : moved[category]) {
            const auto before = NamesOf(category, from);
            const auto after = NamesOf(category, to);
            for (size_t i = 0; i < before.size(); i++) {
                names[before[i]] = after[i];
            }
            if (category == kCarpets) {
                floors[from] = (u8)to;
            } else if (category == kWallpapers) {
                walls[from] = (u8)to;
            }
        }
    }

    Remapper remapper{ names, floors, walls, moved[kVillagers] };
    Walk(remapper, land, mail, passports);
    if (land != nullptr) {
        u8 used[sizeof(land->npc_used_tbl)];
        memcpy(used, land->npc_used_tbl, sizeof(used));
        for (const auto& [from, to] : moved[kVillagers]) {
            SetUsedVillager(land->npc_used_tbl, from, false);
        }
        for (const auto& [from, to] : moved[kVillagers]) {
            if (UsedVillager(used, from)) {
                SetUsedVillager(land->npc_used_tbl, to, true);
            }
        }
    }
    SPDLOG_INFO("save: renumbered mod content saved by another install");
    return true;
}

void SaveMods::WriteIds(SaveJson& ship, const Save_t* land, const mCD_keep_mail_c* mail,
                        const std::vector<const mCD_foreigner_c*>& passports) {
    if (StableIds::All().empty()) {
        return;
    }
    std::set<std::pair<int, int>> used;
    Collector collector{ used };
    std::vector<mCD_foreigner_c*> foreigners;
    for (const mCD_foreigner_c* passport : passports) {
        foreigners.push_back(const_cast<mCD_foreigner_c*>(passport));
    }
    Walk(collector, const_cast<Save_t*>(land), const_cast<mCD_keep_mail_c*>(mail), foreigners);
    if (land != nullptr) {
        for (int npc_id = C_VILLAGER_FIRST; npc_id <= C_VILLAGER_LAST; npc_id++) {
            if (UsedVillager(land->npc_used_tbl, npc_id)) {
                used.insert({ kVillagers, npc_id });
            }
        }
    }

    SaveJson ids = SaveJson::object();
    for (const auto& [category, number] : used) {
        StableIds* table = StableIds::ForKey(kCategories[category].key);
        const std::string* id = table != nullptr ? table->IdOf(number) : nullptr;
        if (id != nullptr) {
            ids[kCategories[category].key][*id] = { { "number", number }, { "mod", table->Owner(*id) } };
        }
    }
    PutMods(ship, "ids", std::move(ids));
}

void SaveMods::ReadCollections(const SaveJson& ship) {
    const SaveJson* collection = FindMods(ship, "collection");
    if (collection == nullptr) {
        return;
    }
    for (const auto& [player, ids] : collection->items()) {
        if (!ids.is_array()) {
            continue;
        }
        for (const auto& id : ids) {
            if (id.is_string()) {
                sCollections[player].insert(id.get<std::string>());
            }
        }
    }
}

void SaveMods::WriteCollections(SaveJson& ship, const Save_t* land,
                                const std::vector<const mCD_foreigner_c*>& passports) {
    SaveJson collection = SaveJson::object();
    const auto add = [&](const Private_c& priv) {
        if (!priv.exists) {
            return;
        }
        const std::string key = PlayerKey(priv.player_ID);
        auto it = sCollections.find(key);
        if (it != sCollections.end() && !it->second.empty()) {
            collection[key] = it->second;
        }
    };
    if (land != nullptr) {
        for (const Private_c& priv : land->private_data) {
            add(priv);
        }
    }
    for (const mCD_foreigner_c* passport : passports) {
        add(passport->priv);
    }
    PutMods(ship, "collection", std::move(collection));
}

std::map<std::string, std::set<std::string>>& SaveMods::Collections() {
    return sCollections;
}

std::string SaveMods::PlayerKey(const PersonalID_c& pid) {
    char key[16];
    snprintf(key, sizeof(key), "%04X-%04X", pid.land_id, pid.player_id);
    return key;
}

void SaveMods::LoadTown(const SaveJson& ship) {
    const SaveJson* data = FindMods(ship, "data");
    const SaveJson* ids = FindMods(ship, "ids");
    const SaveJson* museum = FindMods(ship, "museum");
    sTownData = data != nullptr ? *data : SaveJson::object();
    sTownIds = ids != nullptr ? *ids : SaveJson::object();
    sTownMuseum = museum != nullptr ? *museum : SaveJson::object();
}

void SaveMods::CheckMissing() {
    std::map<std::string, std::vector<std::string>> missing;
    for (const CategoryInfo& info : kCategories) {
        StableIds* table = StableIds::ForKey(info.key);
        auto entries = sTownIds.find(info.key);
        if (table == nullptr || entries == sTownIds.end() || !entries->is_object()) {
            continue;
        }
        for (const auto& [id, entry] : entries->items()) {
            const int number = table->Get(id);
            if (number >= 0 && table->IsActive(number)) {
                continue;
            }
            std::string mod = entry.is_object() ? entry.value("mod", std::string()) : std::string();
            missing[mod.empty() ? table->Owner(id) : mod].push_back(id);
        }
    }
    if (missing.empty()) {
        return;
    }
    std::string mods;
    for (const auto& [mod, ids] : missing) {
        std::string list;
        for (const std::string& id : ids) {
            list += (list.empty() ? "" : ", ") + id;
        }
        SPDLOG_WARN("save: this town has content from {} that isn't registered: {}", mod.empty() ? "unknown mods" : mod,
                    list);
        mods += (mods.empty() ? "" : ", ") + (mod.empty() ? std::string("unknown mods") : mod);
    }
    Notification::Emit({ .prefix = "Missing mods:",
                         .message = mods,
                         .messageColor = ImVec4(1.0f, 0.5f, 0.5f, 1.0f),
                         .suffix = "Their items show as unknown until they are installed again.",
                         .suffixColor = ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
                         .remainingTime = 12.0f });
}

void SaveMods::StoreTown(SaveJson& ship) {
    PutMods(ship, "data", sTownData);
    PutMods(ship, "museum", sTownMuseum);
}

SaveJson& SaveMods::TownData() {
    return sTownData;
}

SaveJson& SaveMods::Museum() {
    return sTownMuseum;
}
