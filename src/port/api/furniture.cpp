#ifdef ENABLE_SCRIPTING

#include "port/ShipInit.hpp"
#include "port/events/Events.h"

#define this this_
extern "C" {
#include "port/api/furniture.h"
#include "m_room_type.h"
}
#undef this

#include <ship/Context.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <array>
#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace {

constexpr int kFirstIndex = FTR_NUM;
constexpr int kCustomCount = mRmTp_FTR_INDEX_MAX - FTR_NUM;
constexpr const char* kIdsFile = "custom_furniture.json";

struct CustomFurniture {
    bool used = false;
    std::string id;
    std::string name;
    C_FurnitureDef def{};
};

std::array<CustomFurniture, kCustomCount> sFurniture;
std::unordered_map<std::string, int> sIndices;
bool sIndicesLoaded = false;

std::string IndicesPath() {
    return Ship::Context::GetPathRelativeToAppDirectory(kIdsFile);
}

void LoadIndices() {
    if (sIndicesLoaded) {
        return;
    }
    sIndicesLoaded = true;
    std::ifstream in(IndicesPath());
    if (!in) {
        return;
    }
    const auto json = nlohmann::json::parse(in, nullptr, false);
    if (!json.is_object()) {
        SPDLOG_WARN("{} is not valid, custom furniture will get new item numbers", kIdsFile);
        return;
    }
    for (const auto& [id, index] : json.items()) {
        if (index.is_number_integer() && index.get<int>() >= kFirstIndex && index.get<int>() < mRmTp_FTR_INDEX_MAX) {
            sIndices[id] = index.get<int>();
        }
    }
}

void SaveIndices() {
    nlohmann::json json = nlohmann::json::object();
    for (const auto& [id, index] : sIndices) {
        json[id] = index;
    }
    std::ofstream out(IndicesPath(), std::ios::trunc);
    out << json.dump(2);
}

int IndexForId(const std::string& id) {
    LoadIndices();
    if (auto it = sIndices.find(id); it != sIndices.end()) {
        return it->second;
    }
    std::unordered_set<int> taken;
    for (const auto& [key, index] : sIndices) {
        taken.insert(index);
    }
    for (int index = kFirstIndex; index < mRmTp_FTR_INDEX_MAX; index++) {
        if (!taken.contains(index)) {
            sIndices[id] = index;
            SaveIndices();
            return index;
        }
    }
    return -1;
}

mActor_name_t IndexToItem(int index) {
    return (mActor_name_t)(FTR1_START + ((index - 0x400) << 2));
}

int ItemToIndex(mActor_name_t item) {
    if (ITEM_NAME_GET_TYPE(item) != NAME_TYPE_FTR1) {
        return -1;
    }
    return 0x400 + ((item - FTR1_START) >> 2);
}

void InitFurnitureApi() {
    REGISTER_LISTENER(FurnitureLookup, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        auto* lookup = reinterpret_cast<FurnitureLookup*>(event);
        const int slot = lookup->index - kFirstIndex;
        if (slot >= 0 && slot < kCustomCount && sFurniture[slot].used) {
            lookup->furniture = &sFurniture[slot].def;
        }
    });
}

RegisterShipInitFunc sInitFurnitureApi(InitFurnitureApi);

} // namespace

extern "C" mActor_name_t C_RegisterFurniture(const C_FurnitureDef* def) {
    if (def == nullptr || def->id == nullptr || def->id[0] == '\0') {
        return EMPTY_NO;
    }
    const int index = IndexForId(def->id);
    if (index < 0) {
        SPDLOG_ERROR("No furniture numbers left for {}", def->id);
        return EMPTY_NO;
    }
    CustomFurniture& furniture = sFurniture[index - kFirstIndex];
    if (furniture.used && furniture.id != def->id) {
        return EMPTY_NO;
    }
    furniture.used = true;
    furniture.id = def->id;
    furniture.name = def->name != nullptr ? std::string(def->name).substr(0, C_FURNITURE_NAME_LEN) : "";
    furniture.def = *def;
    furniture.def.id = furniture.id.c_str();
    furniture.def.name = furniture.name.c_str();
    return IndexToItem(index);
}

extern "C" void C_UnregisterFurniture(mActor_name_t item) {
    const int slot = ItemToIndex(item) - kFirstIndex;
    if (slot >= 0 && slot < kCustomCount) {
        sFurniture[slot].used = false;
    }
}

#endif
