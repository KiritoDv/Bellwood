#ifndef AC_CUSTOM_NPC_H
#define AC_CUSTOM_NPC_H

#include "types.h"
#include "m_actor.h"
#include "ac_npc.h"

#ifdef __cplusplus
extern "C" {
#endif

enum {
    aCNP_SCHEDULE_STAND,
    aCNP_SCHEDULE_WANDER,
    aCNP_SCHEDULE_WALK_WANDER,

    aCNP_SCHEDULE_NUM
};

extern ACTOR_PROFILE Custom_Npc_Profile;

#ifdef __cplusplus
}
#endif

#endif
