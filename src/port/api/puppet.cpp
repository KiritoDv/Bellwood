#ifdef ENABLE_SCRIPTING

#include "port/ShipInit.hpp"
#include "port/api/ModOwner.hpp"
#include "port/api/SizedDef.hpp"
#include "port/events/Events.h"

#define this this_
extern "C" {
#include "port/api/actor.h"
#include "port/api/item.h"
#include "port/api/puppet.h"
#include "ac_port.h"
#include "ac_puppet.h"
#include "ac_tools.h"
#include "ac_uki.h"
#include "audio.h"
#include "ef_effect_control.h"
#include "jsyswrap.h"
#include "m_common_data.h"
#include "m_name_table.h"
#include "m_play.h"
#include "m_player.h"
#include "m_player_lib.h"
#include "m_private.h"
}
#undef this

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <string>

static_assert(sizeof(C_PlayerPose) == sizeof(aPUP_pose_c), "C_PlayerPose and aPUP_pose_c share a layout");
static_assert(offsetof(C_PlayerPose, head_angle) == offsetof(aPUP_pose_c, head_angle));
static_assert(offsetof(C_PlayerPose, item_frame) == offsetof(aPUP_pose_c, item_frame));
static_assert(offsetof(C_PlayerPose, held_offset) == offsetof(aPUP_pose_c, held_offset));
static_assert(sizeof(C_Design::tex) == mNW_DESIGN_TEX_SIZE && sizeof(C_Design::pal) == mNW_PALETTE_SIZE);

namespace {

struct Puppet {
    bool used = false;
    bool posed = false;
    std::string owner;
    aPUP_pose_c pose{};
    uint32_t poseCount = 0;
    std::array<aPUP_design_c, aPUP_DESIGN_NUM> designs{};
    std::array<bool, aPUP_DESIGN_NUM> hasDesign{};
    ACTOR* actor = nullptr;
};

std::array<Puppet, C_PUPPET_MAX> sPuppets;
int16_t sProfile = -1;

Puppet* Find(int puppet) {
    return puppet >= 0 && puppet < C_PUPPET_MAX && sPuppets[puppet].used ? &sPuppets[puppet] : nullptr;
}

void SpawnMissing() {
    if (C_GetPlay() == nullptr || sProfile < 0) {
        return;
    }
    for (int slot = 0; slot < C_PUPPET_MAX; slot++) {
        Puppet& puppet = sPuppets[slot];
        if (puppet.used && puppet.posed && puppet.actor == nullptr) {
            ACTOR* actor = C_SpawnActor(sProfile, puppet.pose.x, puppet.pose.y, puppet.pose.z, puppet.pose.angle_y,
                                        (int16_t)slot);
            if (actor != nullptr) {
                puppet.actor = actor;
            }
        }
    }
}

void InitPuppetApi() {
    sProfile = C_RegisterActor(&Puppet_Profile);
    REGISTER_LISTENER(PlayUpdate, EVENT_PRIORITY_NORMAL, [](IEvent* event) { SpawnMissing(); });
    REGISTER_LISTENER(SceneUnloaded, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        for (Puppet& puppet : sPuppets) {
            puppet.actor = nullptr;
        }
    });
    ModOwner::AddReleaser([](const std::string& mod) {
        for (int slot = 0; slot < C_PUPPET_MAX; slot++) {
            if (sPuppets[slot].used && sPuppets[slot].owner == mod) {
                C_RemovePuppet(slot);
            }
        }
    });
}

RegisterShipInitFunc sInitPuppetApi(InitPuppetApi);

bool IsBoatIndex(int index) {
    return index == mPlayer_INDEX_DEMO_GETON_BOAT_SITDOWN || index == mPlayer_INDEX_DEMO_GETON_BOAT_WAIT ||
           index == mPlayer_INDEX_DEMO_GETON_BOAT_WADE || index == mPlayer_INDEX_DEMO_GETOFF_BOAT_STANDUP;
}

// As Player_actor_draw_Normal tilts the player in the boat.
int16_t BoatTilt(PLAYER_ACTOR* player, int index) {
    if (index == mPlayer_INDEX_DEMO_GETON_BOAT_SITDOWN) {
        return -player->main_data.demo_geton_boat_sitdown.angle_z;
    }
    if (index == mPlayer_INDEX_DEMO_GETOFF_BOAT_STANDUP) {
        return -player->main_data.demo_getoff_boat_standup.angle_z;
    }
    return IsBoatIndex(index) ? -player->boat_angleZ : 0;
}

// As Player_actor_draw_Normal finds an item being picked up or dug up.
void HeldItem(PLAYER_ACTOR* player, int index, C_PlayerPose& out) {
    mActor_name_t item = EMPTY_NO;
    float scale = 0.0f;
    const xyz_t* pos = nullptr;

    if (index == mPlayer_INDEX_PICKUP && !player->main_data.pickup.signboard_flag) {
        item = player->main_data.pickup.item;
        scale = player->main_data.pickup.scale;
        pos = &player->main_data.pickup.item_pos;
    } else if (index == mPlayer_INDEX_PICKUP_JUMP && !player->main_data.pickup_jump.ftr_flag) {
        item = player->main_data.pickup_jump.item;
        scale = player->main_data.pickup_jump.scale;
        pos = &player->main_data.pickup_jump.item_pos;
    } else if (index == mPlayer_INDEX_GET_SCOOP) {
        item = player->main_data.get_scoop.item;
        scale = player->main_data.get_scoop.scale;
        pos = player->keyframe0.frame_control.current_frame <= 42.0f ? &player->scoop_pos : &player->left_hand_pos;
    } else if (index == mPlayer_INDEX_PUTAWAY_SCOOP) {
        item = player->main_data.putaway_scoop.item;
        scale = player->main_data.putaway_scoop.scale;
        pos = &player->left_hand_pos;
    }
    out.held_item = EMPTY_NO;
    if (item != EMPTY_NO && scale > 0.0f && pos != nullptr) {
        out.held_item = item;
        out.held_scale = scale;
        out.held_offset[0] = pos->x - player->actor_class.world.position.x;
        out.held_offset[1] = pos->y - player->actor_class.world.position.y;
        out.held_offset[2] = pos->z - player->actor_class.world.position.z;
    }
}

void CopyAngle(int16_t* out, const s_xyz& angle) {
    out[0] = angle.x;
    out[1] = angle.y;
    out[2] = angle.z;
}

void ReadDesign(uintptr_t tex, uintptr_t pal, bool aram, C_Design* design) {
    if (aram) {
        _JW_GetResourceAram(tex, design->tex, sizeof(design->tex));
        _JW_GetResourceAram(pal, reinterpret_cast<u8*>(design->pal), sizeof(design->pal));
    } else {
        memcpy(design->tex, reinterpret_cast<const void*>(tex), sizeof(design->tex));
        memcpy(design->pal, reinterpret_cast<const void*>(pal), sizeof(design->pal));
    }
}

} // namespace

extern "C" const aPUP_pose_c* aPUP_GetPose(int slot) {
    Puppet* puppet = Find(slot);
    return puppet != nullptr && puppet->posed ? &puppet->pose : nullptr;
}

extern "C" u32 aPUP_GetPoseCount(int slot) {
    Puppet* puppet = Find(slot);
    return puppet != nullptr ? puppet->poseCount : 0;
}

extern "C" void aPUP_SetActor(int slot, ACTOR* actor) {
    if (slot >= 0 && slot < C_PUPPET_MAX) {
        sPuppets[slot].actor = actor;
    }
}

extern "C" const aPUP_design_c* aPUP_GetDesign(int slot, int which) {
    Puppet* puppet = Find(slot);
    if (puppet == nullptr || which < 0 || which >= aPUP_DESIGN_NUM || !puppet->hasDesign[which]) {
        return nullptr;
    }
    return &puppet->designs[which];
}

extern "C" const aPUP_design_c* aPUP_GetActorDesign(ACTOR* actor, int which) {
    for (int slot = 0; slot < C_PUPPET_MAX; slot++) {
        if (actor != nullptr && sPuppets[slot].actor == actor) {
            return aPUP_GetDesign(slot, which);
        }
    }
    return nullptr;
}

extern "C" int C_GetPlayerPoseSized(C_PlayerPose* pose, uint32_t size) {
    GAME_PLAY* play = C_GetPlay();
    Private_c* priv = Common_Get(now_private);
    if (play == nullptr || pose == nullptr || priv == nullptr) {
        return FALSE;
    }
    PLAYER_ACTOR* player = GET_PLAYER_ACTOR(play);
    if (player == nullptr) {
        return FALSE;
    }
    const int index = mPlib_get_player_actor_main_index((GAME*)play);
    const int bank = player->item_bank_idx & 1;
    C_PlayerPose out{};
    out.x = player->actor_class.world.position.x;
    out.y = player->actor_class.world.position.y;
    out.z = player->actor_class.world.position.z;
    out.angle_y = player->actor_class.shape_info.rotation.y;
    out.anim = (int16_t)player->animation0_idx;
    out.frame = player->keyframe0.frame_control.current_frame;
    out.speed = player->keyframe0.frame_control.speed;
    out.anim_mode = (uint8_t)player->keyframe0.frame_control.mode;
    out.gender = (uint8_t)priv->gender;
    out.face = (uint8_t)priv->face;
    out.cloth = priv->cloth.idx;
    out.eye = (uint8_t)player->eye_tex_idx;
    out.mouth = (uint8_t)player->mouth_tex_idx;
    CopyAngle(out.head_angle, player->head_angle);
    out.tilt_z = BoatTilt(player, index);

    out.item_kind = player->item_kind;
    out.item_main = (int8_t)player->now_item_main_index;
    out.item_shape = (int8_t)player->item_shape_type[bank];
    out.item_anim = (int8_t)player->item_animation_idx[bank];
    out.item_mode = (uint8_t)player->item_keyframe.frame_control.mode;
    out.item_frame = player->item_keyframe.frame_control.current_frame;
    out.item_speed = player->item_keyframe.frame_control.speed;
    out.item_scale = player->item_scale;
    out.umbrella_state = player->umbrella_actor != nullptr ? ((TOOLS_ACTOR*)player->umbrella_actor)->work0
                                                            : aTOL_ACTION_DESTRUCT;
    out.rod_angle_z = player->item_rod_angle_z;
    CopyAngle(out.net_angle, player->net_angle);
    out.balloon_lean = player->balloon_lean_angle;
    CopyAngle(out.balloon_angle, player->balloon_angle);
    out.balloon_rot_x = player->ballon_add_rot_x;

    if (player->now_item_main_index == mPlayer_ITEM_MAIN_NONE && index == mPlayer_INDEX_WASH_CAR) {
        out.flags |= C_POSE_SPONGE;
    }
    if (mPlayer_ITEM_IS_ROD(player->item_kind)) {
        UKI_ACTOR* uki = (UKI_ACTOR*)Actor_info_name_search(&play->actor_info, mAc_PROFILE_UKI, ACTOR_PART_BG);
        if (uki != nullptr) {
            out.flags |= C_POSE_BOBBER;
            out.bobber[0] = uki->actor_class.world.position.x;
            out.bobber[1] = uki->actor_class.world.position.y;
            out.bobber[2] = uki->actor_class.world.position.z;
            out.bobber_angle_x = uki->uki_angle.x;
            out.bobber_angle_y = uki->uki_angle.y;
        }
    }
    HeldItem(player, index, out);

    memcpy(pose, &out, std::min<size_t>(size, sizeof(out)));
    return TRUE;
}

extern "C" int C_GetPlayerDesign(int which, C_Design* design) {
    Private_c* priv = Common_Get(now_private);
    if (C_GetPlay() == nullptr || priv == nullptr || design == nullptr) {
        return FALSE;
    }
    if (which == C_DESIGN_CLOTH) {
        const int cloth = priv->cloth.idx;
        if (cloth < CLOTH_NUM + 1) {
            return FALSE;
        }
        ReadDesign(mPlib_Get_PlayerTexRom_p(cloth), mPlib_Get_PlayerPalletRom_p(cloth),
                   mPlib_Check_PlayerClothInAram(cloth), design);
        return TRUE;
    }
    if (which == C_DESIGN_UMBRELLA) {
        const mActor_name_t item = priv->equipment;
        if (const C_ItemDef* custom = C_IsCustomUmbrella(item) ? C_FindItem(item) : nullptr) {
            memcpy(design->tex, custom->umbrella_tex, sizeof(design->tex));
            memcpy(design->pal, custom->umbrella_pal, sizeof(design->pal));
            return TRUE;
        }
        if (item >= ITM_MY_ORG_UMBRELLA0 && item <= ITM_MY_ORG_UMBRELLA7) {
            const int idx = item - ITM_MY_ORG_UMBRELLA0;
            ReadDesign((uintptr_t)Player_Design_Get(idx), (uintptr_t)mNW_PaletteIdx2Palette(Player_Palette_Get(idx)),
                       false, design);
            return TRUE;
        }
    }
    return FALSE;
}

extern "C" int C_SpawnPuppet(void) {
    for (int slot = 0; slot < C_PUPPET_MAX; slot++) {
        // A removed puppet's actor is still deleting itself; its slot is free once it is gone.
        if (!sPuppets[slot].used && sPuppets[slot].actor == nullptr) {
            sPuppets[slot].used = true;
            sPuppets[slot].posed = false;
            sPuppets[slot].hasDesign.fill(false);
            sPuppets[slot].owner = ModOwner::Current();
            return slot;
        }
    }
    return -1;
}

extern "C" void C_SetPuppetPoseSized(int puppet, const C_PlayerPose* pose, uint32_t size) {
    Puppet* target = Find(puppet);
    const SizedDef<C_PlayerPose> sized(pose, size);
    if (target == nullptr || sized.get() == nullptr) {
        return;
    }
    aPUP_pose_c in;
    memcpy(&in, sized.get(), sizeof(in));
    if (in.anim < 0 || in.anim >= mPlayer_ANIM_NUM) {
        in.anim = mPlayer_ANIM_WAIT1;
    }
    in.gender = in.gender == mPr_SEX_FEMALE ? mPr_SEX_FEMALE : mPr_SEX_MALE;
    in.face = in.face < mPr_FACE_TYPE_NUM ? in.face : 0;
    in.anim_mode = in.anim_mode == cKF_FRAMECONTROL_STOP ? cKF_FRAMECONTROL_STOP : cKF_FRAMECONTROL_REPEAT;
    in.item_mode = in.item_mode == cKF_FRAMECONTROL_STOP ? cKF_FRAMECONTROL_STOP : cKF_FRAMECONTROL_REPEAT;
    if (!mPlayer_ITEM_DATA_VALID(in.item_shape)) {
        in.item_shape = -1;
    }
    if (!mPlayer_ITEM_DATA_VALID(in.item_anim)) {
        in.item_anim = -1;
    }
    if (in.item_kind < -1 || in.item_kind >= mPlayer_ITEM_KIND_NUM) {
        in.item_kind = -1;
    }
    if (in.umbrella_state < 0 || in.umbrella_state >= aTOL_ACTION_NUM) {
        in.umbrella_state = aTOL_ACTION_DESTRUCT;
    }
    if (!target->posed || memcmp(&in, &target->pose, sizeof(in)) != 0) {
        target->pose = in;
        target->poseCount++;
    }
    target->posed = true;
}

extern "C" void C_SetPuppetDesign(int puppet, int which, const C_Design* design) {
    Puppet* target = Find(puppet);
    if (target == nullptr || which < 0 || which >= aPUP_DESIGN_NUM) {
        return;
    }
    if (design == nullptr) {
        target->hasDesign[which] = false;
        return;
    }
    aPUP_design_c& out = target->designs[which];
    memcpy(out.tex, design->tex, sizeof(out.tex));
    memcpy(out.pal, design->pal, sizeof(out.pal));
    AC_TexInvalidateRange(out.tex, sizeof(out.tex));
    AC_TexInvalidateRange(out.pal, sizeof(out.pal));
    target->hasDesign[which] = true;
}

extern "C" void C_PuppetPlaySound(int puppet, int kind, uint16_t id, float dx, float dy, float dz) {
    Puppet* target = Find(puppet);
    if (target == nullptr || target->actor == nullptr) {
        return;
    }
    const xyz_t& base = target->actor->world.position;
    const xyz_t pos = { base.x + dx, base.y + dy, base.z + dz };
    switch (kind) {
        case PLAYER_SOUND_STEP:
            sAdo_PlyWalkSe(id, &pos);
            break;
        case PLAYER_SOUND_ROOM_STEP:
            sAdo_PlyWalkSeRoom((u8)id, &pos);
            break;
        default:
            sAdo_OngenTrgStart(id, &pos);
            break;
    }
}

extern "C" void C_PuppetMakeEffect(int puppet, int effect, float dx, float dy, float dz, int prio, int16_t angle,
                                   uint16_t item, int16_t arg0, int16_t arg1) {
    Puppet* target = Find(puppet);
    GAME_PLAY* play = C_GetPlay();
    if (target == nullptr || target->actor == nullptr || play == nullptr || eEC_CLIP == nullptr ||
        effect < 0 || effect >= eEC_EFFECT_NUM || effect == eEC_EFFECT_YAJIRUSHI) {
        return;
    }
    const xyz_t& base = target->actor->world.position;
    const xyz_t pos = { base.x + dx, base.y + dy, base.z + dz };
    // Some effects, such as digging and the net, place themselves from where the player is: for the call, the player
    // stands where the puppet is.
    ACTOR* player = (ACTOR*)GET_PLAYER_ACTOR(play);
    xyz_t saved{};
    if (player != nullptr) {
        saved = player->world.position;
        player->world.position = base;
    }
    eEC_CLIP->effect_make_proc(effect, pos, prio, angle, (GAME*)play, item, arg0, arg1);
    if (player != nullptr) {
        player->world.position = saved;
    }
}

extern "C" void C_RemovePuppet(int puppet) {
    if (Puppet* target = Find(puppet)) {
        // The actor deletes itself on its next move once it has no pose.
        target->used = false;
        target->posed = false;
        target->owner.clear();
    }
}

extern "C" ACTOR* C_GetPuppetActor(int puppet) {
    Puppet* target = Find(puppet);
    return target != nullptr ? target->actor : nullptr;
}

#else
#include "port/interpolation/FrameInterpolation.h"

#define this this_
extern "C" {
#include "ac_puppet.h"
}
#undef this

extern "C" const aPUP_pose_c* aPUP_GetPose(int slot) {
    return nullptr;
}

extern "C" u32 aPUP_GetPoseCount(int slot) {
    return 0;
}

extern "C" void aPUP_SetActor(int slot, ACTOR* actor) {
}

extern "C" const aPUP_design_c* aPUP_GetDesign(int slot, int which) {
    return nullptr;
}

extern "C" const aPUP_design_c* aPUP_GetActorDesign(ACTOR* actor, int which) {
    return nullptr;
}

#endif
