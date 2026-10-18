#pragma once

#include <stdint.h>

#include "api.h"
#include "m_name_table.h"
#include "port/events/Events.h"

#define C_VILLAGER_FIRST (NPC_START + NPC_NUM + 2)
#define C_VILLAGER_LAST (NPC_START + 0xFE)
#define C_VILLAGER_DUMMY_FIRST 0xF800
#define C_VILLAGER_ROOM_SIZE (16 * 16)
#define C_VILLAGER_EYES 8
#define C_VILLAGER_MOUTHS 6

typedef struct C_VillagerDef_s {
    const char* id;                // stable unique key, e.g. "mymod.tabby", keeps the same villager id across sessions
    const char* name;              // shown in game, up to 8 characters (UTF-8)
    const char* catchphrase;       // added to the end of sentences, up to 10 characters (UTF-8)
    mActor_name_t base;            // NPC_* built-in villager whose species, model, animations and house layout it uses
    uint8_t looks;                 // mNpc_LOOKS_* personality
    mActor_name_t cloth;           // shirt worn when moving in (built-in or custom), EMPTY_NO for the base's
    const void* texture;           // body texture replacing the base's, same size and format, NULL for the base's.
                                   // Raw arrays must be 2-byte aligned, like C_ASSET and #asset paths
    const void* palette;           // 16 colour palette replacing the base's, NULL for the base's
    uint16_t voice;                // voice number, 0 for the base's
    int8_t house_type;             // house shape 0 to 4, -1 for the base's
    int8_t house_palette;          // house colours 0 to 4, -1 for the base's
    mActor_name_t wallpaper;       // wallpaper of the room, built-in or custom, EMPTY_NO for the base's
    mActor_name_t carpet;          // carpet of the room, built-in or custom, EMPTY_NO for the base's
    uint8_t starter;               // can be one of the first villagers of a new town
    const mActor_name_t* room;     // 16x16 item numbers on the floor of the room, NULL for the base's
    const mActor_name_t* room_top; // 16x16 item numbers on top of the furniture, NULL for the base's
    const void* skeleton;          // cKF_Skeleton_R_c with the villager rig's 26 joints, NULL for the base's
    const void* const* eyes;       // 8 eye textures in the base's format, NULL for the base's
    const void* const* mouths;     // 6 mouth textures in the base's format, NULL for the base's
    float scale;                   // model scale, 0 for the base's (most villagers use 0.0095)
    uint8_t eye_height;            // height of the eyes above the ground, 0 for the base's
    int16_t col_radius;            // collision radius, 0 for the base's
    int16_t col_height;            // collision height, 0 for the base's
    int8_t talk_type;              // 0 no mouth animation, 1 or 2 the two talking styles, -1 for the base's
    uint8_t mouth_anim;            // 1 or 2 for the two mouth animations, 0 for the base's
    const void* const*
        animations; // aNPC_ANIM_NUM entries of aNPC_Animation_c replacing the base's, NULL for the base's
} C_VillagerDef;

// Registers a villager. Returns its villager id, or EMPTY_NO.
#define C_RegisterVillager(def) C_RegisterVillagerSized((def), sizeof(*(def)))
BW_API mActor_name_t C_RegisterVillagerSized(const C_VillagerDef* def, uint32_t size);
// Unregisters a villager. A town it lives in shows it as its base until it is registered again.
BW_API void C_UnregisterVillager(mActor_name_t npc_id);
// Replaces the town villager old_npc_id with new_npc_id, in the same house, as if they had just moved in.
// The town shows the change the next time it loads. Returns FALSE if old_npc_id doesn't live in town.
BW_API int C_ReplaceVillager(mActor_name_t old_npc_id, mActor_name_t new_npc_id);

static inline int C_IsCustomVillager(mActor_name_t npc_id) {
    return npc_id >= C_VILLAGER_FIRST && npc_id <= C_VILLAGER_LAST;
}

static inline const C_VillagerDef* C_FindVillager(mActor_name_t npc_id) {
    if (!C_IsCustomVillager(npc_id)) {
        return NULL;
    }
    {
        CALL_EVENT(VillagerLookup, npc_id, NULL, NULL, NULL, NULL, NULL);
        return VillagerLookup_.definition;
    }
}

// Built-in villager whose data a villager id uses: itself for built-in villagers, the base for custom ones.
static inline mActor_name_t C_VillagerBase(mActor_name_t npc_id) {
    const C_VillagerDef* villager;

    if (!C_IsCustomVillager(npc_id)) {
        return npc_id;
    }
    villager = C_FindVillager(npc_id);
    return villager != NULL ? villager->base : NPC_START;
}

static inline const uint8_t* C_VillagerName(mActor_name_t npc_id) {
    if (!C_IsCustomVillager(npc_id)) {
        return NULL;
    }
    {
        CALL_EVENT(VillagerLookup, npc_id, NULL, NULL, NULL, NULL, NULL);
        return VillagerLookup_.name;
    }
}

static inline const uint8_t* C_VillagerCatchphrase(mActor_name_t npc_id) {
    if (!C_IsCustomVillager(npc_id)) {
        return NULL;
    }
    {
        CALL_EVENT(VillagerLookup, npc_id, NULL, NULL, NULL, NULL, NULL);
        return VillagerLookup_.catchphrase;
    }
}

// A custom villager's own room layout: the floor with top 0, the items on top of furniture with top 1. NULL
// when it uses the base's.
static inline const mActor_name_t* C_VillagerRoom(mActor_name_t npc_id, int top) {
    if (!C_IsCustomVillager(npc_id)) {
        return NULL;
    }
    {
        CALL_EVENT(VillagerLookup, npc_id, NULL, NULL, NULL, NULL, NULL);
        return top ? VillagerLookup_.roomTop : VillagerLookup_.room;
    }
}

static inline int C_IsVillagerHouse(mActor_name_t item) {
    return item >= NPC_HOUSE_START && item <= NPC_ID_TO_NPC_HOUSE_ID(C_VILLAGER_LAST);
}

// Door marker placed in front of a villager house.
static inline mActor_name_t C_VillagerHouseDummy(mActor_name_t house) {
    if (house >= NPC_ID_TO_NPC_HOUSE_ID(C_VILLAGER_FIRST)) {
        return (mActor_name_t)(C_VILLAGER_DUMMY_FIRST + house - NPC_ID_TO_NPC_HOUSE_ID(C_VILLAGER_FIRST));
    }
    return (mActor_name_t)(house - NPC_HOUSE_START + DUMMY_NPC_HOUSE_000);
}

static inline int C_IsVillagerHouseDummy(mActor_name_t item) {
    return ITEM_IS_DUMMY_NPC_HOUSE(item) ||
           (item >= C_VILLAGER_DUMMY_FIRST && item <= C_VILLAGER_DUMMY_FIRST + C_VILLAGER_LAST - C_VILLAGER_FIRST);
}
