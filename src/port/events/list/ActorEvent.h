#pragma once

#include <stdint.h>

struct actor_s;

// Actor lifecycle

// Fires before an actor is allocated. Cancellable.
// Write through the pointers to spawn something else or somewhere else.
// Cancel — the actor is not created and the spawn returns NULL.
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
