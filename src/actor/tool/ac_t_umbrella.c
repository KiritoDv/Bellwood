#include "ac_t_umbrella.h"

#include "ac_puppet.h"

#include "m_name_table.h"
#include "sys_matrix.h"
#include "m_lib.h"
#include "m_rcp.h"
#include "m_common_data.h"
#include "port/api/item.h"
#include "assets/data/model/tol_umb_01.h"
#include "assets/data/model/tol_umb_02.h"
#include "assets/data/model/tol_umb_03.h"
#include "assets/data/model/tol_umb_04.h"
#include "assets/data/model/tol_umb_05.h"
#include "assets/data/model/tol_umb_06.h"
#include "assets/data/model/tol_umb_07.h"
#include "assets/data/model/tol_umb_08.h"
#include "assets/data/model/tol_umb_09.h"
#include "assets/data/model/tol_umb_10.h"
#include "assets/data/model/tol_umb_11.h"
#include "assets/data/model/tol_umb_12.h"
#include "assets/data/model/tol_umb_13.h"
#include "assets/data/model/tol_umb_14.h"
#include "assets/data/model/tol_umb_15.h"
#include "assets/data/model/tol_umb_16.h"
#include "assets/data/model/tol_umb_17.h"
#include "assets/data/model/tol_umb_18.h"
#include "assets/data/model/tol_umb_19.h"
#include "assets/data/model/tol_umb_20.h"
#include "assets/data/model/tol_umb_21.h"
#include "assets/data/model/tol_umb_22.h"
#include "assets/data/model/tol_umb_23.h"
#include "assets/data/model/tol_umb_24.h"
#include "assets/data/model/tol_umb_25.h"
#include "assets/data/model/tol_umb_26.h"
#include "assets/data/model/tol_umb_27.h"
#include "assets/data/model/tol_umb_28.h"
#include "assets/data/model/tol_umb_29.h"
#include "assets/data/model/tol_umb_30.h"
#include "assets/data/model/tol_umb_31.h"
#include "assets/data/model/tol_umb_32.h"
#include "assets/data/model/tol_umb_w.h"

enum {
    aTUMB_ACTION_TAKEOUT_BEFORE,
    aTUMB_ACTION_OPENING,  // Placeholder. Unknown use.
    aTUMB_ACTION_DELETED2, // Placeholder. Unknown use.
    aTUMB_ACTION_DESTRUCT,
    aTUMB_ACTION_DELETED3, // Placeholder. Unknown use.
    aTUMB_ACTION_DELETED4  // Placeholder. Unknown use.
};

static void aTUMB_actor_ct(ACTOR* actor, GAME* game);
static void aTUMB_actor_move(ACTOR* actor, GAME* game);
static void aTUMB_actor_draw(ACTOR* actor, GAME* game);

ACTOR_PROFILE T_Umbrella_Profile = {
    mAc_PROFILE_T_UMBRELLA,
    ACTOR_PART_BG,
    ACTOR_STATE_NO_DRAW_WHILE_CULLED | ACTOR_STATE_NO_MOVE_WHILE_CULLED,
    EMPTY_NO,
    ACTOR_OBJ_BANK_UMBRELLA,
    sizeof(UMBRELLA_ACTOR),
    &aTUMB_actor_ct,
    NONE_ACTOR_PROC,
    &aTUMB_actor_move,
    &aTUMB_actor_draw,
    NULL,
};

static void aTUMB_setupAction(UMBRELLA_ACTOR* umbrella, int action);

static void aTUMB_actor_ct(ACTOR* actor, GAME* game) {
    UMBRELLA_ACTOR* umbrella = (UMBRELLA_ACTOR*)actor;
    aTUMB_setupAction(umbrella, umbrella->tools_class.work0);
}

static void aTUMB_OngenTrgStart_sub(ACTOR* actor, u16 id) {
    sAdo_OngenTrgStart(id, &actor->world.position);
}

static void aTUMB_OngenTrgStart(ACTOR* actor, int idx) {
    switch (idx) {
        case 1:
            aTUMB_OngenTrgStart_sub(actor, 0x139);
            break;
        case 2:
            aTUMB_OngenTrgStart_sub(actor, 0x10E);
            break;
    }
}

static void aTUMB_calc_model_scale_sub(xyz_t* scale, UMBRELLA_ACTOR* umbrella, int idx) {
    static f32 e_open_sect_table[6] = { 0.0f, 7.0f, 11.0f, 18.0f, 22.0f, 26.0f };
    static f32 e_open_scale_table[12] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 1.0f, 0.5f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };

    static f32 kasa_open_sect_table[4] = { 0.0f, 15.0f, 22.0f, 26.0f };
    static f32 kasa_open_scale_table[8] = { 3.0f, 0.15f, 3.0f, 0.15f, 1.0f, 1.0f, 0.9, 1.0f };

    static UMBRELLA_DATA_TABLE open_data_table[2] = { { 6, e_open_sect_table, e_open_scale_table },
                                                      { 4, kasa_open_sect_table, kasa_open_scale_table } };

    static f32 e_close_sect_table[6] = { 0.0f, 4.0f, 12.0f, 15.0f, 22.0f, 30.0f };
    static f32 e_close_scale_table[12] = { 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 1.0f, 0.5f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f };

    static f32 kasa_close_sect_table[4] = { 0.0f, 4.0f, 12.0f, 30.0f };
    static f32 kasa_close_scale_table[8] = { 1.0f, 1.0f, 1.2f, 1.0f, 3.0f, 0.15f, 3.0f, 0.15f };

    static UMBRELLA_DATA_TABLE close_data_table[2] = { { 6, e_close_sect_table, e_close_scale_table },
                                                       { 4, kasa_close_sect_table, kasa_close_scale_table } };

    static UMBRELLA_DATA_TABLE* data_table[6] = { open_data_table,  open_data_table, close_data_table,
                                                  close_data_table, open_data_table, NULL };

    f32 modifier_percentage;
    f32 base_scale_x;
    f32 base_scale_y;
    f32 max_scale_y;
    f32 max_scale_x;

    int scale_idx_offset;
    f32* scale_table;
    int sector_idx;
    int frame;
    int corresponding_sector;
    UMBRELLA_DATA_TABLE* table;
    f32* sect_table;

    corresponding_sector = 0;
    table = data_table[umbrella->action] + idx;
    sector_idx = table->table_length - 1;
    frame = (int)umbrella->frame;
    sect_table = table->sect_table + 1;

    for (sector_idx; sector_idx > 0; sector_idx--) {
        if (frame <= (int)sect_table[0]) {
            frame -= (int)sect_table[-1];
            break;
        }

        sect_table++;
        corresponding_sector++;
    }

    scale_idx_offset = corresponding_sector * 2;
    scale_table = table->scale_table;
    base_scale_x = scale_table[scale_idx_offset];
    modifier_percentage = (f32)frame / (sect_table[0] - sect_table[-1]);
    base_scale_y = scale_table[scale_idx_offset + 1];
    max_scale_y = scale_table[scale_idx_offset + 3];
    max_scale_x = scale_table[scale_idx_offset + 2];

    scale->x = base_scale_x + modifier_percentage * (max_scale_x - base_scale_x);
    scale->y = base_scale_y + modifier_percentage * (max_scale_y - base_scale_y);
    scale->z = scale->y;
}

static void aTUMB_calc_model_scale(UMBRELLA_ACTOR* umbrella) {
    aTUMB_calc_model_scale_sub(&umbrella->scale_e, umbrella, 0);
    aTUMB_calc_model_scale_sub(&umbrella->scale_kasa, umbrella, 1);
}

static void aTUMB_anime_proc(UMBRELLA_ACTOR* umbrella) {
    static f32 max_anm[6] = { 0.0f, 26.0f, 30.0f, 30.0f, 26.0f, 0.0f };

    f32 max_anm_for_action;
    f32 next_frame;
    int action;

    action = umbrella->action;
    max_anm_for_action = max_anm[action];
    next_frame = umbrella->frame;
    next_frame += 0.5f;

    if (next_frame >= max_anm_for_action) {
        next_frame = max_anm_for_action;
    }

    if (action == aTUMB_ACTION_OPENING) {
        umbrella->opened_fully = next_frame == max_anm_for_action;
    }

    umbrella->frame = next_frame;
    aTUMB_calc_model_scale(umbrella);
}

static void aTUMB_takeout_before(ACTOR* actor) {
    UMBRELLA_ACTOR* umbrella = (UMBRELLA_ACTOR*)actor;
    if (umbrella->tools_class.unk1BC == TRUE) {
        aTUMB_setupAction(umbrella, aTUMB_ACTION_OPENING);
    }
}

static void aTUMB_destruct(ACTOR* actor) {
    Actor_delete(actor);
}

void aTUMB_setupAction(UMBRELLA_ACTOR* umbrella, int action) {
    static UMBRELLA_PROC process[] = { aTUMB_takeout_before, (UMBRELLA_PROC)none_proc1, (UMBRELLA_PROC)none_proc1,
                                       aTUMB_destruct,       (UMBRELLA_PROC)none_proc1, NULL };

    f32 frame;

    umbrella->action_proc = process[action];
    umbrella->action = action;
    umbrella->tools_class.work0 = action;

    aTUMB_OngenTrgStart(&umbrella->tools_class.actor_class, action);

    if (action == aTUMB_ACTION_DELETED3) {
        umbrella->opened_fully = TRUE;
        frame = 26.0f;
    } else {
        frame = 0.0f;
    }

    umbrella->frame = frame;
}

static void aTUMB_actor_move(ACTOR* actor, GAME* game) {
    UMBRELLA_ACTOR* umbrella;
    int action;

    umbrella = (UMBRELLA_ACTOR*)actor;
    action = umbrella->tools_class.work0;

    if (action != umbrella->action) {
        aTUMB_setupAction(umbrella, action);
    }

    if (umbrella->action != aTUMB_ACTION_DESTRUCT) {
        aTUMB_anime_proc(umbrella);
    }

    umbrella->action_proc(actor);
}

static void aTUMB_actor_draw(ACTOR* actor, GAME* game) {
    static UMBRELLA_MODEL draw_dt[40] = {
        { e_umb01_model, kasa_umb01_model }, { e_umb02_model, kasa_umb02_model }, { e_umb03_model, kasa_umb03_model },
        { e_umb04_model, kasa_umb04_model }, { e_umb05_model, kasa_umb05_model }, { e_umb06_model, kasa_umb06_model },
        { e_umb07_model, kasa_umb07_model }, { e_umb08_model, kasa_umb08_model }, { e_umb09_model, kasa_umb09_model },
        { e_umb10_model, kasa_umb10_model }, { e_umb11_model, kasa_umb11_model }, { e_umb12_model, kasa_umb12_model },
        { e_umb13_model, kasa_umb13_model }, { e_umb14_model, kasa_umb14_model }, { e_umb15_model, kasa_umb15_model },
        { e_umb16_model, kasa_umb16_model }, { e_umb17_model, kasa_umb17_model }, { e_umb18_model, kasa_umb18_model },
        { e_umb19_model, kasa_umb19_model }, { e_umb20_model, kasa_umb20_model }, { e_umb21_model, kasa_umb21_model },
        { e_umb22_model, kasa_umb22_model }, { e_umb23_model, kasa_umb23_model }, { e_umb24_model, kasa_umb24_model },
        { e_umb25_model, kasa_umb25_model }, { e_umb26_model, kasa_umb26_model }, { e_umb27_model, kasa_umb27_model },
        { e_umb28_model, kasa_umb28_model }, { e_umb29_model, kasa_umb29_model }, { e_umb30_model, kasa_umb30_model },
        { e_umb31_model, kasa_umb31_model }, { e_umb32_model, kasa_umb32_model }, { e_umb_w_model, kasa_umb_w_model },
        { e_umb_w_model, kasa_umb_w_model }, { e_umb_w_model, kasa_umb_w_model }, { e_umb_w_model, kasa_umb_w_model },
        { e_umb_w_model, kasa_umb_w_model }, { e_umb_w_model, kasa_umb_w_model }, { e_umb_w_model, kasa_umb_w_model },
        { e_umb_w_model, kasa_umb_w_model },
    };

    UMBRELLA_ACTOR* umbrella;
    GRAPH* graph;
    int umbrella_design_index;
    int umbrella_name;
    int tool_name;
    UMBRELLA_MODEL* umbrella_model;
    ACTOR* parent;
    Gfx* gfx;

    umbrella = (UMBRELLA_ACTOR*)actor;
    parent = actor->parent_actor;
    tool_name = umbrella->tools_class.tool_name;
    graph = game->graph;
    umbrella_model = &draw_dt[tool_name];

    if (parent->drawn == FALSE || umbrella->tools_class.init_matrix == FALSE) {
        return;
    }

    Matrix_put(&umbrella->tools_class.matrix_work);
    Matrix_Position_Zero(&umbrella->tools_class.actor_class.world.position);

    umbrella->tools_class.init_matrix = FALSE;
    umbrella->tools_class.unk1BC = TRUE;

    _texture_z_light_fog_prim_npc(graph);

    OPEN_DISP(graph);
    gfx = NOW_POLY_OPA_DISP;

    Matrix_rotateXYZ(0, -0x4000, 0, MTX_MULT);
    Matrix_scale(umbrella->scale_e.x, umbrella->scale_e.y, umbrella->scale_e.z, MTX_MULT);

    gSPMatrix(gfx++, _Matrix_to_Mtx_new(graph), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(gfx++, umbrella_model->model_e);

    Matrix_translate(4500.0f, 0.0f, 0.0f, MTX_MULT);
    Matrix_scale(umbrella->scale_kasa.x, umbrella->scale_kasa.y, umbrella->scale_kasa.z, MTX_MULT);

    gSPMatrix(gfx++, _Matrix_to_Mtx_new(graph), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);

    if (parent->part == ACTOR_PART_PLAYER && C_IsCustomUmbrella(Now_Private->equipment)) {
        const C_ItemDef* custom = C_FindItem(Now_Private->equipment);

        gSPSegment(gfx++, G_MWO_SEGMENT_8, custom->umbrella_pal);
        gSPSegment(gfx++, G_MWO_SEGMENT_9, custom->umbrella_tex);
    } else if (tool_name >= TOOL_ORG_UMBRELLA0) {
        const aPUP_design_c* design = aPUP_GetActorDesign(parent, aPUP_DESIGN_UMBRELLA);

        umbrella_name = tool_name - TOOL_ORG_UMBRELLA0;
        if (design != NULL) {
            gSPSegment(gfx++, G_MWO_SEGMENT_8, design->pal);
            gSPSegment(gfx++, G_MWO_SEGMENT_9, design->tex);
        } else if (parent->part == ACTOR_PART_PLAYER) {
            umbrella_name = (umbrella_name & 7);
            gSPSegment(gfx++, G_MWO_SEGMENT_8, mNW_PaletteIdx2Palette(Player_Palette_Get(umbrella_name)));
            gSPSegment(gfx++, G_MWO_SEGMENT_9, Player_Design_Get(umbrella_name));
        } else {
            umbrella_name = umbrella_name & 3;
            gSPSegment(gfx++, G_MWO_SEGMENT_8,
                       mNW_PaletteIdx2Palette(Able_Sisters_Umbrella_Palette_Get(umbrella_name)));
            gSPSegment(gfx++, G_MWO_SEGMENT_9, Able_Sisters_Umbrella_Design_Get(umbrella_name));
        }
    }

    gSPDisplayList(gfx++, umbrella_model->model_kasa);
    SET_POLY_OPA_DISP(gfx);
    CLOSE_DISP(graph);
}
