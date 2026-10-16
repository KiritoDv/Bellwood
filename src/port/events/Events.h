#pragma once

#include <libultraship/bridge/eventsbridge.h>

#include "list/EngineEvent.h"
#include "list/ActorEvent.h"
#include "list/GameEvent.h"
#include "list/SaveEvent.h"

#ifdef __cplusplus
extern "C" {
#endif

void PortEvents_Init(void);

#ifdef __cplusplus
}
#endif
