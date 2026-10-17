#include "port/Enhancements.h"

#include <libultraship/bridge/consolevariablebridge.h>

extern "C" int AC_ResettiDisabled(void) {
    return CVarGetInteger(CVAR_DISABLE_RESETTI, 0) != 0;
}
