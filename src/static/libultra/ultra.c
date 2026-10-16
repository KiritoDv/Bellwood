#include "libultra/libultra.h"
#include "_mem.h"
#include "gc_compat.h"

s32 osAppNMIBuffer[16];

void osSyncPrintf(const char* format, ...) {
    va_list arg;
    va_start(arg, format);
    OSVReport(format, arg);
    va_end(arg);

}
