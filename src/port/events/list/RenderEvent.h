#pragma once

struct game_play_s;

// Rendering

// Fires after the play frame's display lists are reset, before the scene is drawn.
DEFINE_EVENT(RenderGamePre,
    struct game_play_s* play;
);

// Fires after the play frame has been drawn, menus included.
DEFINE_EVENT(RenderGamePost,
    struct game_play_s* play;
);
