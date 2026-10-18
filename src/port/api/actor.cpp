#ifdef ENABLE_SCRIPTING

#include "port/ShipInit.hpp"
#include "port/events/Events.h"

#define this this_
extern "C" {
#include "port/api/actor.h"
#include "m_camera2.h"
#include "m_demo.h"
#include "m_name_table.h"
}
#undef this

#include <array>

namespace {

constexpr int kMaxCustomActors = 64;

struct CustomActor {
    bool used = false;
    ACTOR_PROFILE profile{};
    ACTOR_DLFTBL table{};
};

std::array<CustomActor, kMaxCustomActors> sActors;
GAME_PLAY* sPlay = nullptr;
int32_t sSpeakMessage = 0;

void SetSpeakInfo(ACTOR* actor) {
    mDemo_Set_msg_num(sSpeakMessage);
    mDemo_Set_talk_display_name(FALSE);
    mDemo_Set_ListenAble();
    mDemo_Set_camera(CAMERA2_PROCESS_NORMAL);
    mDemo_Set_use_zoom_sound(TRUE);
}

CustomActor* FindActor(int16_t profile) {
    const int slot = profile - mAc_PROFILE_NUM;
    if (slot < 0 || slot >= kMaxCustomActors || !sActors[slot].used) {
        return nullptr;
    }
    return &sActors[slot];
}

void DestroyInstances(int16_t profile) {
    if (sPlay == nullptr) {
        return;
    }
    for (int part = 0; part < ACTOR_PART_NUM; part++) {
        for (ACTOR* actor = sPlay->actor_info.list[part].actor; actor != nullptr; actor = actor->next_actor) {
            if (actor->id != profile) {
                continue;
            }
            if (actor->dt_proc != nullptr) {
                actor->dt_proc(actor, (GAME*)sPlay);
            }
            actor->ct_proc = nullptr;
            actor->dt_proc = nullptr;
            actor->sv_proc = nullptr;
            Actor_delete(actor);
        }
    }
}

void InitActorApi() {
    REGISTER_LISTENER(ActorProfileLookup, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        auto* lookup = reinterpret_cast<ActorProfileLookup*>(event);
        if (CustomActor* actor = FindActor(lookup->profile)) {
            lookup->table = &actor->table;
        }
    });
    REGISTER_LISTENER(SceneLoaded, EVENT_PRIORITY_HIGH,
                      [](IEvent* event) { sPlay = reinterpret_cast<SceneLoaded*>(event)->play; });
    REGISTER_LISTENER(SceneUnloaded, EVENT_PRIORITY_LOW, [](IEvent* event) { sPlay = nullptr; });
}

RegisterShipInitFunc sInitActorApi(InitActorApi);

} // namespace

extern "C" int16_t C_RegisterActor(const ACTOR_PROFILE* profile) {
    if (profile == nullptr || profile->part >= ACTOR_PART_NUM || profile->class_size < sizeof(ACTOR)) {
        return -1;
    }
    for (int slot = 0; slot < kMaxCustomActors; slot++) {
        CustomActor& actor = sActors[slot];
        if (actor.used) {
            continue;
        }
        actor.used = true;
        actor.profile = *profile;
        actor.profile.id = (s16)(mAc_PROFILE_NUM + slot);
        actor.table = {};
        actor.table.profile = &actor.profile;
        return actor.profile.id;
    }
    return -1;
}

extern "C" void C_UnregisterActor(int16_t profile) {
    if (CustomActor* actor = FindActor(profile)) {
        DestroyInstances(profile);
        actor->used = false;
    }
}

extern "C" ACTOR* C_SpawnActor(int16_t profile, float x, float y, float z, int16_t angle_y, int16_t arg) {
    if (sPlay == nullptr) {
        return nullptr;
    }
    return Actor_info_make_actor(&sPlay->actor_info, (GAME*)sPlay, profile, x, y, z, 0, angle_y, 0, -1, -1, -1,
                                 EMPTY_NO, arg, -1, -1);
}

extern "C" GAME_PLAY* C_GetPlay(void) {
    return sPlay;
}

extern "C" int C_ActorSpeak(ACTOR* actor, int32_t msg_no) {
    if (actor == nullptr || mDemo_Get_talk_actor() != nullptr) {
        return FALSE;
    }
    sSpeakMessage = msg_no;
    return mDemo_Request(mDemo_TYPE_SPEAK, actor, &SetSpeakInfo);
}

#endif
