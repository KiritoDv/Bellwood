# Bellwood C Mod Template

A starting point for C code mods for Bellwood. Mods are compiled at runtime with TCC, so a mod ships as C source inside an `.o2r` archive.

The build merges every `.c` file in `src/` together with the Bellwood and libultraship headers it includes into a single self-contained file per source under `output/dist/`, ready to be packaged next to `manifest.json`.

## Layout

```
├── CMakeLists.txt
├── manifest.json        Mod metadata (name, version, author, ...)
├── include/
│   └── mod.h            MOD_INIT / MOD_EXIT and allocator macros
├── src/
│   └── demo.c           Example mod
├── assets/              Optional files copied into the archive as-is
└── output/              Generated: dist/*.c, build.gen, manifest.json
```

## Building

The template expects to live inside a Bellwood checkout, in `workspace/<your-mod>`, with the `libultraship` submodule initialized.

Requirements: CMake 3.10+, Python 3. Pillow is only needed for `.png` assets referenced with `#asset`. `#asset "file.png" CI4` and the other N64 formats are described in `workspace/custom-content`.

```sh
cmake -B build -S .
cmake --build build --target package_mod
```

This produces `ModTemplate.o2r` (named after the CMake project). Copy it into Bellwood's `mods/` folder and start the game. Change `project()` in `CMakeLists.txt` to rename the archive.

## Writing a mod

Every mod exports an entry and exit point:

```c
#include "mod.h"

MOD_INIT() {
    // Register listeners and menu entries
}

MOD_EXIT() {
    // Unregister everything registered in MOD_INIT
}
```

### Events

Game events are declared in `src/port/events/list/*.h` and documented in `src/port/events/EVENTS.md`.

```c
#include "port/events/Events.h"

static ListenerID sListener;

static void OnMoneyChanged(IEvent* event) {
    PlayerMoneyChanged* money = (PlayerMoneyChanged*)event;
}

MOD_INIT() {
    sListener = REGISTER_LISTENER(PlayerMoneyChanged, EVENT_PRIORITY_NORMAL, OnMoneyChanged);
}

MOD_EXIT() {
    UNREGISTER_LISTENER(PlayerMoneyChanged, sListener);
}
```

Cancellable events (for example `ResetDetected`, `TrainRideStart`, `FamicomStart`) can be cancelled from a listener with `event->Cancelled = true`.

Always unregister listeners in `MOD_EXIT`. Mods are unloaded and rebuilt when "Reload Scripts" is pressed in the Mods menu.

### Game state

Game headers from `include/` can be included directly, for example `m_common_data.h` for `Common_Get(...)` and `m_private.h` for player data.

On Windows a mod can only read or write a game variable whose declaration uses `extern_s` (it becomes `__declspec(dllimport)` when TCC builds a mod). The core runtime globals such as `common_data`, `gamePT`, `graph_class` and `padmgr_class` already do. To use another one, change its `extern` to `extern_s` in the game header. Functions don't need this.

### Settings and menu

- `port/api/cvar.h`: `CVarGetInteger`, `CVarSetInteger` and the rest of the console variable API.
- `port/api/ui.h`: `C_AddSidebarEntry`, `C_AddWidget` and `C_RemoveSidebarEntry` add pages and widgets under the Mods menu.

Use `port/api/cvar.h` instead of `libultraship/bridge/consolevariablebridge.h`. The libultraship colour types conflict with the game's.

Widget tooltips and CVar names are kept as pointers, so pass string literals.

### Custom actors, dialogue, furniture, items, carpets, wallpapers, fish, insects, NES games and the shop

`port/api/actor.h`, `port/api/message.h`, `port/api/furniture.h`, `port/api/item.h`, `port/api/interior.h`, `port/api/creature.h` and `port/api/famicom.h` register new actors, messages, furniture, items, carpets, wallpapers, fish, insects and NES games, and `port/api/shop.h` puts them in Nook's stock and the catalog. See `workspace/custom-content` for a working example and the full reference.

## Notes

- Everything from all source files ends up in one translation unit per file. Mark helpers `static` so they don't collide with the game or other mods.
- `mod.h` routes `malloc` and `free` through the engine allocator.
- The `code_version` in `manifest.json` must match the engine's code version (currently `1`).
- Archives can be signed with `libultraship/tools/sign.py`. Unsigned mods still load, with a warning.
