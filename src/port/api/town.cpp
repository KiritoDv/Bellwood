#ifdef ENABLE_SCRIPTING

#include "port/events/Events.h"
#include "port/interpolation/FrameInterpolation.h"

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
