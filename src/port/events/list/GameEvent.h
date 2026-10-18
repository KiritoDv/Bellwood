#pragma once

#include <stdint.h>

struct actor_s;
struct door_data_s;
struct game_play_s;

// Scenes

// Fires when the game asks to leave the current scene through a door or exit. Cancellable.
// Write through door to change the destination. Cancel — the player stays in the scene.
DEFINE_EVENT(SceneChangeRequest,
    struct door_data_s* door;  // mutable destination (Door_data_c)
);

// Fires at the end of play initialisation, once the scene and its actors are set up.
DEFINE_EVENT(SceneLoaded,
    struct game_play_s* play;
    int32_t scene;  // scene number (SCENE_*)
);

// Fires when the play state is being torn down, before anything is freed.
DEFINE_EVENT(SceneUnloaded,
    struct game_play_s* play;
    int32_t scene;  // scene number (SCENE_*)
);

// Fires once per play frame, before actors and the field update.
DEFINE_EVENT(PlayUpdate,
    struct game_play_s* play;
);

// Fires before a Famicom (NES) game is launched from furniture. Cancellable.
// Write *romId to launch a different game. Cancel — nothing is launched.
DEFINE_EVENT(FamicomStart,
    int16_t* romId;  // mutable Famicom ROM id
);

// Fires when a Famicom ROM id in the custom range is started.
// Set rom, size and saveName to run the game; tags are optional save data descriptions.
DEFINE_EVENT(FamicomRomLookup,
    int32_t romId;
    const uint8_t* rom;    // set by the listener that owns the game
    uint32_t size;
    const uint8_t* tags;   // nesinfo tags, may stay NULL
    uint32_t tagsSize;
    const char* saveName;  // name of the game's save file
);

// New game

// Fires when K.K. starts his welcome on a new game. Cancellable.
// Cancel — the welcome and the sound setup talk are skipped and the game starts straight away.
DEFINE_EVENT(NewGameWelcome);

// Fires when Rover starts the train ride conversation. Cancellable.
// Cancel — Rover starts at the name question and the train arrives right after the last answer.
DEFINE_EVENT(TrainRideStart,
    bool newTown;  // true for a new town, false for a new resident in an existing town
);

// Messages

// Fires before a message is read from the message table.
// Write *msgNo to load a different message.
DEFINE_EVENT(MessageLoad,
    int32_t* msgNo;  // mutable message number
);

// Fires when a message number in the custom message range is loaded.
// Set text and size to the encoded message, ending with an end code.
DEFINE_EVENT(MessageLookup,
    int32_t msgNo;
    const uint8_t* text;  // set by the listener that owns the message
    uint32_t size;        // size of text in bytes
);

// Fires after a message is copied into the text buffer, before its length is measured.
// Rewrite text in place to change what is shown; keep it terminated with an end code.
DEFINE_EVENT(MessageLoaded,
    int32_t msgNo;
    uint8_t* text;      // mutable message text in the game's encoding
    uint32_t size;      // size of the original message in bytes
    uint32_t capacity;  // size of the text buffer
);

// Fires when a dialogue window starts opening.
DEFINE_EVENT(MessageOpened,
    struct actor_s* speaker;  // talking actor, may be NULL
    int32_t msgNo;
);

// Fires when a dialogue window starts closing.
DEFINE_EVENT(MessageClosed,
    int32_t msgNo;
);

// Menus

// Fires when the game asks for a menu (inventory, map, letters, shops, ...).
// Write *type to open a different menu.
DEFINE_EVENT(SubmenuOpen,
    int32_t* type;  // mutable menu type (mSM_OVL_*)
    int32_t param0;
    int32_t param1;
);

// Fires once a menu is linked in and starts running.
DEFINE_EVENT(SubmenuOpened,
    int32_t type;  // menu type (mSM_OVL_*)
);

// Fires when a menu closes and control goes back to the player.
DEFINE_EVENT(SubmenuClosed,
    int32_t type;  // menu type (mSM_OVL_*)
);

// Clock and weather

// Fires when the in-game clock reaches a new hour.
DEFINE_EVENT(TimeHourChanged,
    int32_t prev;
    int32_t now;
);

// Fires when the in-game date changes.
DEFINE_EVENT(TimeDayChanged,
    int32_t year;
    int32_t month;
    int32_t day;
);

// Fires when the town weather or its intensity changes. Polled once per play frame.
DEFINE_EVENT(WeatherChanged,
    int32_t prev;           // previous mEnv_WEATHER_*
    int32_t now;            // new mEnv_WEATHER_*
    int32_t prevIntensity;  // previous mEnv_WEATHER_INTENSITY_*
    int32_t intensity;      // new mEnv_WEATHER_INTENSITY_*
);
