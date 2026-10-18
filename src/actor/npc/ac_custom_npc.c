#include "ac_custom_npc.h"

#include "m_common_data.h"
#include "m_demo.h"
#include "port/events/Events.h"

static void aCNP_actor_ct(ACTOR* actorx, GAME* game);
static void aCNP_actor_dt(ACTOR* actorx, GAME* game);
static void aCNP_actor_init(ACTOR* actorx, GAME* game);
static void aCNP_actor_move(ACTOR* actorx, GAME* game);
static void aCNP_actor_draw(ACTOR* actorx, GAME* game);
static void aCNP_actor_save(ACTOR* actorx, GAME* game);

// clang-format off
ACTOR_PROFILE Custom_Npc_Profile = {
    0,
    ACTOR_PART_NPC,
    ACTOR_STATE_NONE,
    EMPTY_NO,
    ACTOR_OBJ_BANK_KEEP,
    sizeof(NPC_ACTOR),
    &aCNP_actor_ct,
    &aCNP_actor_dt,
    &aCNP_actor_init,
    mActor_NONE_PROC1,
    &aCNP_actor_save,
};
// clang-format on

static void aCNP_talk_request(ACTOR* actorx, GAME* game);
static int aCNP_talk_init(ACTOR* actorx, GAME* game);
static int aCNP_talk_end_chk(ACTOR* actorx, GAME* game);

static void aCNP_actor_ct(ACTOR* actorx, GAME* game) {
    // clang-format off
    static aNPC_ct_data_c ct_data[aCNP_SCHEDULE_NUM] = {
        { &aCNP_actor_move, &aCNP_actor_draw, aNPC_CT_SCHED_TYPE_STAND, &aCNP_talk_request, &aCNP_talk_init, &aCNP_talk_end_chk, 0 },
        { &aCNP_actor_move, &aCNP_actor_draw, aNPC_CT_SCHED_TYPE_WANDER, &aCNP_talk_request, &aCNP_talk_init, &aCNP_talk_end_chk, 0 },
        { &aCNP_actor_move, &aCNP_actor_draw, aNPC_CT_SCHED_TYPE_WALK_WANDER, &aCNP_talk_request, &aCNP_talk_init, &aCNP_talk_end_chk, 0 },
    };
    // clang-format on
    int schedule = actorx->actor_specific;

    if (schedule < 0 || schedule >= aCNP_SCHEDULE_NUM) {
        schedule = aCNP_SCHEDULE_STAND;
    }

    if (CLIP(npc_clip)->birth_check_proc(actorx, game) == TRUE) {
        CLIP(npc_clip)->ct_proc(actorx, game, &ct_data[schedule]);
    }
}

static void aCNP_actor_dt(ACTOR* actorx, GAME* game) {
    CLIP(npc_clip)->dt_proc(actorx, game);
}

static void aCNP_actor_save(ACTOR* actorx, GAME* game) {
    CLIP(npc_clip)->save_proc(actorx, game);
}

static void aCNP_actor_init(ACTOR* actorx, GAME* game) {
    CLIP(npc_clip)->init_proc(actorx, game);
}

static void aCNP_actor_move(ACTOR* actorx, GAME* game) {
    CLIP(npc_clip)->move_proc(actorx, game);
}

static void aCNP_actor_draw(ACTOR* actorx, GAME* game) {
    CLIP(npc_clip)->draw_proc(actorx, game);
}

static void aCNP_set_talk_info(ACTOR* actorx) {
    CALL_EVENT(CustomNpcTalk, actorx, -1);
    mDemo_Set_msg_num(CustomNpcTalk_.msgNo >= 0 ? CustomNpcTalk_.msgNo : 0);
}

static void aCNP_talk_request(ACTOR* actorx, GAME* game) {
    mDemo_Request(mDemo_TYPE_TALK, actorx, &aCNP_set_talk_info);
}

static int aCNP_talk_init(ACTOR* actorx, GAME* game) {
    mDemo_Set_ListenAble();
    return TRUE;
}

static int aCNP_talk_end_chk(ACTOR* actorx, GAME* game) {
    return mDemo_Check(mDemo_TYPE_TALK, actorx) == FALSE;
}
