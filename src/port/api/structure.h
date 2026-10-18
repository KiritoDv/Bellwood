#pragma once

#include <stdint.h>

#include "api.h"
#include "m_name_table.h"
#include "port/api/villager.h"
#include "port/events/Events.h"

#define C_STRUCTURE_FIRST 0x5900
#define C_STRUCTURE_LAST 0x5FFF
#define C_STRUCTURE_MAX_SIZE 9
#define C_ROOM_SIZE (16 * 16)
#define C_ROOM_MAX_WIDTH 16
#define C_ROOM_MAX_DEPTH 15

typedef struct C_StructureRoomDef_s {
    const mActor_name_t* layout;     // 16x16 item numbers on the floor, like a villager room: RSV_WALL_NO for walls,
                                     // RSV_NO for free floor, EXIT_DOOR where the player leaves. NULL for an empty
                                     // room with walls around it and the exit in the middle of the front wall
    const mActor_name_t* layout_top; // 16x16 item numbers on top of furniture, NULL for none
    mActor_name_t wallpaper;         // wallpaper item, built-in or custom, EMPTY_NO for the first one
    mActor_name_t carpet;            // carpet item, built-in or custom, EMPTY_NO for the first one
    const void* model;               // room model drawn instead of the walls and floor, e.g. #asset "room.gltf"
                                     // ROOM_MODEL, NULL for the villager house room
    const void* model_xlu;           // translucent part of the room model (ROOM_MODEL_XLU), NULL for none
    uint8_t width;                   // tiles across, walls included, up to 16; 0 for 8. Only used with a model
    uint8_t depth;                   // tiles from the back wall to the front wall, up to 15; 0 for 8
    mActor_name_t resident;          // villager id of a town villager who is found in the room, EMPTY_NO for nobody
} C_StructureRoomDef;

typedef struct C_StructureDef_s {
    const char* id;                  // stable unique key, e.g. "mymod.kiosk", keeps the same item number across sessions
    const char* name;                // shown in its acre on the town map, up to 8 characters (UTF-8), NULL for none
    const void* model;               // display list drawn with its origin at the centre of the anchor tile on the
                                     // ground, e.g. #asset "kiosk.gltf" MODEL
    const void* model_xlu;           // translucent part (MODEL_XLU), NULL for none
    const void* winter_model;        // model shown in winter, NULL to use model all year
    const void* winter_model_xlu;    // translucent part in winter, NULL to keep model_xlu
    uint8_t width;                   // footprint in tiles, 1 to 9, centred on the anchor
    uint8_t depth;                   // footprint in tiles, 1 to 9, from the anchor (the front row) to the back
    int8_t door_x;                   // column of the door in the front row, counted from the anchor (negative is west)
    const C_StructureRoomDef* room;  // room behind the door, NULL if it can't be entered
} C_StructureDef;

// Registers a structure. Returns its item number, or EMPTY_NO.
#define C_RegisterStructure(def) C_RegisterStructureSized((def), sizeof(*(def)), sizeof(C_StructureRoomDef))
BW_API mActor_name_t C_RegisterStructureSized(const C_StructureDef* def, uint32_t size, uint32_t room_size);
// Unregisters a structure. Where it stands the town keeps its place, empty, until it is registered again.
BW_API void C_UnregisterStructure(mActor_name_t item);
// Whether the structure fits with its anchor at the tile: the footprint and the tile in front of the door are empty,
// flat grass or soil inside one acre, and the player isn't standing there. block_x 1 to 5 and block_z 1 to 6 are the
// town's acres, ut_x and ut_z 0 to 15 the tile in the acre. Only in town.
BW_API int C_CanPlaceStructure(mActor_name_t item, int block_x, int block_z, int ut_x, int ut_z);
// Places the structure in town with its anchor at the tile. Returns FALSE if it doesn't fit there.
BW_API int C_PlaceStructure(mActor_name_t item, int block_x, int block_z, int ut_x, int ut_z);
// Places the structure with its anchor at the tile under the world position x, z. Returns FALSE if it doesn't fit.
BW_API int C_PlaceStructureAt(mActor_name_t item, float x, float z);
// Removes the structure whose anchor is at the tile. Returns FALSE if there is none.
BW_API int C_RemoveStructure(int block_x, int block_z, int ut_x, int ut_z);
// Finds the first placed structure with that item number. Returns FALSE if it isn't in town.
BW_API int C_FindPlacedStructure(mActor_name_t item, int* block_x, int* block_z, int* ut_x, int* ut_z);

static inline int C_IsCustomStructure(mActor_name_t item) {
    return item >= C_STRUCTURE_FIRST && item <= C_STRUCTURE_LAST;
}

static inline const C_StructureDef* C_FindStructure(mActor_name_t item) {
    if (!C_IsCustomStructure(item)) {
        return NULL;
    }
    {
        CALL_EVENT(StructureLookup, item, NULL, -1, NULL, NULL, NULL, -1, -1);
        return StructureLookup_.definition;
    }
}

// Actor profile that draws a custom structure, -1 if it isn't registered.
static inline int16_t C_StructureProfile(mActor_name_t item) {
    if (!C_IsCustomStructure(item)) {
        return -1;
    }
    {
        CALL_EVENT(StructureLookup, item, NULL, -1, NULL, NULL, NULL, -1, -1);
        return StructureLookup_.definition != NULL ? StructureLookup_.profile : -1;
    }
}

// Layout of the house room being visited when its owner is a custom villager or a custom structure: the floor with
// top 0, the items on top of furniture with top 1. NULL for the game's own.
static inline const mActor_name_t* C_HouseRoom(mActor_name_t owner, int top) {
    if (C_IsCustomVillager(owner)) {
        return C_VillagerRoom(owner, top);
    }
    if (!C_IsCustomStructure(owner)) {
        return NULL;
    }
    {
        CALL_EVENT(StructureLookup, owner, NULL, -1, NULL, NULL, NULL, -1, -1);
        return top ? StructureLookup_.roomTop : StructureLookup_.room;
    }
}

// Room model of a custom structure's room, or its translucent part with xlu. NULL for the walls and floor.
static inline const void* C_HouseRoomModel(mActor_name_t owner, int xlu) {
    const C_StructureDef* structure = C_FindStructure(owner);

    if (structure == NULL || structure->room == NULL) {
        return NULL;
    }
    return xlu ? structure->room->model_xlu : structure->room->model;
}

// 16x16 collision of a custom structure's room that has its own model, NULL otherwise.
static inline const uint32_t* C_HouseRoomCollision(mActor_name_t owner) {
    if (!C_IsCustomStructure(owner)) {
        return NULL;
    }
    {
        CALL_EVENT(StructureLookup, owner, NULL, -1, NULL, NULL, NULL, -1, -1);
        return StructureLookup_.roomCollision;
    }
}

// Width (depth 0) or depth (depth 1) in tiles of a custom structure's room, 0 for the game's own room size.
static inline int C_HouseRoomSize(mActor_name_t owner, int depth) {
    const C_StructureDef* structure = C_FindStructure(owner);

    if (structure == NULL || structure->room == NULL || structure->room->model == NULL) {
        return 0;
    }
    if (depth) {
        return structure->room->depth != 0 ? structure->room->depth : 8;
    }
    return structure->room->width != 0 ? structure->room->width : 8;
}

// Name of a custom structure for the town map, PLAYER_NAME_LEN bytes in the game's encoding, or NULL.
static inline const uint8_t* C_StructureName(mActor_name_t item) {
    if (!C_IsCustomStructure(item)) {
        return NULL;
    }
    {
        CALL_EVENT(StructureLookup, item, NULL, -1, NULL, NULL, NULL, -1, -1, NULL, EMPTY_NO, 0, 0);
        return StructureLookup_.name;
    }
}

// Villager found in a custom structure's room, EMPTY_NO for nobody.
static inline mActor_name_t C_HouseRoomResident(mActor_name_t owner) {
    if (!C_IsCustomStructure(owner)) {
        return EMPTY_NO;
    }
    {
        CALL_EVENT(StructureLookup, owner, NULL, -1, NULL, NULL, NULL, -1, -1, NULL, EMPTY_NO, 0, 0);
        return StructureLookup_.resident;
    }
}

// Where the player enters a custom structure's room: x (z 0) or z (z 1) in world units, -1 when it has no room.
static inline int C_HouseRoomEntry(mActor_name_t owner, int z) {
    if (!C_IsCustomStructure(owner)) {
        return -1;
    }
    {
        CALL_EVENT(StructureLookup, owner, NULL, -1, NULL, NULL, NULL, -1, -1, NULL, EMPTY_NO, -1, -1);
        return z ? StructureLookup_.entryZ : StructureLookup_.entryX;
    }
}

// Wallpaper (wall 1) or carpet (wall 0) index of a custom structure's room, -1 when the owner isn't one.
static inline int C_HouseRoomInterior(mActor_name_t owner, int wall) {
    if (!C_IsCustomStructure(owner)) {
        return -1;
    }
    {
        CALL_EVENT(StructureLookup, owner, NULL, -1, NULL, NULL, NULL, -1, -1);
        if (StructureLookup_.definition == NULL) {
            return -1;
        }
        return wall ? StructureLookup_.wall : StructureLookup_.floor;
    }
}
