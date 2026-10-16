#ifndef SHUTDOWN_H
#define SHUTDOWN_H

#include "types.h"

#define OS_RESET_RESTART 0
#define OS_RESET_HOTRESET 1
#define OS_RESET_SHUTDOWN 2

#ifdef __cplusplus
extern "C" {
#endif

void osShutdownStart(int shutdown_type);

#ifdef __cplusplus
}
#endif

#endif
