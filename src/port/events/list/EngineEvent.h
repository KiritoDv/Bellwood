#pragma once

// ────────────────────────────────────────────────────────────────────────────
// Engine lifecycle
// ────────────────────────────────────────────────────────────────────────────

DEFINE_EVENT(EngineReady);

// Fires when a keyboard key is pressed (desktop only).
DEFINE_EVENT(KeyboardInput,
    uint32_t scancode;  // platform scancode of the pressed key
);
