#include "ac_puppet.h"

#include "ac_port.h"
#include "audio.h"
#include "audio_defs.h"
#include "libultra/libultra.h"

#include "m_common_data.h"
#include "m_field_info.h"
#include "m_lib.h"
#include "m_play.h"
#include "m_player.h"
#include "m_player_lib.h"
#include "m_actor_shadow.h"
#include "m_rcp.h"
#include "c_keyframe.h"
#include "jsyswrap.h"
#include "sys_matrix.h"
#include "ac_tools.h"
#include "libforest/gbi_extensions.h"
#include "assets/game/m_player.h"
#include "assets/data/model/boy_model.h"
#include "assets/data/model/girl_model.h"
#include "assets/data/model/tol_uki_12.h"
#include "assets/data/model/tol_uki_22.h"

// Eyes and mouths, then the face palette, as the game keeps a face in its resources.
#define aPUP_FACE_SIZE 0xE20
#define aPUP_FACE_PALETTE 0xE00
#define aPUP_FACE_TEX 0x100
#define aPUP_SNAP_DISTANCE 200.0f
#define aPUP_RESYNC_FRAMES 4.0f
#define aPUP_ITEM_JOINT_NUM 8

#define aPUP_JOINT_LARM2 16
#define aPUP_JOINT_HAND 20
#define aPUP_JOINT_HEAD 24

typedef struct puppet_actor_s {
    ACTOR actor_class;
    cKF_SkeletonInfo_R_c keyframe;
    s_xyz joint_data[mPlayer_JOINT_NUM + 1];
    s_xyz morph_data[mPlayer_JOINT_NUM + 1];
    Mtx work_mtx[2][13];
    cKF_SkeletonInfo_R_c item_keyframe;
    s_xyz item_joint_data[aPUP_ITEM_JOINT_NUM];
    s_xyz item_morph_data[aPUP_ITEM_JOINT_NUM];
    Mtx item_work_mtx[2][4];
    MtxF right_hand_mtx;
    xyz_t right_hand_pos;
    ACTOR* umbrella;
    int umbrella_state;
    int seen_without_umbrella;
    u32 pose_count;
    int slot;
    int gender;
    int face;
    int cloth;
    int anim;
    int item_shape;
    int item_anim;
    u8 face_tex[aPUP_FACE_SIZE] ATTRIBUTE_ALIGN(32);
    u8 cloth_tex[mNW_DESIGN_TEX_SIZE + mNW_PALETTE_SIZE] ATTRIBUTE_ALIGN(32);
} PUPPET_ACTOR;

static void aPUP_actor_ct(ACTOR* actorx, GAME* game);
static void aPUP_actor_dt(ACTOR* actorx, GAME* game);
static void aPUP_actor_move(ACTOR* actorx, GAME* game);
static void aPUP_actor_draw(ACTOR* actorx, GAME* game);

// clang-format off
ACTOR_PROFILE Puppet_Profile = {
    0,
    ACTOR_PART_NPC,
    ACTOR_STATE_NONE,
    EMPTY_NO,
    ACTOR_OBJ_BANK_KEEP,
    sizeof(PUPPET_ACTOR),
    &aPUP_actor_ct,
    &aPUP_actor_dt,
    &aPUP_actor_move,
    &aPUP_actor_draw,
    NULL,
};
// clang-format on

static void aPUP_load_face(PUPPET_ACTOR* puppet, int gender, int face) {
    // As mPlib_Get_UseFaceTexRom_p, without a bee sting or a decoy.
    u32 idx = face + gender * 8;

    _JW_GetResourceAram(JW_GetAramAddress(RESOURCE_FACE_BOY) + idx * aPUP_FACE_SIZE, puppet->face_tex, aPUP_FACE_SIZE);
    AC_TexInvalidateRange(puppet->face_tex, aPUP_FACE_SIZE);
    puppet->gender = gender;
    puppet->face = face;
}

static void aPUP_load_cloth(PUPPET_ACTOR* puppet, int cloth) {
    // Other clothing numbers are designs or modded clothes, which differ between games: those come as a design.
    if (cloth < CLOTH_NUM + 1) {
        _JW_GetResourceAram(mPlib_Get_PlayerTexRom_p(cloth), puppet->cloth_tex, mNW_DESIGN_TEX_SIZE);
        _JW_GetResourceAram(mPlib_Get_PlayerPalletRom_p(cloth), puppet->cloth_tex + mNW_DESIGN_TEX_SIZE,
                            mNW_PALETTE_SIZE);
        AC_TexInvalidateRange(puppet->cloth_tex, sizeof(puppet->cloth_tex));
    }
    puppet->cloth = cloth;
}

static void aPUP_set_skeleton(PUPPET_ACTOR* puppet, int gender) {
    cKF_Skeleton_R_c* skeleton = gender == mPr_SEX_MALE ? &cKF_bs_r_boy_1 : &cKF_bs_r_grl_1;

    cKF_SkeletonInfo_R_ct(&puppet->keyframe, skeleton, NULL, puppet->joint_data, puppet->morph_data);
    puppet->anim = -1;
}

static int aPUP_frame_far(const cKF_FrameControl_c* fc, f32 target) {
    f32 diff = ABS(fc->current_frame - target);

    if (fc->mode == cKF_FRAMECONTROL_REPEAT) {
        f32 length = ABS(fc->end_frame - fc->start_frame);

        if (length > 0.0f && length - diff < diff) {
            diff = length - diff;
        }
    }
    return diff > aPUP_RESYNC_FRAMES;
}

static void aPUP_set_anim(PUPPET_ACTOR* puppet, const aPUP_pose_c* pose) {
    cKF_Animation_R_c* anim = mPlib_Get_Pointer_Animation(pose->anim);

    if (anim == NULL) {
        anim = mPlib_Get_Pointer_Animation(mPlayer_ANIM_WAIT1);
    }
    cKF_SkeletonInfo_R_init_standard_setframeandspeedandmorphandmode(&puppet->keyframe, anim, NULL, pose->frame,
                                                                     pose->speed, 0.0f, pose->anim_mode);
    puppet->anim = pose->anim;
}

static int aPUP_item_has_skeleton(int shape) {
    return mPlayer_ITEM_DATA_VALID(shape) && mPlib_Get_Item_DataPointerType(shape) == mPlayer_ITEM_DATA_TYPE_SKELETON;
}

static void aPUP_set_item(PUPPET_ACTOR* puppet, const aPUP_pose_c* pose, int new_pose) {
    cKF_SkeletonInfo_R_c* kf = &puppet->item_keyframe;

    if (pose->item_shape != puppet->item_shape) {
        puppet->item_shape = pose->item_shape;
        puppet->item_anim = -1;
        if (aPUP_item_has_skeleton(pose->item_shape)) {
            cKF_SkeletonInfo_R_ct(kf, mPlib_Get_Item_DataPointer(pose->item_shape), NULL, puppet->item_joint_data,
                                  puppet->item_morph_data);
        }
    }
    if (!aPUP_item_has_skeleton(puppet->item_shape) || !mPlayer_ITEM_DATA_VALID(pose->item_anim)) {
        return;
    }
    if (pose->item_anim != puppet->item_anim) {
        cKF_SkeletonInfo_R_init_standard_setframeandspeedandmorphandmode(
            kf, mPlib_Get_Item_DataPointer(pose->item_anim), NULL, pose->item_frame, pose->item_speed, 0.0f,
            pose->item_mode);
        puppet->item_anim = pose->item_anim;
        return;
    }
    kf->frame_control.speed = pose->item_speed;
    cKF_SkeletonInfo_R_play(kf);
    if (new_pose && aPUP_frame_far(&kf->frame_control, pose->item_frame)) {
        kf->frame_control.current_frame = pose->item_frame;
    }
}

static void aPUP_set_umbrella(PUPPET_ACTOR* puppet, const aPUP_pose_c* pose, GAME* game) {
    aTOL_Clip_c* tools = Common_Get(clip).tools_clip;
    ACTOR* actorx = &puppet->actor_class;
    int kind = pose->item_kind;
    int want = mPlayer_ITEM_IS_UMBRELLA(kind) && pose->umbrella_state != aTOL_ACTION_DESTRUCT;

    if (tools == NULL) {
        return;
    }
    if (puppet->umbrella != NULL &&
        (!want || ((TOOLS_ACTOR*)puppet->umbrella)->tool_name != kind - mPlayer_ITEM_KIND_UMBRELLA00)) {
        tools->aTOL_chg_request_mode_proc(actorx, puppet->umbrella, aTOL_ACTION_DESTRUCT);
        puppet->umbrella = NULL;
    }
    if (!want) {
        puppet->seen_without_umbrella = TRUE;
        return;
    }
    if (puppet->umbrella == NULL) {
        int state = pose->umbrella_state;

        // An umbrella that was already out before the puppet saw its player starts open, not opening.
        if (!puppet->seen_without_umbrella && (state == aTOL_ACTION_WAIT || state == aTOL_ACTION_TAKEOUT)) {
            state = aTOL_ACTION_S_TAKEOUT;
        }
        puppet->umbrella = tools->aTOL_birth_proc(kind - mPlayer_ITEM_KIND_UMBRELLA00, state, actorx, game, -1, NULL);
        if (puppet->umbrella != NULL) {
            puppet->umbrella->world.position = actorx->world.position;
        }
        puppet->umbrella_state = pose->umbrella_state;
    } else if (pose->umbrella_state != puppet->umbrella_state) {
        tools->aTOL_chg_request_mode_proc(actorx, puppet->umbrella, pose->umbrella_state);
        puppet->umbrella_state = pose->umbrella_state;
    }
}

static void aPUP_actor_ct(ACTOR* actorx, GAME* game) {
    PUPPET_ACTOR* puppet = (PUPPET_ACTOR*)actorx;
    const aPUP_pose_c* pose;

    puppet->slot = actorx->actor_specific;
    pose = aPUP_GetPose(puppet->slot);
    actorx->scale.x = 0.01f;
    actorx->scale.y = 0.01f;
    actorx->scale.z = 0.01f;
    Shape_Info_init(actorx, 0.0f, &mAc_ActorShadowCircle, 18.0f, 18.0f);
    puppet->gender = -1;
    puppet->face = -1;
    puppet->cloth = -1;
    puppet->anim = -1;
    puppet->item_shape = -1;
    puppet->item_anim = -1;
    puppet->umbrella = NULL;
    puppet->seen_without_umbrella = FALSE;
    if (pose != NULL) {
        puppet->pose_count = aPUP_GetPoseCount(puppet->slot);
        aPUP_set_skeleton(puppet, pose->gender);
        aPUP_load_face(puppet, pose->gender, pose->face);
        aPUP_load_cloth(puppet, pose->cloth);
        aPUP_set_anim(puppet, pose);
        actorx->shape_info.rotation.y = pose->angle_y;
        actorx->world.angle.y = pose->angle_y;
    }
    aPUP_SetActor(puppet->slot, actorx);
}

static void aPUP_actor_dt(ACTOR* actorx, GAME* game) {
    PUPPET_ACTOR* puppet = (PUPPET_ACTOR*)actorx;
    aTOL_Clip_c* tools = Common_Get(clip).tools_clip;

    if (puppet->umbrella != NULL && tools != NULL) {
        tools->aTOL_chg_request_mode_proc(actorx, puppet->umbrella, aTOL_ACTION_DESTRUCT);
        puppet->umbrella = NULL;
    }
    aPUP_SetActor(puppet->slot, NULL);
}

static void aPUP_actor_move(ACTOR* actorx, GAME* game) {
    PUPPET_ACTOR* puppet = (PUPPET_ACTOR*)actorx;
    const aPUP_pose_c* pose = aPUP_GetPose(puppet->slot);
    u32 count = aPUP_GetPoseCount(puppet->slot);
    int new_pose = count != puppet->pose_count;
    xyz_t* pos = &actorx->world.position;
    f32 dx;
    f32 dy;
    f32 dz;

    if (pose == NULL) {
        Actor_delete(actorx);
        return;
    }
    puppet->pose_count = count;

    if (pose->gender != puppet->gender) {
        aPUP_set_skeleton(puppet, pose->gender);
        aPUP_load_face(puppet, pose->gender, pose->face);
    } else if (pose->face != puppet->face) {
        aPUP_load_face(puppet, pose->gender, pose->face);
    }
    if (pose->cloth != puppet->cloth) {
        aPUP_load_cloth(puppet, pose->cloth);
    }

    // Poses come about ten times a second; ease towards each one, and jump when it is far off.
    dx = pose->x - pos->x;
    dy = pose->y - pos->y;
    dz = pose->z - pos->z;
    if (dx * dx + dy * dy + dz * dz > aPUP_SNAP_DISTANCE * aPUP_SNAP_DISTANCE) {
        pos->x = pose->x;
        pos->y = pose->y;
        pos->z = pose->z;
    } else {
        pos->x += dx * 0.35f;
        pos->y += dy * 0.35f;
        pos->z += dz * 0.35f;
    }
    actorx->shape_info.rotation.y += (s16)(pose->angle_y - actorx->shape_info.rotation.y) / 3;
    actorx->world.angle.y = actorx->shape_info.rotation.y;

    if (pose->anim != puppet->anim) {
        aPUP_set_anim(puppet, pose);
    } else {
        puppet->keyframe.frame_control.speed = pose->speed;
        cKF_SkeletonInfo_R_play(&puppet->keyframe);
        if (new_pose && aPUP_frame_far(&puppet->keyframe.frame_control, pose->frame)) {
            puppet->keyframe.frame_control.current_frame = pose->frame;
        }
    }
    aPUP_set_item(puppet, pose, new_pose);
    aPUP_set_umbrella(puppet, pose, game);

    // As Player_actor_sound_move_temochi_kazaguruma, the pinwheel whirs while it spins.
    if (pose->item_main == mPlayer_ITEM_MAIN_WINDMILL_NORMAL) {
        f32 level = ABS(puppet->item_keyframe.frame_control.speed / 44.0f);

        if (level > 1.0f) {
            level = 1.0f;
        }
        if (level != 0.0f) {
            sAdo_kazagurumaLevel(level);
            sAdo_OngenPos((uintptr_t)puppet, NA_SE_TEMOCHI_KAZAGURUMA, pos);
        }
    }
}

static int aPUP_draw_before(GAME* game, cKF_SkeletonInfo_R_c* kf, int joint_no, Gfx** gfx_pp, u8* flag, void* arg,
                            s_xyz* rot, xyz_t* pos) {
    PUPPET_ACTOR* puppet = (PUPPET_ACTOR*)arg;
    const aPUP_pose_c* pose = aPUP_GetPose(puppet->slot);

    // As Player_actor_draw_Before_head.
    if (joint_no == aPUP_JOINT_HEAD && pose != NULL &&
        (pose->head_angle.x != 0 || pose->head_angle.y != 0 || pose->head_angle.z != 0)) {
        Matrix_push();
        Matrix_mult(&MtxF_clear, MTX_LOAD);
        Matrix_softcv3_mult(&ZeroVec, rot);
        Matrix_RotateX(pose->head_angle.x, MTX_MULT);
        Matrix_RotateY(pose->head_angle.y, MTX_MULT);
        Matrix_to_rotate2_new(get_Matrix_now(), rot, MTX_LOAD);
        Matrix_pull();
    }
    return TRUE;
}

static int aPUP_draw_after(GAME* game, cKF_SkeletonInfo_R_c* kf, int joint_no, Gfx** gfx_pp, u8* flag, void* arg,
                           s_xyz* rot, xyz_t* pos) {
    PUPPET_ACTOR* puppet = (PUPPET_ACTOR*)arg;

    if (joint_no == aPUP_JOINT_HAND) {
        Matrix_Position_Zero(&puppet->right_hand_pos);
        Matrix_get(&puppet->right_hand_mtx);
    }
    return TRUE;
}

static int aPUP_draw_net_after(GAME* game, cKF_SkeletonInfo_R_c* kf, int joint_no, Gfx** gfx_pp, u8* flag, void* arg,
                               s_xyz* rot, xyz_t* pos) {
    const aPUP_pose_c* pose = aPUP_GetPose(((PUPPET_ACTOR*)arg)->slot);

    if (joint_no == 3 && pose != NULL) {
        Matrix_rotateXYZ(pose->net_angle.x, pose->net_angle.y, pose->net_angle.z, MTX_MULT);
    }
    return TRUE;
}

static int aPUP_draw_balloon_before(GAME* game, cKF_SkeletonInfo_R_c* kf, int joint_no, Gfx** gfx_pp, u8* flag,
                                    void* arg, s_xyz* rot, xyz_t* pos) {
    if (joint_no >= 1 && joint_no <= 3) {
        OPEN_POLY_OPA_DISP(game->graph);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetTexEdgeAlpha(POLY_OPA_DISP++, 80);
        CLOSE_POLY_OPA_DISP(game->graph);
    }
    return TRUE;
}

static int aPUP_draw_balloon_after(GAME* game, cKF_SkeletonInfo_R_c* kf, int joint_no, Gfx** gfx_pp, u8* flag,
                                   void* arg, s_xyz* rot, xyz_t* pos) {
    if (joint_no >= 1 && joint_no <= 3) {
        OPEN_POLY_OPA_DISP(game->graph);
        gDPPipeSync(POLY_OPA_DISP++);
        gDPSetTexEdgeAlpha(POLY_OPA_DISP++, 144);
        CLOSE_POLY_OPA_DISP(game->graph);
    }
    return TRUE;
}

static void aPUP_draw_item_skeleton(PUPPET_ACTOR* puppet, GAME* game, cKF_draw_callback before,
                                    cKF_draw_callback after) {
    if (puppet->item_anim >= 0 && aPUP_item_has_skeleton(puppet->item_shape)) {
        cKF_Si3_draw_R_SV(game, &puppet->item_keyframe, puppet->item_work_mtx[game->frame_counter % 2], before, after,
                          puppet);
    }
}

static void aPUP_draw_balloon(PUPPET_ACTOR* puppet, const aPUP_pose_c* pose, GAME* game) {
    ACTOR* actorx = &puppet->actor_class;
    GRAPH* graph = game->graph;
    f32 scale = pose->item_scale;
    s16 angle = DEG2SHORT_ANGLE2(-90.0f) + pose->balloon_lean + pose->balloon_angle.x + pose->balloon_rot_x;

    // As Player_actor_Item_draw_balloon, with the swing the other game worked out.
    Matrix_push();
    Matrix_translate(puppet->right_hand_pos.x, puppet->right_hand_pos.y, puppet->right_hand_pos.z, MTX_LOAD);
    Matrix_RotateY(actorx->shape_info.rotation.y, MTX_MULT);
    Matrix_RotateX(angle, MTX_MULT);
    Matrix_RotateZ(0x4000, MTX_MULT);
    Matrix_RotateX(pose->balloon_angle.z, MTX_MULT);
    Matrix_scale(actorx->scale.x * scale, actorx->scale.y * scale, actorx->scale.z * scale, MTX_MULT);
    OPEN_POLY_OPA_DISP(graph);
    gSPMatrix(POLY_OPA_DISP++, _Matrix_to_Mtx_new(graph), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    CLOSE_POLY_OPA_DISP(graph);
    Setpos_HiliteReflect_init(&puppet->right_hand_pos, (GAME_PLAY*)game);
    aPUP_draw_item_skeleton(puppet, game, aPUP_draw_balloon_before, aPUP_draw_balloon_after);
    Matrix_pull();
}

static void aPUP_draw_item(PUPPET_ACTOR* puppet, const aPUP_pose_c* pose, GAME* game) {
    GRAPH* graph = game->graph;
    int main = pose->item_main;
    Mtx* mtx;

    if (main == mPlayer_ITEM_MAIN_NONE) {
        if (pose->flags & aPUP_FLAG_SPONGE) {
            _texture_z_light_fog_prim(graph);
            Matrix_push();
            Matrix_put(&puppet->right_hand_mtx);
            mtx = _Matrix_to_Mtx_new(graph);
            if (mtx != NULL) {
                OPEN_POLY_OPA_DISP(graph);
                gSPMatrix(POLY_OPA_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
                gSPDisplayList(POLY_OPA_DISP++, tol_sponge_1_model);
                CLOSE_POLY_OPA_DISP(graph);
            }
            Matrix_pull();
        }
        return;
    }
    if (main <= mPlayer_ITEM_MAIN_NONE || main >= mPlayer_ITEM_MAIN_NUM) {
        return;
    }

    // As Player_actor_Item_draw: everything is held in the right hand.
    Matrix_push();
    Matrix_put(&puppet->right_hand_mtx);
    if (pose->item_scale != 1.0f) {
        Matrix_scale(pose->item_scale, pose->item_scale, pose->item_scale, MTX_MULT);
    }
    mtx = _Matrix_to_Mtx_new(graph);
    if (mtx != NULL) {
        _texture_z_light_fog_prim(graph);
        OPEN_POLY_OPA_DISP(graph);
        gSPMatrix(POLY_OPA_DISP++, mtx, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
        CLOSE_POLY_OPA_DISP(graph);

        if (main == mPlayer_ITEM_MAIN_UMBRELLA_NORMAL) {
            if (puppet->umbrella != NULL) {
                TOOLS_ACTOR* umbrella = (TOOLS_ACTOR*)puppet->umbrella;

                Matrix_get(&umbrella->matrix_work);
                umbrella->init_matrix = TRUE;
            }
        } else if (main >= mPlayer_ITEM_MAIN_NET_NORMAL && main <= mPlayer_ITEM_MAIN_NET_COMPLETE_COLLECTION) {
            aPUP_draw_item_skeleton(puppet, game, NULL, aPUP_draw_net_after);
        } else if (main >= mPlayer_ITEM_MAIN_ROD_NORMAL && main <= mPlayer_ITEM_MAIN_ROD_PUTAWAY) {
            Matrix_push();
            Matrix_RotateZ(pose->rod_angle_z, MTX_MULT);
            OPEN_POLY_OPA_DISP(graph);
            gSPMatrix(POLY_OPA_DISP++, _Matrix_to_Mtx_new(graph), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
            CLOSE_POLY_OPA_DISP(graph);
            aPUP_draw_item_skeleton(puppet, game, NULL, NULL);
            Matrix_pull();
        } else if (main == mPlayer_ITEM_MAIN_BALLOON_NORMAL) {
            aPUP_draw_balloon(puppet, pose, game);
        } else if (main == mPlayer_ITEM_MAIN_WINDMILL_NORMAL) {
            aPUP_draw_item_skeleton(puppet, game, NULL, NULL);
        } else if (mPlayer_ITEM_DATA_VALID(pose->item_shape) &&
                   mPlib_Get_Item_DataPointerType(pose->item_shape) == mPlayer_ITEM_DATA_TYPE_GFX) {
            // The axe, the shovel and the fan are a single model.
            OPEN_POLY_OPA_DISP(graph);
            gSPDisplayList(POLY_OPA_DISP++, mPlib_Get_Item_DataPointer(pose->item_shape));
            CLOSE_POLY_OPA_DISP(graph);
        }
    }
    Matrix_pull();
}

static void aPUP_draw_held(PUPPET_ACTOR* puppet, const aPUP_pose_c* pose, GAME* game) {
    xyz_t pos;

    if (pose->held_item == EMPTY_NO || pose->held_scale <= 0.0f) {
        return;
    }
    pos.x = puppet->actor_class.world.position.x + pose->held_offset.x;
    pos.y = puppet->actor_class.world.position.y + pose->held_offset.y;
    pos.z = puppet->actor_class.world.position.z + pose->held_offset.z;
    if (mFI_GET_TYPE(mFI_GetFieldId()) == mFI_FIELD_FG) {
        if (Common_Get(clip).bg_item_clip != NULL && Common_Get(clip).bg_item_clip->single_draw_proc != NULL) {
            Common_Get(clip).bg_item_clip->single_draw_proc(game, pose->held_item, &pos, pose->held_scale, NULL, NULL,
                                                            NULL);
        }
    } else if (Common_Get(clip).shop_goods_clip != NULL && Common_Get(clip).shop_goods_clip->single_draw_proc != NULL) {
        Common_Get(clip).shop_goods_clip->single_draw_proc(game, pose->held_item, &pos, pose->held_scale, 0, FALSE);
    }
}

static void aPUP_draw_bobber(const aPUP_pose_c* pose, GAME* game) {
    GRAPH* graph = game->graph;

    if (!(pose->flags & aPUP_FLAG_BOBBER)) {
        return;
    }
    Matrix_push();
    Matrix_translate(pose->bobber.x, pose->bobber.y, pose->bobber.z, MTX_LOAD);
    Matrix_RotateY(pose->bobber_angle_y, MTX_MULT);
    Matrix_RotateX(pose->bobber_angle_x, MTX_MULT);
    Matrix_scale(0.01f, 0.01f, 0.01f, MTX_MULT);
    _texture_z_light_fog_prim(graph);
    OPEN_POLY_OPA_DISP(graph);
    gSPMatrix(POLY_OPA_DISP++, _Matrix_to_Mtx_new(graph), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPSetPrimColor(POLY_OPA_DISP++, 0, 128, 255, 255, 255, 255);
    gSPDisplayList(POLY_OPA_DISP++, pose->item_kind == mPlayer_ITEM_KIND_ROD ? tol_uki1_model : tol_uki2_model);
    CLOSE_POLY_OPA_DISP(graph);
    Matrix_pull();
}

static void aPUP_actor_draw(ACTOR* actorx, GAME* game) {
    PUPPET_ACTOR* puppet = (PUPPET_ACTOR*)actorx;
    const aPUP_pose_c* pose = aPUP_GetPose(puppet->slot);
    const aPUP_design_c* cloth = aPUP_GetDesign(puppet->slot, aPUP_DESIGN_CLOTH);
    GRAPH* graph = game->graph;
    Mtx* mtx = puppet->work_mtx[game->frame_counter % 2];
    int eye;
    int mouth;

    if (puppet->anim < 0 || pose == NULL) {
        return;
    }
    eye = pose->eye < mPlayer_EYE_TEX_NUM ? pose->eye : 0;
    mouth = pose->mouth < mPlayer_MOUTH_TEX_NUM ? pose->mouth : 0;

    _texture_z_light_fog_prim(graph);
    OPEN_POLY_OPA_DISP(graph);
    gSPSegment(POLY_OPA_DISP++, ANIME_1_TXT_SEG, puppet->face_tex + eye * aPUP_FACE_TEX);
    gSPSegment(POLY_OPA_DISP++, ANIME_2_TXT_SEG, puppet->face_tex + (mPlayer_EYE_TEX_NUM + mouth) * aPUP_FACE_TEX);
    if (cloth != NULL) {
        gSPSegment(POLY_OPA_DISP++, ANIME_3_TXT_SEG, cloth->tex);
        gSPSegment(POLY_OPA_DISP++, ANIME_4_TXT_SEG, cloth->pal);
    } else {
        gSPSegment(POLY_OPA_DISP++, ANIME_3_TXT_SEG, puppet->cloth_tex);
        gSPSegment(POLY_OPA_DISP++, ANIME_4_TXT_SEG, puppet->cloth_tex + mNW_DESIGN_TEX_SIZE);
    }
    gSPSegment(POLY_OPA_DISP++, ANIME_5_TXT_SEG, puppet->face_tex + aPUP_FACE_PALETTE);
    CLOSE_POLY_OPA_DISP(graph);

    if (pose->tilt_z != 0) {
        Matrix_push();
        Matrix_RotateZ(pose->tilt_z, MTX_MULT);
    }
    cKF_Si3_draw_R_SV(game, &puppet->keyframe, mtx, aPUP_draw_before, aPUP_draw_after, puppet);
    aPUP_draw_item(puppet, pose, game);
    aPUP_draw_held(puppet, pose, game);
    if (pose->tilt_z != 0) {
        Matrix_pull();
    }
    aPUP_draw_bobber(pose, game);
}
