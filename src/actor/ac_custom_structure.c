#include "ac_custom_structure.h"

#include "m_name_table.h"
#include "m_common_data.h"
#include "m_player_lib.h"
#include "m_collision_bg.h"
#include "m_field_info.h"
#include "m_scene_table.h"
#include "sys_matrix.h"
#include "m_rcp.h"
#include "port/api/structure.h"

static void aCST_actor_ct(ACTOR* actorx, GAME* game);
static void aCST_actor_init(ACTOR* actorx, GAME* game);
static void aCST_actor_move(ACTOR* actorx, GAME* game);
static void aCST_actor_draw(ACTOR* actorx, GAME* game);

// clang-format off
ACTOR_PROFILE Custom_Structure_Profile = {
    0,
    ACTOR_PART_ITEM,
    ACTOR_STATE_TA_SET,
    EMPTY_NO,
    ACTOR_OBJ_BANK_KEEP,
    sizeof(STRUCTURE_ACTOR),
    &aCST_actor_ct,
    mActor_NONE_PROC1,
    &aCST_actor_init,
    &aCST_actor_draw,
    NULL
};
// clang-format on

static void aCST_actor_ct(ACTOR* actorx, GAME* game) {
    STRUCTURE_ACTOR* structure = (STRUCTURE_ACTOR*)actorx;
    const C_StructureDef* def = C_FindStructure(actorx->npc_id);

    structure->season = Common_Get(time.season);

    if (def != NULL) {
        f32 size = (def->width > def->depth ? def->width : def->depth) * mFI_UT_WORLDSIZE_X_F + 100.0f;

        if (size > actorx->cull_width) {
            actorx->cull_width = size;
        }

        if (size > actorx->cull_radius) {
            actorx->cull_radius = size;
        }
    }
}

static void aCST_set_bgOffset(ACTOR* actorx, const C_StructureDef* def) {
    static mCoBG_OffsetTable_c offset = { mCoBG_ATTRIBUTE_NONE, 10, 10, 10, 10, 10, 0 };
    xyz_t pos;
    int x;
    int z;

    for (z = 0; z < def->depth; z++) {
        for (x = 0; x < def->width; x++) {
            pos.x = actorx->home.position.x + (x - (def->width - 1) / 2) * mFI_UT_WORLDSIZE_X_F;
            pos.y = actorx->home.position.y;
            pos.z = actorx->home.position.z - z * mFI_UT_WORLDSIZE_Z_F;
            mCoBG_SetPluss5PointOffset_file(pos, offset, __FILE__, __LINE__);
        }
    }
}

static f32 aCST_door_x(ACTOR* actorx, const C_StructureDef* def) {
    return actorx->home.position.x + def->door_x * mFI_UT_WORLDSIZE_X_F;
}

static void aCST_rewrite_out_data(ACTOR* actorx, const C_StructureDef* def, GAME_PLAY* play) {
    Door_data_c* door_data = Common_GetPointer(structure_exit_door_data);
    xyz_t pos;

    if (play->fb_wipe_mode == WIPE_MODE_NONE) {
        Common_Set(house_owner_name, actorx->npc_id);

        door_data->next_scene_id = Save_Get(scene_no);
        door_data->exit_orientation = mSc_DIRECT_SOUTH;
        door_data->exit_type = 0;
        door_data->extra_data = 2;

        pos.x = aCST_door_x(actorx, def);
        pos.z = actorx->home.position.z + 60.0f;
        pos.y = mCoBG_GetBgY_OnlyCenter_FromWpos2(pos, 0.0f);

        door_data->exit_position.x = (s16)pos.x;
        door_data->exit_position.y = (s16)pos.y;
        door_data->exit_position.z = (s16)pos.z;
        door_data->door_actor_name = actorx->npc_id;
        door_data->wipe_type = WIPE_TYPE_TRIFORCE;
    }
}

static int aCST_check_player(ACTOR* actorx, const C_StructureDef* def, PLAYER_ACTOR* player) {
    u16 angle = player->actor_class.shape_info.rotation.y;
    f32 dx = player->actor_class.world.position.x - aCST_door_x(actorx, def);
    f32 dz = player->actor_class.world.position.z - (actorx->world.position.z + mFI_UT_WORLDSIZE_Z_F);

    return angle > DEG2SHORT_ANGLE2(135.0f) && angle < DEG2SHORT_ANGLE2(225.0f) && (SQ(dx) + SQ(dz)) < 550.0f &&
           player->actor_class.speed > 0.0f;
}

static void aCST_enter(ACTOR* actorx, const C_StructureDef* def, GAME_PLAY* play) {
    Door_data_c door_data;

    aCST_rewrite_out_data(actorx, def, play);

    door_data.next_scene_id = SCENE_NPC_HOUSE;
    door_data.exit_orientation = mSc_DIRECT_NORTH;
    door_data.exit_type = 0;
    door_data.extra_data = 0;
    door_data.exit_position.x = C_HouseRoomEntry(actorx->npc_id, FALSE);
    door_data.exit_position.y = 0;
    door_data.exit_position.z = C_HouseRoomEntry(actorx->npc_id, TRUE);
    door_data.door_actor_name = EMPTY_NO;
    door_data.wipe_type = 1;
    goto_other_scene(play, &door_data, FALSE);
}

static void aCST_actor_move(ACTOR* actorx, GAME* game) {
    GAME_PLAY* play = (GAME_PLAY*)game;
    const C_StructureDef* def = C_FindStructure(actorx->npc_id);
    PLAYER_ACTOR* player = GET_PLAYER_ACTOR(play);

    if (def == NULL) {
        Actor_delete(actorx);
        return;
    }

    if (def->room == NULL || player == NULL) {
        return;
    }

    if (actorx == (*player->get_door_label_proc)(game)) {
        aCST_enter(actorx, def, play);
    } else if (mPlib_check_player_actor_main_index_OutDoorMove(game) == FALSE &&
               aCST_check_player(actorx, def, player)) {
        xyz_t pos;

        pos.x = aCST_door_x(actorx, def);
        pos.y = player->actor_class.world.position.y;
        pos.z = actorx->world.position.z + 50.0f;
        mPlib_request_main_door_type1(game, &pos, DEG2SHORT_ANGLE(180.0f), TRUE, actorx);
    }
}

static void aCST_actor_init(ACTOR* actorx, GAME* game) {
    const C_StructureDef* def = C_FindStructure(actorx->npc_id);

    if (def == NULL) {
        Actor_delete(actorx);
        return;
    }

    aCST_set_bgOffset(actorx, def);
    actorx->mv_proc = &aCST_actor_move;
    aCST_actor_move(actorx, game);
}

static void aCST_actor_draw(ACTOR* actorx, GAME* game) {
    STRUCTURE_ACTOR* structure = (STRUCTURE_ACTOR*)actorx;
    const C_StructureDef* def = C_FindStructure(actorx->npc_id);
    GRAPH* graph = game->graph;
    const void* model;
    const void* model_xlu;
    Gfx* gfx;

    if (def == NULL) {
        return;
    }

    model = def->model;
    model_xlu = def->model_xlu;

    if (structure->season == mTM_SEASON_WINTER) {
        if (def->winter_model != NULL) {
            model = def->winter_model;
        }

        if (def->winter_model_xlu != NULL) {
            model_xlu = def->winter_model_xlu;
        }
    }

    OPEN_DISP(graph);

    if (model != NULL) {
        _texture_z_light_fog_prim_npc(graph);

        gfx = NOW_POLY_OPA_DISP;
        gSPMatrix(gfx++, _Matrix_to_Mtx_new(graph), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(gfx++, (Gfx*)model);
        SET_POLY_OPA_DISP(gfx);
    }

    if (model_xlu != NULL) {
        _texture_z_light_fog_prim_xlu(graph);

        gfx = NOW_POLY_XLU_DISP;
        gSPMatrix(gfx++, _Matrix_to_Mtx_new(graph), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        gSPDisplayList(gfx++, (Gfx*)model_xlu);
        SET_POLY_XLU_DISP(gfx);
    }

    CLOSE_DISP(graph);
}
