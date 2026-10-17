#pragma once

#include <libultraship/libultra/controller.h>

typedef enum BellwoodGameState {
    BW_GAME_STATE_NONE = -1,
    BW_GAME_STATE_FIRST,
    BW_GAME_STATE_SELECT,
    BW_GAME_STATE_PLAY,
    BW_GAME_STATE_SECOND,
    BW_GAME_STATE_UNUSED,
    BW_GAME_STATE_TRADEMARK,
    BW_GAME_STATE_PLAYER_SELECT,
    BW_GAME_STATE_SAVE_MENU,
    BW_GAME_STATE_FAMICOM_EMU,
    BW_GAME_STATE_PRENMI,
} BellwoodGameState;

// Engine lifecycle

// Fires once after the engine, the resource manager and the GUI are initialised.
DEFINE_EVENT(EngineReady);

// Fires when a keyboard key is pressed (desktop only).
DEFINE_EVENT(KeyboardInput,
    uint32_t scancode;  // platform scancode of the pressed key
);

// Fires every main-loop iteration, before the game frame runs.
DEFINE_EVENT(GameLoopTick);

// Fires once per game logic frame, before the current game state runs.
DEFINE_EVENT(GameFrameUpdate);

// Fires after a new game state has been constructed.
DEFINE_EVENT(GameStateChanged,
    int32_t prev;  // previous BellwoodGameState, BW_GAME_STATE_NONE on boot
    int32_t now;   // new BellwoodGameState
);

// Fires when the game is soft reset or rebooted, before the running state is torn down.
DEFINE_EVENT(GameReset);

// Input

// Fires after the controllers are read, before the game sees the new input.
// Write through pads to change what the game reads this frame.
DEFINE_EVENT(ControllerRead,
    OSContPad* pads;  // mutable controller state, one per port
    int32_t count;    // number of entries in pads
);
