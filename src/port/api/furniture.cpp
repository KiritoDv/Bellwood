#ifdef ENABLE_SCRIPTING

#include "port/ShipInit.hpp"
#include "port/api/StableIds.hpp"
#include "port/events/Events.h"

#define this this_
extern "C" {
#include "port/api/furniture.h"
#include "m_room_type.h"
}
#undef this

#include <array>
#include <string>

namespace {

constexpr int kFirstIndex = FTR_NUM;
constexpr int kCustomCount = mRmTp_FTR_INDEX_MAX - FTR_NUM;

struct CustomFurniture {
    bool used = false;
    std::string id;
    std::string name;
    C_FurnitureDef def{};
};

std::array<CustomFurniture, kCustomCount> sFurniture;
StableIds sIndices("custom_furniture.json", kFirstIndex, mRmTp_FTR_INDEX_MAX - 1);

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
    const int index = sIndices.Get(def->id);
    if (index < 0) {
        return EMPTY_NO;
    }
    CustomFurniture& furniture = sFurniture[index - kFirstIndex];
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
