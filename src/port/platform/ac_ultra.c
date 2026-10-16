#include "libultra/libultra.h"
#include "libultra/osThread.h"
#include "libultra/shutdown.h"
#include "boot.h"
#include "gc_compat.h"
#include "ac_port.h"

#define AC_GRAPH_THREAD_ID 4

static char l_copy_date[] = "acport " __DATE__ " " __TIME__;

void* boot_copyDate = l_copy_date;

s32 gAC_PrerenderFb;

OSId osGetThreadId(OSThread* thread) {
    return AC_GRAPH_THREAD_ID;
}

void osShutdownStart(int shutdown_type) {
    OSReport("osShutdownStart(%d) ignored by the port\n", shutdown_type);
}

void HotResetIplMenu(void) {
    OSReport("HotResetIplMenu ignored by the port\n");
}
