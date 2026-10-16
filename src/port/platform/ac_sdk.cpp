#include <libultraship.h>
#include <libultraship/bridge/controllerbridge.h>
#include <ship/controller/controldeck/ControlDeck.h>
#include <ship/controller/controldevice/controller/Controller.h>

#include <chrono>
#include <cstring>
#include <thread>

extern "C" {
#include "gc_compat.h"
#include "ac_port.h"
}

static int sInterruptLevel = 1;
static u32 sSoundMode = OS_SOUND_MODE_STEREO;
static u32 sMotorState[4];

extern "C" {

void DCFlushRange(void* addr, u32 nbytes) {
    AC_TexInvalidateRange(addr, nbytes);
}

void DCFlushRangeNoSync(void* addr, u32 nbytes) {
    AC_TexInvalidateRange(addr, nbytes);
}

void DCStoreRange(void* addr, u32 nbytes) {
    AC_TexInvalidateRange(addr, nbytes);
}

void DCStoreRangeNoSync(void* addr, u32 nbytes) {
    AC_TexInvalidateRange(addr, nbytes);
}

void DCInvalidateRange(void* addr, u32 nbytes) {
}

void DCTouchRange(void* addr, u32 nbytes) {
}

void DCZeroRange(void* addr, u32 nbytes) {
    if (addr != nullptr) {
        memset(addr, 0, nbytes);
    }
}

BOOL OSDisableInterrupts(void) {
    int prev = sInterruptLevel;
    sInterruptLevel = 0;
    return prev;
}

BOOL OSRestoreInterrupts(BOOL level) {
    int prev = sInterruptLevel;
    sInterruptLevel = level;
    return prev;
}

s32 OSCheckActiveThreads(void) {
    return 1;
}

u32 OSGetConsoleType(void) {
    return 0x00000001;
}

u32 OSGetSoundMode(void) {
    return sSoundMode;
}

void OSSetSoundMode(u32 mode) {
    sSoundMode = mode;
}

void VIWaitForRetrace(void) {
    using Clock = std::chrono::steady_clock;
    constexpr auto kField = std::chrono::nanoseconds(1000000000 / 60);
    static Clock::time_point sNextField;

    Clock::time_point now = Clock::now();
    if (sNextField <= now || sNextField - now > kField) {
        sNextField = now + kField;
    }
    std::this_thread::sleep_until(sNextField);
    sNextField += kField;
}

void VISetBlack(BOOL black) {
}

void VIFlush(void) {
}

void VIConfigurePan(u16 x_origin, u16 y_origin, u16 width, u16 height) {
}

void PADControlMotor(s32 chan, u32 command) {
    if (chan < 0 || chan >= 4 || sMotorState[chan] == command) {
        return;
    }
    sMotorState[chan] = command;
    auto deck = ControllerGetControlDeck();
    if (deck == nullptr) {
        return;
    }
    auto controller = deck->GetControllerByPort((uint8_t)chan);
    if (controller == nullptr) {
        return;
    }
    if (command == PAD_MOTOR_RUMBLE) {
        controller->GetRumble()->StartRumble();
    } else {
        controller->GetRumble()->StopRumble();
    }
}

void PADControlAllMotors(const u32* commands) {
    for (int i = 0; i < 4; i++) {
        PADControlMotor(i, commands[i]);
    }
}

DVDDiskID* DVDGetCurrentDiskID(void) {
    static DVDDiskID sDiskId = { { 'G', 'A', 'F', 'E' }, { '0', '1' }, 0, 0, 0, 0, { 0 } };
    return &sDiskId;
}

BOOL DVDCheckDisk(void) {
    return TRUE;
}
}
