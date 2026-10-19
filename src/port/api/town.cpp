#ifdef ENABLE_SCRIPTING

#include "port/api/ModOwner.hpp"
#include "port/events/Events.h"
#include "port/interpolation/FrameInterpolation.h"
#include "port/ShipInit.hpp"

#define this this_
extern "C" {
#include "port/api/actor.h"
#include "port/api/town.h"
#include "m_common_data.h"
#include "m_field_info.h"
#include "m_npc.h"
#include "m_scene_table.h"
}
#undef this

#include <algorithm>
#include <cstring>

static int32_t sTimeOffset = 0;
static std::string sTimeOffsetOwner;

static void ApplyTimeOffset(IEvent* event) {
    auto* read = reinterpret_cast<ClockRead*>(event);
    if (sTimeOffset > 0) {
        lbRTC_Add_ss(read->time, sTimeOffset);
    } else if (sTimeOffset < 0) {
        lbRTC_Sub_ss(read->time, -sTimeOffset);
    }
}

static RegisterShipInitFunc sTownInit([] {
    REGISTER_LISTENER(ClockRead, EVENT_PRIORITY_NORMAL, ApplyTimeOffset);
    ModOwner::AddReleaser([](const std::string& mod) {
        if (sTimeOffsetOwner == mod) {
            sTimeOffset = 0;
            sTimeOffsetOwner.clear();
        }
    });
});

extern "C" void C_SetTimeOffset(int32_t seconds) {
    sTimeOffset = seconds;
    sTimeOffsetOwner = seconds != 0 ? ModOwner::Current() : std::string();
}

extern "C" int32_t C_GetTimeOffset(void) {
    return sTimeOffset;
}

extern "C" int C_GetScene(void) {
    return C_GetPlay() != nullptr ? Save_Get(scene_no) : -1;
}

extern "C" int C_InTown(void) {
    return C_GetPlay() != nullptr && mFI_CheckFieldData() == TRUE && mFI_GET_TYPE(mFI_GetFieldId()) == mFI_FIELD_FG &&
           Save_Get(scene_no) == SCENE_FG;
}

extern "C" void C_GetTimeSized(C_Time* time, uint32_t size) {
    if (time == nullptr) {
        return;
    }
    const lbRTC_time_c& now = Common_Get(time.rtc_time);
    C_Time out{};
    out.year = now.year;
    out.month = now.month;
    out.day = now.day;
    out.weekday = now.weekday;
    out.hour = now.hour;
    out.minute = now.min;
    out.second = now.sec;
    memcpy(time, &out, std::min<size_t>(size, sizeof(out)));
}

extern "C" int C_GetTownVillagers(mActor_name_t* ids, int capacity) {
    Animal_c* animals = Save_Get(animals);
    int count = 0;
    for (int i = 0; i < ANIMAL_NUM_MAX; i++) {
        if (mNpc_CheckFreeAnimalInfo(&animals[i])) {
            continue;
        }
        if (ids != nullptr && count < capacity) {
            ids[count] = animals[i].id.npc_id;
        }
        count++;
    }
    return count;
}

#endif
