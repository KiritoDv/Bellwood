#define INIT_EVENT_IDS
#include "Events.h"

extern "C" void PortEvents_Init(void) {
    REGISTER_EVENT(EngineReady);
    REGISTER_EVENT(KeyboardInput);
}
