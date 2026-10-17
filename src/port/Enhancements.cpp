#include "port/Enhancements.h"
#include "port/ShipInit.hpp"
#include "port/events/Events.h"

#include <libultraship/bridge/consolevariablebridge.h>

namespace {

void CancelWhenEnabled(IEvent* event, const char* cvar) {
    if (CVarGetInteger(cvar, 0) != 0) {
        event->Cancelled = true;
    }
}

void InitEnhancements() {
    REGISTER_LISTENER(ResetDetected, EVENT_PRIORITY_NORMAL,
                      [](IEvent* event) { CancelWhenEnabled(event, CVAR_DISABLE_RESETTI); });
    REGISTER_LISTENER(NewGameWelcome, EVENT_PRIORITY_NORMAL,
                      [](IEvent* event) { CancelWhenEnabled(event, CVAR_SKIP_TRAIN_INTRO); });
    REGISTER_LISTENER(TrainRideStart, EVENT_PRIORITY_NORMAL,
                      [](IEvent* event) { CancelWhenEnabled(event, CVAR_SKIP_TRAIN_INTRO); });
}

RegisterShipInitFunc sInitEnhancements(InitEnhancements);

} // namespace
