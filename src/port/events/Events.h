#pragma once

#ifndef __cplusplus
#include <stdbool.h>
#endif

#include <libultraship/bridge/eventsbridge.h>

#include "list/EngineEvent.h"
#include "list/ActorEvent.h"
#include "list/FurnitureEvent.h"
#include "list/ItemEvent.h"
#include "list/InteriorEvent.h"
#include "list/CreatureEvent.h"
#include "list/ClothEvent.h"
#include "list/VillagerEvent.h"
#include "list/StructureEvent.h"
#include "list/ShopEvent.h"
#include "list/PlayerEvent.h"
#include "list/GameEvent.h"
#include "list/SaveEvent.h"
#include "list/AudioEvent.h"
#include "list/RenderEvent.h"

#ifdef __cplusplus
extern "C" {
#endif

void PortEvents_Init(void);
void PortEvents_WatchReset(void);
void PortEvents_Watch(struct game_play_s* play);

#ifdef __cplusplus
}
#endif
