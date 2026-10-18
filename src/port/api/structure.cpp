#ifdef ENABLE_SCRIPTING

#include "port/ShipInit.hpp"
#include "port/api/GameText.hpp"
#include "port/api/ModOwner.hpp"
#include "port/api/SizedDef.hpp"
#include "port/api/StableIds.hpp"
#include "port/events/Events.h"

#include <spdlog/spdlog.h>

#define this this_
extern "C" {
#include "port/api/actor.h"
#include "port/api/structure.h"
#include "ac_custom_structure.h"
#include "ac_structure.h"
#include "m_collision_bg.h"
#include "m_common_data.h"
#include "m_field_info.h"
#include "m_play.h"
#include "m_scene_table.h"
}
#undef this

#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int kCount = C_STRUCTURE_LAST + 1 - C_STRUCTURE_FIRST;
constexpr int kStockRoomSize = 8;
constexpr int kFloorHeight = 4;
constexpr int kWallHeight = 31;
constexpr int kFootprintHeight = 10;

struct CustomStructure {
    bool used = false;
    std::string id;
    std::string name;
    std::string owner;
    C_StructureDef def{};
    C_StructureRoomDef room{};
    std::vector<mActor_name_t> layout;
    std::vector<mActor_name_t> layoutTop;
    std::vector<uint32_t> collision;
    int16_t wall = 0;
    int16_t floor = 0;
    int16_t entryX = 0;
    int16_t entryZ = 0;
};

std::array<CustomStructure, kCount> sStructures;
StableIds sIds("structures", C_STRUCTURE_FIRST, C_STRUCTURE_LAST);
int16_t sProfile = -1;

CustomStructure* Find(int item) {
    const int slot = item - C_STRUCTURE_FIRST;
    if (slot < 0 || slot >= kCount || !sStructures[slot].used) {
        return nullptr;
    }
    return &sStructures[slot];
}

int RoomWidth(const C_StructureRoomDef& room) {
    return room.model != nullptr && room.width != 0 ? room.width : kStockRoomSize;
}

int RoomDepth(const C_StructureRoomDef& room) {
    return room.model != nullptr && room.depth != 0 ? room.depth : kStockRoomSize;
}

void AddDefaultExit(std::vector<mActor_name_t>& layout, int width, int depth) {
    for (int x = width / 2 - 1; x <= width / 2; x++) {
        layout[(depth - 1) * 16 + x] = RSV_NO;
        layout[depth * 16 + x] = EXIT_DOOR;
    }
}

void BuildLayout(CustomStructure& structure) {
    const C_StructureRoomDef& room = structure.room;
    const int width = RoomWidth(room);
    const int depth = RoomDepth(room);

    structure.layout.assign(C_ROOM_SIZE, RSV_NO);
    if (room.layout != nullptr) {
        structure.layout.assign(room.layout, room.layout + C_ROOM_SIZE);
    } else {
        for (int z = 0; z < depth; z++) {
            for (int x = 0; x < width; x++) {
                if (x == 0 || x == width - 1 || z == 0 || z == depth - 1) {
                    structure.layout[z * 16 + x] = RSV_WALL_NO;
                }
            }
        }
        AddDefaultExit(structure.layout, width, depth);
    }

    bool has_exit = false;
    for (mActor_name_t item : structure.layout) {
        has_exit |= item == EXIT_DOOR || item == EXIT_DOOR1;
    }
    if (!has_exit) {
        SPDLOG_WARN("Structure {}: the room has no EXIT_DOOR, adding one in the middle of the front wall", structure.id);
        AddDefaultExit(structure.layout, width, depth);
    }

    int exit_row = -1;
    int exit_x0 = 0;
    int exit_x1 = 0;
    for (int z = UT_Z_NUM - 1; z >= 0 && exit_row < 0; z--) {
        for (int x = 0; x < UT_X_NUM; x++) {
            const mActor_name_t item = structure.layout[z * UT_X_NUM + x];
            if (item != EXIT_DOOR && item != EXIT_DOOR1) {
                continue;
            }
            if (exit_row < 0) {
                exit_row = z;
                exit_x0 = x;
            }
            exit_x1 = x;
        }
    }
    structure.entryX = (int16_t)((exit_x0 + exit_x1 + 1) * mFI_UT_WORLDSIZE_X / 2);
    structure.entryZ = (int16_t)((exit_row - 1) * mFI_UT_WORLDSIZE_Z + mFI_UT_WORLDSIZE_Z / 2);

    if (room.layout_top != nullptr) {
        structure.layoutTop.assign(room.layout_top, room.layout_top + C_ROOM_SIZE);
    } else {
        structure.layoutTop.assign(C_ROOM_SIZE, RSV_NO);
    }

    structure.collision.clear();
    if (room.model != nullptr) {
        structure.collision.resize(C_ROOM_SIZE);
        for (int z = 0; z < 16; z++) {
            for (int x = 0; x < 16; x++) {
                const bool floor = x < width && z < depth && structure.layout[z * 16 + x] != RSV_WALL_NO;
                const u32 height = floor ? kFloorHeight : kWallHeight;
                mCoBG_Collision_u col;

                col.raw = 0;
                col.data.center = height;
                col.data.top_left = height;
                col.data.bot_left = height;
                col.data.bot_right = height;
                col.data.top_right = height;
                col.data.unit_attribute = mCoBG_ATTRIBUTE_FLOOR;
                structure.collision[z * 16 + x] = col.raw;
            }
        }
    }

    structure.wall = room.wallpaper != EMPTY_NO ? (int16_t)(room.wallpaper - ITM_WALL_START) : 0;
    structure.floor = room.carpet != EMPTY_NO ? (int16_t)(room.carpet - ITM_CARPET_START) : 0;
}

bool InTown() {
    return mFI_CheckFieldData() == TRUE && mFI_GET_TYPE(mFI_GetFieldId()) == mFI_FIELD_FG &&
           Save_Get(scene_no) == SCENE_FG;
}

bool ValidBlock(int block_x, int block_z) {
    return block_x >= 1 && block_x <= FG_BLOCK_X_NUM && block_z >= 1 && block_z <= FG_BLOCK_Z_NUM;
}

mActor_name_t* BlockItems(int block_x, int block_z) {
    return Save_Get(fg[block_z - 1][block_x - 1]).items[0];
}

struct Footprint {
    int x0;
    int x1;
    int z0;
    int z1;
    bool door;
    int door_x;
};

bool GetFootprint(const C_StructureDef& def, int ut_x, int ut_z, Footprint* out) {
    out->x0 = ut_x - (def.width - 1) / 2;
    out->x1 = ut_x + def.width / 2;
    out->z0 = ut_z - (def.depth - 1);
    out->z1 = ut_z;
    out->door = def.room != nullptr;
    out->door_x = ut_x + def.door_x;
    const int last_z = out->door ? out->z1 + 1 : out->z1;
    return out->x0 >= 0 && out->x1 < UT_X_NUM && out->z0 >= 0 && last_z < UT_Z_NUM;
}

template <typename Fn> void ForEachTile(const Footprint& footprint, Fn fn) {
    for (int z = footprint.z0; z <= footprint.z1; z++) {
        for (int x = footprint.x0; x <= footprint.x1; x++) {
            fn(x, z);
        }
    }
    if (footprint.door) {
        fn(footprint.door_x, footprint.z1 + 1);
    }
}

xyz_t TilePosition(int block_x, int block_z, int ut_x, int ut_z) {
    xyz_t pos;

    pos.x = (f32)(block_x * mFI_BK_WORLDSIZE_X + ut_x * mFI_UT_WORLDSIZE_X) + mFI_UT_WORLDSIZE_HALF_X_F;
    pos.y = 0.0f;
    pos.z = (f32)(block_z * mFI_BK_WORLDSIZE_Z + ut_z * mFI_UT_WORLDSIZE_Z) + mFI_UT_WORLDSIZE_HALF_Z_F;
    return pos;
}

void SetCollision(const C_StructureDef& def, int block_x, int block_z, int ut_x, int ut_z, bool raise) {
    Footprint footprint;
    mCoBG_OffsetTable_c offset = { mCoBG_ATTRIBUTE_NONE, 0, 0, 0, 0, 0, 0 };

    if (!InTown() || !GetFootprint(def, ut_x, ut_z, &footprint)) {
        return;
    }
    if (raise) {
        offset.centerRight_offset = kFootprintHeight;
        offset.leftUp_offset = kFootprintHeight;
        offset.leftDown_offset = kFootprintHeight;
        offset.rightDown_offset = kFootprintHeight;
        offset.rightUp_offset = kFootprintHeight;
    }
    for (int z = footprint.z0; z <= footprint.z1; z++) {
        for (int x = footprint.x0; x <= footprint.x1; x++) {
            mCoBG_SetPluss5PointOffset_file(TilePosition(block_x, block_z, x, z), offset, (char*)__FILE__, __LINE__);
        }
    }
}

ACTOR* FindLiveActor(int block_x, int block_z, int ut_x, int ut_z) {
    GAME_PLAY* play = C_GetPlay();
    int bx;
    int bz;
    int ux;
    int uz;

    if (play == nullptr || !InTown()) {
        return nullptr;
    }
    for (ACTOR* actor = play->actor_info.list[ACTOR_PART_ITEM].actor; actor != nullptr; actor = actor->next_actor) {
        if (actor->mv_proc == nullptr && actor->dw_proc == nullptr) {
            continue;
        }
        if (C_IsCustomStructure(actor->npc_id) &&
            mFI_Wpos2BkandUtNuminBlock(&bx, &bz, &ux, &uz, actor->home.position) && bx == block_x &&
            bz == block_z && ux == ut_x && uz == ut_z) {
            return actor;
        }
    }
    return nullptr;
}

void DeleteActor(ACTOR* actor, bool keep) {
    if (!keep) {
        actor->block_x = -1;
        actor->block_z = -1;
    }
    Actor_delete(actor);
}

void InitStructureApi() {
    sProfile = C_RegisterActor(&Custom_Structure_Profile);

    REGISTER_LISTENER(StructureLookup, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        auto* lookup = reinterpret_cast<StructureLookup*>(event);
        if (CustomStructure* structure = Find(lookup->item)) {
            lookup->definition = &structure->def;
            lookup->profile = sProfile;
            lookup->name = structure->def.name != nullptr ? (const uint8_t*)structure->name.data() : nullptr;
            if (structure->def.room != nullptr) {
                lookup->room = structure->layout.data();
                lookup->roomTop = structure->layoutTop.data();
                lookup->roomCollision = structure->collision.empty() ? nullptr : structure->collision.data();
                lookup->wall = structure->wall;
                lookup->floor = structure->floor;
                lookup->resident = structure->room.resident;
                lookup->entryX = structure->entryX;
                lookup->entryZ = structure->entryZ;
            }
        }
    });
    ModOwner::AddReleaser([](const std::string& mod) {
        for (int slot = 0; slot < kCount; slot++) {
            if (sStructures[slot].used && sStructures[slot].owner == mod) {
                C_UnregisterStructure((mActor_name_t)(C_STRUCTURE_FIRST + slot));
            }
        }
    });
}

RegisterShipInitFunc sInitStructureApi(InitStructureApi);

} // namespace

extern "C" mActor_name_t C_RegisterStructureSized(const C_StructureDef* structure_def, uint32_t size,
                                                  uint32_t room_size) {
    const SizedDef<C_StructureDef> sized(structure_def, size);
    const C_StructureDef* def = sized.get();
    if (def == nullptr || def->id == nullptr || def->id[0] == '\0') {
        return EMPTY_NO;
    }
    const SizedDef<C_StructureRoomDef> sizedRoom(def->room, room_size);
    if (def->width < 1 || def->width > C_STRUCTURE_MAX_SIZE || def->depth < 1 || def->depth > C_STRUCTURE_MAX_SIZE) {
        SPDLOG_ERROR("Structure {}: width and depth must be 1 to {} tiles", def->id, C_STRUCTURE_MAX_SIZE);
        return EMPTY_NO;
    }
    if (def->door_x < -(def->width - 1) / 2 || def->door_x > def->width / 2) {
        SPDLOG_ERROR("Structure {}: door_x must be a column of the front row, {} to {}", def->id,
                     -(def->width - 1) / 2, def->width / 2);
        return EMPTY_NO;
    }
    if (def->room != nullptr) {
        const C_StructureRoomDef& room = *sizedRoom.get();
        if (RoomWidth(room) < 3 || RoomWidth(room) > C_ROOM_MAX_WIDTH || RoomDepth(room) < 3 ||
            RoomDepth(room) > C_ROOM_MAX_DEPTH) {
            SPDLOG_ERROR("Structure {}: the room must be 3 to {} tiles wide and 3 to {} deep", def->id,
                         C_ROOM_MAX_WIDTH, C_ROOM_MAX_DEPTH);
            return EMPTY_NO;
        }
        if ((room.wallpaper != EMPTY_NO && !ITEM_IS_WALL(room.wallpaper) &&
             ITEM_NAME_GET_CAT(room.wallpaper) != ITEM1_CAT_WALL) ||
            (room.carpet != EMPTY_NO && !ITEM_IS_CARPET(room.carpet) &&
             ITEM_NAME_GET_CAT(room.carpet) != ITEM1_CAT_CARPET)) {
            SPDLOG_ERROR("Structure {}: wallpaper and carpet must be wallpaper and carpet items", def->id);
            return EMPTY_NO;
        }
    }
    const std::string owner = ModOwner::Current();
    const int item = sIds.Register(def->id, owner);
    if (item < 0) {
        return EMPTY_NO;
    }
    CustomStructure& structure = sStructures[item - C_STRUCTURE_FIRST];
    structure.used = true;
    structure.id = def->id;
    structure.owner = owner;
    structure.name = GameText_Encode(def->name != nullptr ? def->name : "", false);
    structure.name.resize(PLAYER_NAME_LEN, ' ');
    structure.def = *def;
    structure.def.id = structure.id.c_str();
    if (def->room != nullptr) {
        structure.room = *sizedRoom.get();
        structure.def.room = &structure.room;
        BuildLayout(structure);
        structure.room.layout = structure.layout.data();
        structure.room.layout_top = structure.layoutTop.data();
    } else {
        structure.def.room = nullptr;
        structure.layout.clear();
        structure.layoutTop.clear();
        structure.collision.clear();
    }
    return (mActor_name_t)item;
}

extern "C" void C_UnregisterStructure(mActor_name_t item) {
    CustomStructure* structure = Find(item);
    GAME_PLAY* play = C_GetPlay();
    int bx;
    int bz;
    int ux;
    int uz;

    if (structure == nullptr) {
        return;
    }
    if (play != nullptr && InTown()) {
        for (ACTOR* actor = play->actor_info.list[ACTOR_PART_ITEM].actor; actor != nullptr;
             actor = actor->next_actor) {
            if (actor->npc_id == item && actor->dw_proc != nullptr &&
                mFI_Wpos2BkandUtNuminBlock(&bx, &bz, &ux, &uz, actor->home.position)) {
                SetCollision(structure->def, bx, bz, ux, uz, false);
                DeleteActor(actor, true);
            }
        }
    }
    structure->used = false;
    sIds.Unregister(item);
}

extern "C" int C_CanPlaceStructure(mActor_name_t item, int block_x, int block_z, int ut_x, int ut_z) {
    CustomStructure* structure = Find(item);
    GAME_PLAY* play = C_GetPlay();
    Footprint footprint;

    if (structure == nullptr || play == nullptr || !InTown() || !ValidBlock(block_x, block_z) ||
        !GetFootprint(structure->def, ut_x, ut_z, &footprint)) {
        return FALSE;
    }

    const mActor_name_t* items = BlockItems(block_x, block_z);
    const int base_x = block_x * UT_X_NUM;
    const int base_z = block_z * UT_Z_NUM;
    const u8 ground = mFI_UtNum2UtKeepH(base_x + ut_x, base_z + ut_z);
    int player_bx = -1;
    int player_bz = -1;
    int player_ux = -1;
    int player_uz = -1;
    bool fits = true;

    if (ACTOR* player = play->actor_info.list[ACTOR_PART_PLAYER].actor) {
        mFI_Wpos2BkandUtNuminBlock(&player_bx, &player_bz, &player_ux, &player_uz, player->world.position);
    }

    ForEachTile(footprint, [&](int x, int z) {
        const mCoBG_Collision_u* col = mFI_UtNum2UtCol(base_x + x, base_z + z);

        if (items[z * UT_X_NUM + x] != EMPTY_NO || col == nullptr ||
            mFI_UtNum2UtKeepH(base_x + x, base_z + z) != ground || col->data.center != ground ||
            col->data.top_left != ground || col->data.bot_left != ground || col->data.bot_right != ground ||
            col->data.top_right != ground || col->data.slate_flag || col->data.unit_attribute > mCoBG_ATTRIBUTE_SOIL2 ||
            (player_bx == block_x && player_bz == block_z && player_ux == x && player_uz == z)) {
            fits = false;
        }
    });
    return fits ? TRUE : FALSE;
}

extern "C" int C_PlaceStructure(mActor_name_t item, int block_x, int block_z, int ut_x, int ut_z) {
    GAME_PLAY* play = C_GetPlay();
    Footprint footprint;

    if (!C_CanPlaceStructure(item, block_x, block_z, ut_x, ut_z)) {
        return FALSE;
    }
    GetFootprint(Find(item)->def, ut_x, ut_z, &footprint);

    mActor_name_t* items = BlockItems(block_x, block_z);
    ForEachTile(footprint, [&](int x, int z) { items[z * UT_X_NUM + x] = RSV_NO; });
    items[ut_z * UT_X_NUM + ut_x] = item;

    if (play->block_table.block_x == block_x && play->block_table.block_z == block_z &&
        Common_Get(clip).structure_clip != nullptr) {
        const xyz_t pos = TilePosition(block_x, block_z, ut_x, ut_z);
        (*Common_Get(clip).structure_clip->setup_actor_proc)((GAME*)play, item, -1, pos.x, pos.z);
    }
    return TRUE;
}

extern "C" int C_PlaceStructureAt(mActor_name_t item, float x, float z) {
    xyz_t pos = { x, 0.0f, z };
    int block_x;
    int block_z;
    int ut_x;
    int ut_z;

    if (!mFI_Wpos2BkandUtNuminBlock(&block_x, &block_z, &ut_x, &ut_z, pos)) {
        return FALSE;
    }
    return C_PlaceStructure(item, block_x, block_z, ut_x, ut_z);
}

extern "C" int C_RemoveStructure(int block_x, int block_z, int ut_x, int ut_z) {
    if (!ValidBlock(block_x, block_z) || ut_x < 0 || ut_x >= UT_X_NUM || ut_z < 0 || ut_z >= UT_Z_NUM) {
        return FALSE;
    }

    mActor_name_t* items = BlockItems(block_x, block_z);
    mActor_name_t item = items[ut_z * UT_X_NUM + ut_x];
    ACTOR* actor = FindLiveActor(block_x, block_z, ut_x, ut_z);

    if (actor != nullptr) {
        item = actor->npc_id;
    }
    if (!C_IsCustomStructure(item)) {
        return FALSE;
    }

    if (CustomStructure* structure = Find(item)) {
        Footprint footprint;

        SetCollision(structure->def, block_x, block_z, ut_x, ut_z, false);
        if (GetFootprint(structure->def, ut_x, ut_z, &footprint)) {
            ForEachTile(footprint, [&](int x, int z) {
                mActor_name_t& tile = items[z * UT_X_NUM + x];
                if (tile == RSV_NO || tile == item) {
                    tile = EMPTY_NO;
                }
            });
        }
    }
    items[ut_z * UT_X_NUM + ut_x] = EMPTY_NO;
    if (actor != nullptr) {
        DeleteActor(actor, false);
    }
    return TRUE;
}

extern "C" int C_FindPlacedStructure(mActor_name_t item, int* block_x, int* block_z, int* ut_x, int* ut_z) {
    GAME_PLAY* play = C_GetPlay();
    int bx;
    int bz;
    int ux;
    int uz;
    bool found = false;

    if (!C_IsCustomStructure(item)) {
        return FALSE;
    }
    if (play != nullptr && InTown()) {
        for (ACTOR* actor = play->actor_info.list[ACTOR_PART_ITEM].actor; actor != nullptr && !found;
             actor = actor->next_actor) {
            if (actor->npc_id == item && actor->dw_proc != nullptr &&
                mFI_Wpos2BkandUtNuminBlock(&bx, &bz, &ux, &uz, actor->home.position)) {
                found = true;
            }
        }
    }
    for (int z = 1; z <= FG_BLOCK_Z_NUM && !found; z++) {
        for (int x = 1; x <= FG_BLOCK_X_NUM && !found; x++) {
            const mActor_name_t* items = BlockItems(x, z);
            for (int i = 0; i < UT_X_NUM * UT_Z_NUM; i++) {
                if (items[i] == item) {
                    bx = x;
                    bz = z;
                    ux = i % UT_X_NUM;
                    uz = i / UT_X_NUM;
                    found = true;
                    break;
                }
            }
        }
    }
    if (!found) {
        return FALSE;
    }
    if (block_x != nullptr) {
        *block_x = bx;
    }
    if (block_z != nullptr) {
        *block_z = bz;
    }
    if (ut_x != nullptr) {
        *ut_x = ux;
    }
    if (ut_z != nullptr) {
        *ut_z = uz;
    }
    return TRUE;
}

#endif
