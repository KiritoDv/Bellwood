#ifdef ENABLE_SCRIPTING

#include "port/ShipInit.hpp"
#include "port/api/GameText.hpp"
#include "port/api/ModOwner.hpp"
#include "port/api/SizedDef.hpp"
#include "port/api/StableIds.hpp"
#include "port/events/Events.h"

#define this this_
extern "C" {
#include "port/api/furniture.h"
#include "ac_my_room.h"
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
    std::string owner;
    C_FurnitureDef def{};
    aFTR_vtable_c procs{};
    aFTR_vtable_c vtable{};
};

std::array<CustomFurniture, kCustomCount> sFurniture;
StableIds sIndices("furniture", kFirstIndex, mRmTp_FTR_INDEX_MAX - 1);

mActor_name_t IndexToItem(int index) {
    return (mActor_name_t)(FTR1_START + ((index - 0x400) << 2));
}

int ItemToIndex(mActor_name_t item) {
    if (ITEM_NAME_GET_TYPE(item) != NAME_TYPE_FTR1) {
        return -1;
    }
    return 0x400 + ((item - FTR1_START) >> 2);
}

CustomFurniture* Find(int index) {
    const int slot = index - kFirstIndex;
    return slot >= 0 && slot < kCustomCount && sFurniture[slot].used ? &sFurniture[slot] : nullptr;
}

void FurnitureCt(FTR_ACTOR* ftr_actor, u8* data) {
    if (CustomFurniture* furniture = Find(ftr_actor->name)) {
        ModOwner::Scope scope(furniture->owner);
        furniture->procs.ct_proc(ftr_actor, data);
    }
}

void FurnitureMove(FTR_ACTOR* ftr_actor, ACTOR* my_room, GAME* game, u8* data) {
    if (CustomFurniture* furniture = Find(ftr_actor->name)) {
        ModOwner::Scope scope(furniture->owner);
        furniture->procs.move_proc(ftr_actor, my_room, game, data);
    }
}

void FurnitureDraw(FTR_ACTOR* ftr_actor, ACTOR* my_room, GAME* game, u8* data) {
    if (CustomFurniture* furniture = Find(ftr_actor->name)) {
        ModOwner::Scope scope(furniture->owner);
        furniture->procs.draw_proc(ftr_actor, my_room, game, data);
    }
}

void FurnitureDt(FTR_ACTOR* ftr_actor, u8* data) {
    if (CustomFurniture* furniture = Find(ftr_actor->name)) {
        ModOwner::Scope scope(furniture->owner);
        furniture->procs.dt_proc(ftr_actor, data);
    }
}

void FurnitureDma(mActor_name_t ftr_name, u8* data) {
    if (CustomFurniture* furniture = Find(mRmTp_FtrItemNo2FtrIdx(ftr_name))) {
        ModOwner::Scope scope(furniture->owner);
        furniture->procs.dma_proc(ftr_name, data);
    }
}

void WrapVtable(CustomFurniture& furniture) {
    const aFTR_vtable_c* procs = furniture.def.profile.vtable;
    if (procs == nullptr) {
        return;
    }
    furniture.procs = *procs;
    furniture.vtable = {
        procs->ct_proc != nullptr ? &FurnitureCt : nullptr,     procs->move_proc != nullptr ? &FurnitureMove : nullptr,
        procs->draw_proc != nullptr ? &FurnitureDraw : nullptr, procs->dt_proc != nullptr ? &FurnitureDt : nullptr,
        procs->dma_proc != nullptr ? &FurnitureDma : nullptr,
    };
    furniture.def.profile.vtable = &furniture.vtable;
}

void InitFurnitureApi() {
    REGISTER_LISTENER(FurnitureLookup, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        auto* lookup = reinterpret_cast<FurnitureLookup*>(event);
        const int slot = lookup->index - kFirstIndex;
        if (slot >= 0 && slot < kCustomCount && sFurniture[slot].used) {
            lookup->furniture = &sFurniture[slot].def;
        }
    });
    ModOwner::AddReleaser([](const std::string& mod) {
        for (int slot = 0; slot < kCustomCount; slot++) {
            if (sFurniture[slot].used && sFurniture[slot].owner == mod) {
                C_UnregisterFurniture(IndexToItem(kFirstIndex + slot));
            }
        }
    });
}

RegisterShipInitFunc sInitFurnitureApi(InitFurnitureApi);

} // namespace

extern "C" mActor_name_t C_RegisterFurnitureSized(const C_FurnitureDef* furniture_def, uint32_t size) {
    const SizedDef<C_FurnitureDef> sized(furniture_def, size);
    const C_FurnitureDef* def = sized.get();
    if (def == nullptr || def->id == nullptr || def->id[0] == '\0') {
        return EMPTY_NO;
    }
    const std::string owner = ModOwner::Current();
    const int index = sIndices.Register(def->id, owner);
    if (index < 0) {
        return EMPTY_NO;
    }
    CustomFurniture& furniture = sFurniture[index - kFirstIndex];
    if (furniture.used) {
        aMR_DestructFurniture(index);
    }
    furniture.used = true;
    furniture.id = def->id;
    furniture.owner = owner;
    furniture.name = GameText_Encode(def->name != nullptr ? def->name : "", false);
    furniture.name.resize(C_FURNITURE_NAME_LEN, ' ');
    furniture.def = *def;
    furniture.def.id = furniture.id.c_str();
    furniture.def.name = furniture.name.c_str();
    WrapVtable(furniture);
    aMR_ConstructFurniture(index);
    return IndexToItem(index);
}

extern "C" void C_UnregisterFurniture(mActor_name_t item) {
    const int slot = ItemToIndex(item) - kFirstIndex;
    if (slot >= 0 && slot < kCustomCount && sFurniture[slot].used) {
        aMR_DestructFurniture(kFirstIndex + slot);
        sFurniture[slot].used = false;
        sIndices.Unregister(kFirstIndex + slot);
    }
}

#endif
