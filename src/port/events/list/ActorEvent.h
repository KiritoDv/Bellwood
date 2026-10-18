#pragma once

#include <stdint.h>

struct actor_s;
struct actor_dlftbl_s;

// Actor lifecycle

// Fires when an actor is spawned with a profile number past the built-in table.
// Set table to the profile's table entry to make the spawn succeed.
DEFINE_EVENT(ActorProfileLookup,
    int16_t profile;               // requested profile number
    struct actor_dlftbl_s* table;  // set by the listener that owns the profile
);

// Fires before an actor is allocated. Cancellable.
// Write through the pointers to spawn something else or somewhere else.
// Cancel — the actor is not created and the spawn returns NULL.
// Fires when an NPC is set up whose special NPC name the game has no actor for. Set profile to spawn it.
DEFINE_EVENT(NpcProfileLookup,
    uint16_t name;
    int16_t profile;  // set by the listener that owns the name, -1 leaves it unspawned
);

// Fires when the game looks for the stand-in data of a special NPC name it has no mask NPC for.
// Set mask to a mNpc_MaskNpc_c to give the name a villager's look.
DEFINE_EVENT(MaskNpcLookup,
    uint16_t name;
    void* mask;
);

// Fires when the player talks to an NPC spawned through port/api/visitor.h. Set msgNo to the message to show.
DEFINE_EVENT(CustomNpcTalk,
    struct actor_s* actor;
    int32_t msgNo;
);

DEFINE_EVENT(ActorSpawn,
    int16_t* profile;  // mutable actor profile number
    float* pos;        // mutable world position, x/y/z
    int16_t* rot;      // mutable rotation, x/y/z
    uint16_t* name;    // mutable actor name id
    int16_t* arg;      // mutable spawn argument
);

// Fires after an actor's constructor has run, once its data bank is ready.
DEFINE_EVENT(ActorInit,
    struct actor_s* actor;
);

// Fires every frame before an actor's update function. Cancellable.
// Cancel — the actor does not update this frame.
DEFINE_EVENT(ActorUpdate,
    struct actor_s* actor;
);

// Fires every frame before an actor is drawn. Cancellable.
// Cancel — the actor and its shadow are not drawn this frame.
DEFINE_EVENT(ActorDraw,
    struct actor_s* actor;
);

// Fires when an actor is destroyed, before its save and destructor functions run.
DEFINE_EVENT(ActorDestroy,
    struct actor_s* actor;
);
