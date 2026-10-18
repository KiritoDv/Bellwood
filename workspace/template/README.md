# Bellwood C Mod Template

A starting point for C code mods for Bellwood. Mods are compiled at runtime with TCC, so a mod ships as C source inside an `.o2r` archive.

The build merges every `.c` file in `src/` together with the Bellwood and libultraship headers it includes into a single self-contained file per source under `output/dist/`, ready to be packaged next to `manifest.json`.

## Layout

```
├── CMakeLists.txt
├── manifest.json        Mod metadata (name, version, author, dependencies, ...)
├── include/
│   └── mod.h            MOD_INIT / MOD_EXIT and allocator macros
├── src/
│   └── demo.c           Example mod
├── assets/              Optional files copied into the archive as-is
└── output/              Generated: dist/*.c, build.gen, manifest.json with code_version and api_version
```

## Building

The template expects to live inside a Bellwood checkout, in `workspace/<your-mod>`, with the `libultraship` submodule initialized.

Requirements: CMake 3.10+, Python 3. Pillow is only needed for `.png` and `.gltf` assets referenced with `#asset`. `#asset "file.png" CI4`, the other N64 formats and the `NPC_MODEL` / `NPC_ANIM` glTF conversions are described in `workspace/custom-content`.

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
    // Register listeners, content and menu entries
}

MOD_EXIT() {
    // Undo anything else MOD_INIT did
}
```

When a mod unloads, everything it registered through the `port/api` headers is unregistered after its `MOD_EXIT`, along with its event listeners, menu entries and GUI callbacks. `MOD_EXIT` is optional and only needs to undo what the mod set up by other means. Mods are unloaded and rebuilt when "Reload Scripts" is pressed in the Mods menu.

### Events

Game events are declared in `src/port/events/list/*.h` and documented in `src/port/events/EVENTS.md`.

```c
#include "port/events/Events.h"

static void OnMoneyChanged(IEvent* event) {
    PlayerMoneyChanged* money = (PlayerMoneyChanged*)event;
}

MOD_INIT() {
    REGISTER_LISTENER(PlayerMoneyChanged, EVENT_PRIORITY_NORMAL, OnMoneyChanged);
}
```

Cancellable events (for example `ResetDetected`, `TrainRideStart`, `FamicomStart`) can be cancelled from a listener with `event->Cancelled = true`.

`UNREGISTER_LISTENER` removes a listener while the mod runs; the ones still registered when it unloads are removed for it.

### Game state

`port/api/player.h` and `port/api/town.h` cover the common cases (pockets, wallet, player position, scene, date and time, the town's villagers) with functions that keep their signatures across versions. `port/api/savedata.h` keeps values of the mod's own in the town's save file. `port/api/sound.h` plays sounds and music from the mod's archive, `port/api/mail.h` sends letters, and `port/api/visitor.h` brings characters to town on a schedule. See `workspace/custom-content` for both.

Game headers from `include/` can be included directly, for example `m_common_data.h` for `Common_Get(...)` and `m_private.h` for player data.

On Windows a mod can only read or write a game variable whose declaration uses `extern_s` (it becomes `__declspec(dllimport)` when TCC builds a mod). The core runtime globals such as `common_data`, `gamePT`, `graph_class` and `padmgr_class` already do. To use another one, change its `extern` to `extern_s` in the game header. Functions don't need this.

### Settings and menu

- `port/api/cvar.h`: `CVarGetInteger`, `CVarSetInteger` and the rest of the console variable API.
- `port/api/ui.h`: `C_AddSidebarEntry`, `C_AddWidget` and `C_RemoveSidebarEntry` add pages and widgets under the Mods menu.

Use `port/api/cvar.h` instead of `libultraship/bridge/consolevariablebridge.h`. The libultraship colour types conflict with the game's.

Widget tooltips and CVar names are kept as pointers, so pass string literals.

### Custom actors, dialogue, furniture, items, carpets, wallpapers, clothes, fish, insects, villagers, NES games and the shop

`port/api/actor.h`, `port/api/message.h`, `port/api/furniture.h`, `port/api/item.h`, `port/api/interior.h`, `port/api/cloth.h`, `port/api/creature.h`, `port/api/villager.h` and `port/api/famicom.h` register new actors, messages, furniture, items, carpets, wallpapers, shirts, fish, insects, villagers and NES games, and `port/api/shop.h` puts them in Nook's stock and the catalog. See `workspace/custom-content` for a working example and the full reference.

### Dependencies

A mod that needs another lists it by its manifest name. A version range is optional:

```json
"dependencies": ["Other Mod", { "name": "Core Library", "version": ">=1.2 <2" }]
```

Dependencies initialize first. A range is a list of comparisons (`>=`, `>`, `<=`, `<`, `=`); a version without one is a minimum. A mod whose dependency is missing, failed to build or is outside the range is skipped with a notification, and so are the mods that depend on it. Call into another mod with `CALL_FUNC` from `mod.h`.

## Notes

- Everything from all source files ends up in one translation unit per file. Mark helpers `static` so they don't collide with the game or other mods.
- `mod.h` routes `malloc` and `free` through the engine allocator.
- The build writes `code_version` and `api_version` to the packaged `manifest.json` from `src/port/api/api.h`. A mod loads only in a Bellwood with the same code version and at least its API version; rebuild it when the code version changes.
- The `C_Register*` and `C_AddWidget` macros pass the size of the definition the mod was built with, so fields added to the end of a definition in later versions read as zero for older mods.
- Archives can be signed with `libultraship/tools/sign.py`. Unsigned mods still load, with a warning.
