#include "padmgr.h"
#include "libultra/libultra.h"
#include "libultra/osContPad.h"
#include "ac_port.h"

#include <math.h>

#define AC_PAD_STICK_RANGE 85.0f

extern u8 __osMaxControllers;

s32 osContStartQuery(OSMesgQueue* mq) {
    osSendMesg(mq, OS_MESG_PTR(NULL), OS_MESG_NOBLOCK);
    return 0;
}

void osContGetQuery(OSContStatus* status) {
    int i;

    for (i = 0; i < MAXCONTROLLERS; i++) {
        if (i == 0) {
            status[i].type = CONT_TYPE_NORMAL;
            status[i].status = 0;
            status[i].err_no = CONT_NO_ERROR;
        } else {
            status[i].type = 0;
            status[i].status = 0;
            status[i].err_no = CONT_NO_RESPONSE_ERROR;
        }
    }
}

s32 osContSetCh(u8 num_controllers) {
    __osMaxControllers = num_controllers <= MAXCONTROLLERS ? num_controllers : MAXCONTROLLERS;
    return 0;
}

f32 AC_PadGetSubStickValue(void) {
    const OSContPad* pad = &padmgr_class.pads[0].now;
    f32 x = pad->right_stick_x / AC_PAD_STICK_RANGE;
    f32 y = pad->right_stick_y / AC_PAD_STICK_RANGE;
    f32 value = sqrtf(x * x + y * y);

    return value > 1.0f ? 1.0f : value;
}

s16 AC_PadGetSubStickAngle(void) {
    const OSContPad* pad = &padmgr_class.pads[0].now;

    return (s16)(atan2f((f32)pad->right_stick_x, (f32)-pad->right_stick_y) * (32768.0f / (f32)M_PI));
}
