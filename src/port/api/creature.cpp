#ifdef ENABLE_SCRIPTING

#include "port/ShipCompat.h"
#include "port/ShipInit.hpp"
#include "port/api/GameText.hpp"
#include "port/api/StableIds.hpp"
#include "port/events/Events.h"

#include <spdlog/spdlog.h>

#define this this_
extern "C" {
#include "port/api/creature.h"
#include "port/api/furniture.h"
#include "port/api/message.h"
#include "ac_my_room.h"
#include "ac_set_ovl_gyoei.h"
#include "ac_set_ovl_insect.h"
#include "m_common_data.h"
#include "m_huusui_room.h"
#include "m_item_name.h"
#include "m_room_type.h"
}
#undef this

#include <array>
#include <string>
#include <unordered_map>

namespace {

constexpr int kCount = C_CREATURE_LAST + 1 - C_CREATURE_FIRST;
constexpr int kBuiltInCount = 40;

struct CustomCreature {
    bool used = false;
    std::string id;
    std::string name;
    C_CreatureDef def{};
    mActor_name_t furnitureItem = EMPTY_NO;
    int furniture = -1;
    int32_t catchMessage = -1;
};

std::array<CustomCreature, kCount> sFish;
std::array<CustomCreature, kCount> sInsects;
std::unordered_map<int, std::pair<bool, int>> sByFurniture;
StableIds sFishIds("custom_fish.json", C_CREATURE_FIRST, C_CREATURE_LAST);
StableIds sInsectIds("custom_insects.json", C_CREATURE_FIRST, C_CREATURE_LAST);

std::array<CustomCreature, kCount>& Table(bool insect) {
    return insect ? sInsects : sFish;
}

CustomCreature* Find(int index, bool insect) {
    const int slot = index - C_CREATURE_FIRST;
    if (slot < 0 || slot >= kCount || !Table(insect)[slot].used) {
        return nullptr;
    }
    return &Table(insect)[slot];
}

std::string CatchText(const C_CreatureDef* def) {
    if (def->catch_text != nullptr) {
        return def->catch_text;
    }
    std::string article;
    switch (def->article) {
        case mIN_ARTICLE_A:
            article = "a ";
            break;
        case mIN_ARTICLE_AN:
            article = "an ";
            break;
        case mIN_ARTICLE_THE:
            article = "the ";
            break;
        case mIN_ARTICLE_SOME:
            article = "some ";
            break;
        default:
            break;
    }
    return "I caught " + article + (def->name != nullptr ? def->name : "") + "!";
}

int BuiltInFurniture(int base, bool insect) {
    return mRmTp_FtrItemNo2FtrIdx(insect ? FTR_INSECT_START : FTR_FISH_START) + base;
}

bool Active(const C_CreatureDef& def) {
    const lbRTC_time_c& now = Common_Get(time.rtc_time);
    if (def.weight == 0) {
        return false;
    }
    if (def.months != 0 && (def.months & (1 << (now.month - 1))) == 0) {
        return false;
    }
    return def.hours == 0 || (def.hours & (1u << now.hour)) != 0;
}

bool FishFits(const C_CreatureDef& def, int water) {
    switch (water) {
        case aSOG_RANGE_PROC_RIVER:
            return def.place == aSOG_SPAWN_AREA_RIVER || def.place == aSOG_SPAWN_AREA_POOL ||
                   def.place == aSOG_SPAWN_AREA_WATERFALL;
        case aSOG_RANGE_PROC_SEA:
            return def.place == aSOG_SPAWN_AREA_SEA || def.place == aSOG_SPAWN_AREA_RIVER_MOUTH;
        case aSOG_RANGE_PROC_OFFING:
            return def.place == aSOG_SPAWN_AREA_OFFING;
        case aSOG_RANGE_PROC_POOL:
            return def.place == aSOG_SPAWN_AREA_POND;
        case aSOG_RANGE_PROC_ISLAND:
            return def.island != 0;
        default:
            return false;
    }
}

void AddFish(FishSpawnList* list) {
    auto* entries = reinterpret_cast<aSOG_gyoei_spawn_info_weight_f_c*>(list->entries);
    for (int i = 0; i < kCount && *list->count < list->capacity; i++) {
        const CustomCreature& fish = sFish[i];
        if (!fish.used || !Active(fish.def) || !FishFits(fish.def, list->water)) {
            continue;
        }
        aSOG_gyoei_spawn_info_weight_f_c& entry = entries[(*list->count)++];
        entry.type = (s16)(C_CREATURE_FIRST + i);
        entry.spawn_area = fish.def.place;
        entry.spawn_weight = (f32)fish.def.weight;
    }
}

void AddInsects(InsectSpawnList* list) {
    auto* entries = reinterpret_cast<aSOI_insect_spawn_info_f_c*>(list->entries);
    for (int i = 0; i < kCount && *list->count < list->capacity; i++) {
        const CustomCreature& insect = sInsects[i];
        if (!insect.used || !Active(insect.def) || (list->island && !insect.def.island)) {
            continue;
        }
        aSOI_insect_spawn_info_f_c& entry = entries[(*list->count)++];
        entry.type = C_CREATURE_FIRST + i;
        entry.spawn_area = insect.def.place;
        entry.weight = (f32)insect.def.weight;
    }
}

void InitCreatureApi() {
    REGISTER_LISTENER(CreatureLookup, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        auto* lookup = reinterpret_cast<CreatureLookup*>(event);
        CustomCreature* creature = nullptr;
        int index = lookup->index;
        if (index >= 0) {
            creature = Find(index, lookup->insect != 0);
        } else if (auto it = sByFurniture.find(lookup->furniture); it != sByFurniture.end()) {
            lookup->insect = it->second.first;
            index = it->second.second;
            creature = Find(index, it->second.first);
        }
        if (creature != nullptr) {
            lookup->definition = &creature->def;
            lookup->foundIndex = index;
            lookup->foundFurniture = creature->furniture;
            lookup->catchMessage = creature->catchMessage;
        }
    });
    REGISTER_LISTENER(FishSpawnList, EVENT_PRIORITY_NORMAL,
                      [](IEvent* event) { AddFish(reinterpret_cast<FishSpawnList*>(event)); });
    REGISTER_LISTENER(InsectSpawnList, EVENT_PRIORITY_NORMAL,
                      [](IEvent* event) { AddInsects(reinterpret_cast<InsectSpawnList*>(event)); });
}

RegisterShipInitFunc sInitCreatureApi(InitCreatureApi);

void Unregister(mActor_name_t item, bool insect) {
    const int category = insect ? ITEM1_CAT_INSECT : ITEM1_CAT_FISH;
    if (ITEM_NAME_GET_TYPE(item) != NAME_TYPE_ITEM1 || ITEM_NAME_GET_CAT(item) != category) {
        return;
    }
    CustomCreature* creature = Find(ITEM_NAME_GET_INDEX(item), insect);
    if (creature == nullptr) {
        return;
    }
    creature->used = false;
    sByFurniture.erase(creature->furniture);
    if (creature->furnitureItem != EMPTY_NO) {
        C_UnregisterFurniture(creature->furnitureItem);
    }
    if (creature->catchMessage >= 0) {
        C_UnregisterMessage(creature->catchMessage);
    }
    creature->furnitureItem = EMPTY_NO;
    creature->furniture = -1;
    creature->catchMessage = -1;
}

mActor_name_t Register(const C_CreatureDef* def, bool insect) {
    if (def == nullptr || def->id == nullptr || def->id[0] == '\0') {
        return EMPTY_NO;
    }
    if (def->base >= kBuiltInCount) {
        SPDLOG_ERROR("Creature {}: base must be one of the 40 built-in {}", def->id, insect ? "insects" : "fish");
        return EMPTY_NO;
    }
    const int index = (insect ? sInsectIds : sFishIds).Get(def->id);
    if (index < 0) {
        return EMPTY_NO;
    }
    const mActor_name_t item = (mActor_name_t)((insect ? ITM_INSECT_START : ITM_FISH_START) + index);
    Unregister(item, insect);

    CustomCreature& creature = Table(insect)[index - C_CREATURE_FIRST];
    creature.id = def->id;
    creature.name = GameText_Encode(def->name != nullptr ? def->name : "", false);
    creature.name.resize(C_CREATURE_NAME_LEN, ' ');
    creature.def = *def;
    creature.def.id = creature.id.c_str();
    creature.def.name = creature.name.c_str();
    creature.def.catch_text = nullptr;
    creature.catchMessage = C_RegisterMessage(CatchText(def).c_str());

    u8 set_type = aFTR_SET_TYPE_NORMAL;
    if (aFTR_PROFILE* profile = aMR_GetBuiltInFurniture(BuiltInFurniture(def->base, insect), &set_type)) {
        const std::string furnitureId = creature.id + ".furniture";
        C_FurnitureDef furniture{};
        furniture.id = furnitureId.c_str();
        furniture.name = def->name;
        furniture.article = def->article;
        furniture.price = def->price;
        furniture.set_type = set_type;
        furniture.huusui_type = mHsRm_HUUSUI_NONE;
        furniture.profile = *profile;
        furniture.not_for_sale = TRUE;
        creature.furnitureItem = C_RegisterFurniture(&furniture);
        if (creature.furnitureItem != EMPTY_NO) {
            creature.furniture = mRmTp_FtrItemNo2FtrIdx(creature.furnitureItem);
            sByFurniture[creature.furniture] = { insect, index };
        }
    }
    creature.used = true;
    return item;
}

} // namespace

extern "C" mActor_name_t C_RegisterFish(const C_CreatureDef* def) {
    return Register(def, false);
}

extern "C" mActor_name_t C_RegisterInsect(const C_CreatureDef* def) {
    return Register(def, true);
}

extern "C" void C_UnregisterFish(mActor_name_t item) {
    Unregister(item, false);
}

extern "C" void C_UnregisterInsect(mActor_name_t item) {
    Unregister(item, true);
}

#endif
