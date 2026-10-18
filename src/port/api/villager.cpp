#ifdef ENABLE_SCRIPTING

#include "port/ShipInit.hpp"
#include "port/api/GameText.hpp"
#include "port/api/StableIds.hpp"
#include "port/api/TextureData.hpp"
#include "port/events/Events.h"

#include <spdlog/spdlog.h>

#define this this_
extern "C" {
#include "port/api/villager.h"
#include "m_common_data.h"
#include "m_npc.h"
extern mNpc_Default_Data_c npc_def_list[];
}
#undef this

#include <array>
#include <cstring>
#include <string>

namespace {

constexpr int kCount = C_VILLAGER_LAST + 1 - C_VILLAGER_FIRST;

struct CustomVillager {
    bool used = false;
    std::string id;
    std::string name;
    std::string catchphrase;
    alignas(8) uint8_t palette[TextureData::kPaletteSize];
    C_VillagerDef def{};
};

std::array<CustomVillager, kCount> sVillagers;
StableIds sIds("custom_villagers.json", C_VILLAGER_FIRST, C_VILLAGER_LAST);

CustomVillager* Find(int npc_id) {
    const int slot = npc_id - C_VILLAGER_FIRST;
    if (slot < 0 || slot >= kCount || !sVillagers[slot].used) {
        return nullptr;
    }
    return &sVillagers[slot];
}

std::string Encode(const char* text, size_t length) {
    std::string encoded = GameText_Encode(text != nullptr ? text : "", false);
    encoded.resize(length, ' ');
    return encoded;
}

bool IsKnownVillager(mActor_name_t npc_id) {
    if (npc_id >= NPC_START && npc_id < NPC_START + NPC_NUM) {
        return true;
    }
    return Find(npc_id) != nullptr;
}

void InitVillagerApi() {
    REGISTER_LISTENER(VillagerLookup, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        auto* lookup = reinterpret_cast<VillagerLookup*>(event);
        if (CustomVillager* villager = Find(lookup->npcId)) {
            lookup->definition = &villager->def;
            lookup->name = (const uint8_t*)villager->name.data();
            lookup->catchphrase = (const uint8_t*)villager->catchphrase.data();
        }
    });
}

RegisterShipInitFunc sInitVillagerApi(InitVillagerApi);

} // namespace

extern "C" mActor_name_t C_RegisterVillager(const C_VillagerDef* def) {
    if (def == nullptr || def->id == nullptr || def->id[0] == '\0') {
        return EMPTY_NO;
    }
    if (def->base < NPC_START || def->base >= NPC_START + NPC_NUM || def->looks >= mNpc_LOOKS_NUM) {
        SPDLOG_ERROR("Villager {}: base must be a built-in villager and looks a mNpc_LOOKS_* value", def->id);
        return EMPTY_NO;
    }
    uint8_t palette[TextureData::kPaletteSize];
    if (def->palette != nullptr && !TextureData::ReadPalette16(def->palette, palette)) {
        SPDLOG_ERROR("Villager {}: could not load the palette", def->id);
        return EMPTY_NO;
    }
    const int npc_id = sIds.Get(def->id);
    if (npc_id < 0) {
        return EMPTY_NO;
    }
    CustomVillager& villager = sVillagers[npc_id - C_VILLAGER_FIRST];
    villager.used = true;
    villager.id = def->id;
    villager.name = Encode(def->name, ANIMAL_NAME_LEN);
    villager.catchphrase = Encode(def->catchphrase, ANIMAL_CATCHPHRASE_LEN);
    villager.def = *def;
    villager.def.id = villager.id.c_str();
    villager.def.name = nullptr;
    villager.def.catchphrase = nullptr;
    if (def->palette != nullptr) {
        memcpy(villager.palette, palette, sizeof(palette));
        villager.def.palette = villager.palette;
    }
    mNpc_ClearCacheName();
    return (mActor_name_t)npc_id;
}

extern "C" void C_UnregisterVillager(mActor_name_t npc_id) {
    if (CustomVillager* villager = Find(npc_id)) {
        villager->used = false;
        mNpc_ClearCacheName();
    }
}

extern "C" int C_ReplaceVillager(mActor_name_t old_npc_id, mActor_name_t new_npc_id) {
    Animal_c* animals = Save_Get(animals);
    const int slot = mNpc_SearchAnimalinfo(animals, old_npc_id, ANIMAL_NUM_MAX);
    if (slot < 0 || !IsKnownVillager(new_npc_id) || mNpc_SearchAnimalinfo(animals, new_npc_id, ANIMAL_NUM_MAX) >= 0) {
        return FALSE;
    }
    Animal_c* animal = &animals[slot];
    const Anmhome_c home = animal->home_info;

    mNpc_ClearAnimalInfo(animal);
    mNpc_SetDefAnimal(animal, new_npc_id, npc_def_list);
    animal->id.name_id = (u8)new_npc_id;
    animal->home_info = home;
    animal->moved_in = TRUE;

    if (home.block_x >= 1 && home.block_x <= FG_BLOCK_X_NUM && home.block_z >= 1 && home.block_z <= FG_BLOCK_Z_NUM &&
        home.ut_z >= 1) {
        mActor_name_t& house = Save_Get(fg[home.block_z - 1][home.block_x - 1]).items[home.ut_z - 1][home.ut_x];
        if (house == NPC_ID_TO_NPC_HOUSE_ID(old_npc_id)) {
            house = NPC_ID_TO_NPC_HOUSE_ID(new_npc_id);
        }
    }

    const int used = new_npc_id & 0xFFF;
    if (used / 8 < (int)sizeof(Save_Get(npc_used_tbl))) {
        Save_Get(npc_used_tbl)[used / 8] |= (u8)(1 << (used & 7));
    }
    mNpc_SetNpcList(Common_Get(npclist), animals, ANIMAL_NUM_MAX, FALSE);
    mNpc_ClearCacheName();
    return TRUE;
}

#endif
