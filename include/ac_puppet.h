#ifndef AC_PUPPET_H
#define AC_PUPPET_H

#include "types.h"
#include "m_actor.h"
#include "m_needlework.h"

#ifdef __cplusplus
extern "C" {
#endif

#define aPUP_FLAG_BOBBER (1 << 0)
#define aPUP_FLAG_SPONGE (1 << 1)

enum {
    aPUP_DESIGN_CLOTH,
    aPUP_DESIGN_UMBRELLA,

    aPUP_DESIGN_NUM
};

// What a puppet shows: where another player is, what they are doing and holding, and their look.
typedef struct puppet_pose_s {
    f32 x;
    f32 y;
    f32 z;
    s16 angle_y;
    s16 anim; // mPlayer_ANIM_*
    f32 frame;
    f32 speed;
    u8 gender; // mPr_SEX_*
    u8 face;   // mPr_FACE_TYPE*
    u16 cloth; // clothing texture index, as Private_c.cloth.idx
    u8 anim_mode; // cKF_FRAMECONTROL_*
    u8 eye;
    u8 mouth;
    u8 flags; // aPUP_FLAG_*
    s_xyz head_angle;
    s16 tilt_z;
    s8 item_kind;  // mPlayer_ITEM_KIND_*, or -1
    s8 item_main;  // mPlayer_ITEM_MAIN_*
    s8 item_shape; // mPlayer_ITEM_DATA_*, or -1
    s8 item_anim;  // mPlayer_ITEM_DATA_*, or -1
    u8 item_mode;  // cKF_FRAMECONTROL_*
    s8 umbrella_state; // aTOL_ACTION_*
    s16 rod_angle_z;
    f32 item_frame;
    f32 item_speed;
    f32 item_scale;
    s_xyz net_angle;
    s16 balloon_lean;
    s_xyz balloon_angle;
    s16 balloon_rot_x;
    xyz_t bobber;
    s16 bobber_angle_x;
    s16 bobber_angle_y;
    u16 held_item; // a picked up item in hand, or EMPTY_NO
    f32 held_scale;
    xyz_t held_offset; // from x, y, z
} aPUP_pose_c;

// A design shown instead of the clothing or umbrella by number: the texture, then its palette.
typedef struct puppet_design_s {
    u8 tex[mNW_DESIGN_TEX_SIZE] ATTRIBUTE_ALIGN(32);
    u16 pal[16] ATTRIBUTE_ALIGN(32);
} aPUP_design_c;

// A player figure that follows a pose set from outside, such as another player's over the network. The actor's arg is
// the slot it follows.
extern ACTOR_PROFILE Puppet_Profile;

// Provided by the puppet API: the pose of a slot, NULL once it is removed, and where its actor is.
extern const aPUP_pose_c* aPUP_GetPose(int slot);
// Counts the pose changes of a slot, so the actor can tell a new pose from one it has followed.
extern u32 aPUP_GetPoseCount(int slot);
extern void aPUP_SetActor(int slot, ACTOR* actor);
// The design a puppet shows instead of its clothing or umbrella by number, or NULL. The actor may be any actor.
extern const aPUP_design_c* aPUP_GetDesign(int slot, int which);
extern const aPUP_design_c* aPUP_GetActorDesign(ACTOR* actor, int which);

#ifdef __cplusplus
}
#endif

#endif
