#include "libultra/libultra.h"
#include "libultra/osThread.h"
#include "libultra/shutdown.h"
#include "boot.h"
#include "gc_compat.h"
#include "ac_port.h"
#include "libultra/initialize.h"
#include "port/save/SaveManager.h"

#define AC_GRAPH_THREAD_ID 4

static char l_copy_date[] = "acport " __DATE__ " " __TIME__;

void* boot_copyDate = l_copy_date;

s32 gAC_PrerenderFb;

OSId osGetThreadId(OSThread* thread) {
    return AC_GRAPH_THREAD_ID;
}

static int l_reboot_requested;

void osShutdownStart(int shutdown_type) {
    OSReport("osShutdownStart(%d): rebooting\n", shutdown_type);
    l_reboot_requested = TRUE;
}

void AC_RequestReset(void) {
    if (osShutdown == FALSE) {
        osShutdown = TRUE;
        __osShutdownTime = osGetTime();
    }
}

void AC_ResetTick(void) {
    if (SaveManager_IsSaving()) {
        __osShutdownTime = osGetTime();
        return;
    }

    if (osShutdown != FALSE && l_reboot_requested == FALSE) {
        u64 wait = osIsEnableShutdown() ? OS_USEC_TO_CYCLES(3000000) : OS_USEC_TO_CYCLES(30000000);
        OSTime since = osIsEnableShutdown() ? __osShutdownTime : osGetDisableShutdownTime();

        if (osGetTime() - since > wait) {
            osShutdownStart(OS_RESET_RESTART);
        }
    }
}

int AC_TakeRebootRequest(void) {
    int requested = l_reboot_requested;

    l_reboot_requested = FALSE;
    return requested;
}

void HotResetIplMenu(void) {
    OSReport("HotResetIplMenu ignored by the port\n");
}
