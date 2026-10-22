#ifdef ENABLE_SCRIPTING

#include "port/ShipInit.hpp"
#include "port/api/ModOwner.hpp"
#include "port/api/SizedDef.hpp"
#include "port/events/Events.h"
#include "port/interpolation/FrameInterpolation.h"

#define this this_
extern "C" {
#include "port/api/actor.h"
#include "port/api/cloth.h"
#include "port/api/town.h"
#include "port/api/visitor.h"
#include "ac_custom_npc.h"
#include "ac_npc.h"
#include "m_collision_bg.h"
#include "m_common_data.h"
#include "m_field_info.h"
#include "m_npc.h"
#include "m_play.h"
extern mNpc_Default_Data_c npc_def_list[];
}
#undef this

#include <array>
#include <string>
#include <vector>

namespace {

constexpr int kCount = C_VISITOR_LAST + 1 - C_VISITOR_FIRST;
constexpr int kSpawnTries = 300;

struct Visitor {
    bool used = false;
    std::string owner;
    C_VisitorDef def{};
    mNpc_MaskNpc_c mask{};
    bool scheduled = false;
    C_VisitDef visit{};
};

struct SpawnRequest {
    mActor_name_t name;
    int block_x;
    int block_z;
    int ut_x;
    int ut_z;
    int tries;
};

std::array<Visitor, kCount> sVisitors;
std::vector<SpawnRequest> sRequests;
int16_t sProfile = -1;
int sCheckTimer = 0;

Visitor* Find(int name) {
    const int slot = name - C_VISITOR_FIRST;
    return slot >= 0 && slot < kCount && sVisitors[slot].used ? &sVisitors[slot] : nullptr;
}

void SetupMask(mActor_name_t name, Visitor& visitor) {
    mNpc_MaskNpc_c& mask = visitor.mask;
    memset(&mask, 0, sizeof(mask));
    mask.mask_id = name;
    mask.npc_id = visitor.def.look;
    mask.in_use = TRUE;
    mNpc_ClearAnimalInfo(&mask.animal_data);
    mNpc_SetDefAnimal(&mask.animal_data, visitor.def.look, npc_def_list);
    mActor_name_t cloth = visitor.def.cloth;
    if (cloth != EMPTY_NO && !(cloth >= ITM_CLOTH_START && cloth < ITM_CLOTH_END) && C_FindCloth(cloth) == nullptr) {
        cloth = EMPTY_NO;
    }
    mask.cloth_id = cloth != EMPTY_NO ? cloth : mask.animal_data.cloth;
}

bool AcreIsValid(int block_x, int block_z) {
    return block_x >= 1 && block_x <= FG_BLOCK_X_NUM && block_z >= 1 && block_z <= FG_BLOCK_Z_NUM;
}

bool PlayerAcre(int* block_x, int* block_z) {
    GAME_PLAY* play = C_GetPlay();
    ACTOR* player = play != nullptr ? play->actor_info.list[ACTOR_PART_PLAYER].actor : nullptr;
    int ut_x;
    int ut_z;
    return player != nullptr && C_InTown() &&
           mFI_Wpos2BkandUtNuminBlock(block_x, block_z, &ut_x, &ut_z, player->world.position);
}

bool TileIsFree(int block_x, int block_z, int ut_x, int ut_z) {
    if (ut_x < 1 || ut_x > UT_X_NUM - 2 || ut_z < 1 || ut_z > UT_Z_NUM - 2) {
        return false;
    }
    if (Save_Get(fg[block_z - 1][block_x - 1]).items[ut_z][ut_x] != EMPTY_NO) {
        return false;
    }
    const mCoBG_Collision_u* col = mFI_UtNum2UtCol(block_x * UT_X_NUM + ut_x, block_z * UT_Z_NUM + ut_z);
    return col != nullptr && !col->data.slate_flag && col->data.unit_attribute <= mCoBG_ATTRIBUTE_SOIL2;
}

bool FindFreeTile(int block_x, int block_z, int* ut_x, int* ut_z) {
    for (int i = 0; i < 64; i++) {
        const int x = 1 + RANDOM(UT_X_NUM - 2);
        const int z = 1 + RANDOM(UT_Z_NUM - 2);
        if (TileIsFree(block_x, block_z, x, z)) {
            *ut_x = x;
            *ut_z = z;
            return true;
        }
    }
    return false;
}

bool VisitingNow(mActor_name_t name, const C_VisitDef& visit, int* block_x, int* block_z) {
    C_Time now;
    C_GetTime(&now);
    if ((visit.months != 0 && (visit.months & (1 << (now.month - 1))) == 0) ||
        (visit.days != 0 && (visit.days & (1u << (now.day - 1))) == 0) ||
        (visit.weekdays != 0 && (visit.weekdays & (1 << now.weekday)) == 0) ||
        (visit.hours != 0 && (visit.hours & (1u << now.hour)) == 0)) {
        return false;
    }
    if (AcreIsValid(visit.block_x, visit.block_z)) {
        *block_x = visit.block_x;
        *block_z = visit.block_z;
    } else {
        const uint32_t seed = ((uint32_t)now.year * 372 + now.month * 31 + now.day) * 2654435761u + name * 40503u;
        const uint32_t acre = (seed >> 8) % (FG_BLOCK_X_NUM * FG_BLOCK_Z_NUM);
        *block_x = 1 + (int)(acre % FG_BLOCK_X_NUM);
        *block_z = 1 + (int)(acre / FG_BLOCK_X_NUM);
    }
    return true;
}

bool Requested(mActor_name_t name) {
    for (const SpawnRequest& request : sRequests) {
        if (request.name == name) {
            return true;
        }
    }
    return false;
}

void UpdateSchedules() {
    int player_x;
    int player_z;
    if (!PlayerAcre(&player_x, &player_z)) {
        return;
    }
    for (int slot = 0; slot < kCount; slot++) {
        Visitor& visitor = sVisitors[slot];
        if (!visitor.used || !visitor.scheduled) {
            continue;
        }
        const mActor_name_t name = (mActor_name_t)(C_VISITOR_FIRST + slot);
        int block_x;
        int block_z;
        const bool due = VisitingNow(name, visitor.visit, &block_x, &block_z);
        ACTOR* actor = C_FindVisitor(name);
        if (!due) {
            if (actor != nullptr) {
                Actor_delete(actor);
            }
            continue;
        }
        if (actor != nullptr || Requested(name) || block_x != player_x || block_z != player_z) {
            continue;
        }
        int ut_x = visitor.visit.ut_x;
        int ut_z = visitor.visit.ut_z;
        if ((ut_x < 0 || ut_z < 0) && !FindFreeTile(block_x, block_z, &ut_x, &ut_z)) {
            continue;
        }
        sRequests.push_back({ name, block_x, block_z, ut_x, ut_z, 0 });
    }
}

void ProcessRequests() {
    GAME_PLAY* play = C_GetPlay();
    aNPC_Clip_c* clip = Common_Get(clip).npc_clip;
    if (play == nullptr || clip == nullptr || clip->setupActor_proc == nullptr) {
        return;
    }
    std::erase_if(sRequests, [&](SpawnRequest& request) {
        const Visitor* visitor = Find(request.name);
        if (visitor == nullptr || C_FindVisitor(request.name) != nullptr) {
            return true;
        }
        if (clip->setupActor_proc(play, request.name, -1, -1, visitor->def.schedule, request.block_x, request.block_z,
                                  request.ut_x, request.ut_z)) {
            return true;
        }
        return ++request.tries >= kSpawnTries;
    });
}

void InitVisitorApi() {
    sProfile = C_RegisterActor(&Custom_Npc_Profile);
    REGISTER_LISTENER(NpcProfileLookup, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        auto* lookup = reinterpret_cast<NpcProfileLookup*>(event);
        if (Find(lookup->name) != nullptr) {
            lookup->profile = sProfile;
        }
    });
    REGISTER_LISTENER(MaskNpcLookup, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        auto* lookup = reinterpret_cast<MaskNpcLookup*>(event);
        if (Visitor* visitor = Find(lookup->name)) {
            lookup->mask = &visitor->mask;
        }
    });
    REGISTER_LISTENER(CustomNpcTalk, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        auto* talk = reinterpret_cast<CustomNpcTalk*>(event);
        if (Visitor* visitor = Find(talk->actor->npc_id)) {
            if (visitor->def.on_talk != nullptr) {
                ModOwner::Scope scope(visitor->owner);
                talk->msgNo = visitor->def.on_talk(talk->actor);
            } else {
                talk->msgNo = visitor->def.talk_msg;
            }
        }
    });
    REGISTER_LISTENER(PlayUpdate, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        if (++sCheckTimer >= 30) {
            sCheckTimer = 0;
            UpdateSchedules();
        }
        ProcessRequests();
    });
    REGISTER_LISTENER(SceneUnloaded, EVENT_PRIORITY_NORMAL, [](IEvent* event) { sRequests.clear(); });
    ModOwner::AddReleaser([](const std::string& mod) {
        for (int slot = 0; slot < kCount; slot++) {
            if (sVisitors[slot].used && sVisitors[slot].owner == mod) {
                C_UnregisterVisitor((mActor_name_t)(C_VISITOR_FIRST + slot));
            }
        }
    });
}

RegisterShipInitFunc sInitVisitorApi(InitVisitorApi);

} // namespace

extern "C" mActor_name_t C_RegisterVisitorSized(const C_VisitorDef* visitor_def, uint32_t size) {
    const SizedDef<C_VisitorDef> sized(visitor_def, size);
    const C_VisitorDef* def = sized.get();
    if (def == nullptr || ITEM_NAME_GET_TYPE(def->look) != NAME_TYPE_NPC) {
        return EMPTY_NO;
    }
    for (int slot = 0; slot < kCount; slot++) {
        Visitor& visitor = sVisitors[slot];
        if (visitor.used) {
            continue;
        }
        const mActor_name_t name = (mActor_name_t)(C_VISITOR_FIRST + slot);
        visitor = {};
        visitor.used = true;
        visitor.owner = ModOwner::Current();
        visitor.def = *def;
        SetupMask(name, visitor);
        return name;
    }
    return EMPTY_NO;
}

extern "C" void C_UnregisterVisitor(mActor_name_t name) {
    if (Visitor* visitor = Find(name)) {
        C_RemoveVisitor(name);
        std::erase_if(sRequests, [name](const SpawnRequest& request) { return request.name == name; });
        *visitor = {};
    }
}

extern "C" int C_SpawnVisitor(mActor_name_t name, int block_x, int block_z, int ut_x, int ut_z) {
    if (Find(name) == nullptr || !C_InTown() || !AcreIsValid(block_x, block_z) || ut_x < 0 || ut_x >= UT_X_NUM ||
        ut_z < 0 || ut_z >= UT_Z_NUM) {
        return FALSE;
    }
    if (C_FindVisitor(name) == nullptr && !Requested(name)) {
        sRequests.push_back({ name, block_x, block_z, ut_x, ut_z, 0 });
    }
    return TRUE;
}

extern "C" ACTOR* C_FindVisitor(mActor_name_t name) {
    GAME_PLAY* play = C_GetPlay();
    if (play == nullptr || Find(name) == nullptr) {
        return nullptr;
    }
    for (ACTOR* actor = play->actor_info.list[ACTOR_PART_NPC].actor; actor != nullptr; actor = actor->next_actor) {
        if (actor->npc_id == name && (actor->mv_proc != nullptr || actor->dw_proc != nullptr)) {
            return actor;
        }
    }
    return nullptr;
}

extern "C" void C_RemoveVisitor(mActor_name_t name) {
    std::erase_if(sRequests, [name](const SpawnRequest& request) { return request.name == name; });
    if (ACTOR* actor = C_FindVisitor(name)) {
        Actor_delete(actor);
    }
}

extern "C" int C_ScheduleVisitorSized(mActor_name_t name, const C_VisitDef* visit_def, uint32_t size) {
    const SizedDef<C_VisitDef> sized(visit_def, size);
    Visitor* visitor = Find(name);
    if (visitor == nullptr || sized.get() == nullptr) {
        return FALSE;
    }
    visitor->visit = *sized.get();
    visitor->scheduled = true;
    return TRUE;
}

extern "C" void C_UnscheduleVisitor(mActor_name_t name) {
    if (Visitor* visitor = Find(name)) {
        visitor->scheduled = false;
    }
}

extern "C" int C_GetVisitorAcre(mActor_name_t name, int* block_x, int* block_z) {
    Visitor* visitor = Find(name);
    int x;
    int z;
    if (visitor == nullptr || !visitor->scheduled || !VisitingNow(name, visitor->visit, &x, &z)) {
        return FALSE;
    }
    if (block_x != nullptr) {
        *block_x = x;
    }
    if (block_z != nullptr) {
        *block_z = z;
    }
    return TRUE;
}

#endif
