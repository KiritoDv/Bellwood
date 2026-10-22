#pragma once

#include <stdint.h>

#include "api.h"
#include "m_actor.h"
#include "m_name_table.h"

#define C_VISITOR_FIRST 0xDF00
#define C_VISITOR_LAST 0xDF3F

enum {
    C_VISITOR_STAND,       // stays where it spawned
    C_VISITOR_WANDER,      // turns and looks around near its spot
    C_VISITOR_WALK_WANDER, // walks around the acre
};

typedef struct C_VisitorDef_s {
    mActor_name_t look;                // villager id, built-in or custom, whose model, name, voice and personality it has
    mActor_name_t cloth;               // shirt, built-in or custom, EMPTY_NO for the look's
    uint8_t schedule;                  // C_VISITOR_*
    int32_t talk_msg;                  // message shown when the player talks to it
    int32_t (*on_talk)(ACTOR* visitor); // called when the player talks to it, returns the message to show; NULL for
                                       // talk_msg
} C_VisitorDef;

// Registers a visitor: a character with a villager's look who doesn't live in town. Returns its name
// (C_VISITOR_FIRST to C_VISITOR_LAST), or EMPTY_NO. A custom villager makes a good look for a visitor of its own.
#define C_RegisterVisitor(def) C_RegisterVisitorSized((def), sizeof(*(def)))
BW_API mActor_name_t C_RegisterVisitorSized(const C_VisitorDef* def, uint32_t size);
// Unregisters a visitor and removes it from the scene.
BW_API void C_UnregisterVisitor(mActor_name_t visitor);
// Puts the visitor in town at a tile of an acre (block_x 1 to 5, block_z 1 to 6, tiles 0 to 15). It appears once its
// clothes are loaded, a few frames later, and leaves with the acre. Returns FALSE outside of town.
BW_API int C_SpawnVisitor(mActor_name_t visitor, int block_x, int block_z, int ut_x, int ut_z);
// The visitor's actor, or NULL when it isn't in the scene.
BW_API ACTOR* C_FindVisitor(mActor_name_t visitor);
BW_API void C_RemoveVisitor(mActor_name_t visitor);

typedef struct C_VisitDef_s {
    uint16_t months;  // bit 0 = January ... bit 11 = December, 0 for every month
    uint32_t days;    // bit 0 = the 1st ... bit 30 = the 31st, 0 for every day
    uint8_t weekdays; // bit 0 = Sunday ... bit 6 = Saturday, 0 for every day of the week
    uint32_t hours;   // bit 0 = midnight ... bit 23 = 11 PM, 0 for all day
    int8_t block_x;   // acre it is in, 1 to 5 and 1 to 6, or 0 for a different acre each day
    int8_t block_z;
    int8_t ut_x;      // tile it stands on, -1 for a free one
    int8_t ut_z;
} C_VisitDef;

// Makes the visitor come to town at the times the visit allows. It is in its acre whenever the player goes there
// then. A visitor has one schedule; scheduling it again replaces it.
#define C_ScheduleVisitor(visitor, def) C_ScheduleVisitorSized((visitor), (def), sizeof(*(def)))
BW_API int C_ScheduleVisitorSized(mActor_name_t visitor, const C_VisitDef* def, uint32_t size);
BW_API void C_UnscheduleVisitor(mActor_name_t visitor);
// The acre a scheduled visitor is in now, FALSE when it isn't visiting at this time.
BW_API int C_GetVisitorAcre(mActor_name_t visitor, int* block_x, int* block_z);
