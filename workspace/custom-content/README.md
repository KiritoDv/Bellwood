# Custom Content Example

Adds a custom actor (a spinning Groundhog Day podium that greets you when you press A next to it), new furniture (a "mod table" and a "mod NES" that plays a small demo ROM) and a new item (a "lucky gem" that breaks into 1,000 Bells when used) to Bellwood. All of them can be triggered from **Mods > Custom Content** in the menu. The mod table is also on sale at Nook's on half of the days, and everything shows up in the catalog once you own it.

Build and install it like the template:

```sh
cmake -B build -S .
cmake --build build --target package_mod
```

Copy `CustomContent.o2r` into Bellwood's `mods/` folder.

## Custom actors (`port/api/actor.h`)

Define an `ACTOR_PROFILE` the same way the game's actors do and register it in `MOD_INIT`. You get back a profile number past the built-in ones.

```c
static ACTOR_PROFILE sMyProfile = {
    0, ACTOR_PART_ITEM, ACTOR_STATE_TA_SET, EMPTY_NO, ACTOR_OBJ_BANK_KEEP,
    sizeof(MY_ACTOR), &My_ct, mActor_NONE_PROC1, &My_mv, &My_dw, NULL,
};

static s16 sMyActor;

MOD_INIT() {
    sMyActor = C_RegisterActor(&sMyProfile);
}

MOD_EXIT() {
    C_UnregisterActor(sMyActor);
}
```

| Function | Description |
|---|---|
| `int16_t C_RegisterActor(const ACTOR_PROFILE* profile)` | Copies the profile and returns its new profile number, or -1. Up to 64 custom actors can be registered. |
| `void C_UnregisterActor(int16_t profile)` | Runs the destructor of every live actor using the profile, deletes them and frees the number. Always call it in `MOD_EXIT`, the actor code is unloaded right after. |
| `ACTOR* C_SpawnActor(int16_t profile, float x, float y, float z, int16_t angle_y, int16_t arg)` | Spawns any actor, built-in or custom, in the current scene. Returns NULL outside of gameplay. |
| `GAME_PLAY* C_GetPlay(void)` | The scene being played, or NULL. |
| `int C_ActorSpeak(ACTOR* actor, int32_t msg_no)` | Starts a conversation with the player showing a message. Call it when the player presses A next to the actor, like the game's signs do. |

The profile number also works with the game's own `Actor_info_make_actor`, and through the `ActorSpawn` event.

The draw function starts with the actor's position, rotation (`shape_info.rotation`) and scale already loaded into the matrix. Spawn actors from a game event such as `PlayUpdate`, not from a menu callback.

## Dialogue (`port/api/message.h`)

Register text in `MOD_INIT` and you get a message number that works anywhere the game takes one, including `C_ActorSpeak` and the game's own `mDemo_Set_msg_num`.

```c
static int32_t sGreeting;

MOD_INIT() {
    sGreeting = C_RegisterMessage("Hi, <<STR_PLAYERNAME>>!\nI'm a podium from a mod.<<BTN>>\n"
                                  "<<MSGCLEAR>>Come back and talk to me\nany time!");
}

static void My_mv(ACTOR* actor, GAME* game) {
    if (!mDemo_IS_ACTOR_TALKING(actor) && actor->player_distance_xz < 80.0f && chkTrigger(BUTTON_A)) {
        C_ActorSpeak(actor, sGreeting);
    }
}
```

Text is UTF-8, so accented letters and the game's symbols (♥, ♪, ★ and so on) can be typed directly. `\n` starts a new line, and control codes use the same `<<NAME [hex bytes]>>` tags as `tools/msg_tool.py`, so lines decoded from the game's messages can be copied as they are:

| Tag | Effect |
|---|---|
| `<<BTN>>` then `\n<<MSGCLEAR>>` | Wait for A, then start a new page. |
| `<<STR_PLAYERNAME>>`, `<<STR_TALKNAME>>`, `<<STR_TAIL>>` | Player name, speaker name, speaker's catchphrase. |
| `<<STR_HOUR>>`, `<<STR_MIN>>`, `<<STR_DAY>>`, `<<STR_MONTH>>` | Current time and date. |
| `<<PAUSE [08]>>` | Pause while typing, in frames. |
| `<<TEXTCOLOR [RR GG BB]>>` | Text colour. |
| `<<SETFORCEMSG [40 01]>>` then `<<MSGCONTINUE>>` | Continue with another message (here 0x4001). |

An end code is added when the text doesn't finish with one. Each message holds up to 1,534 bytes; windows fit four lines.

| Function | Description |
|---|---|
| `int32_t C_RegisterMessage(const char* text)` | Returns a new message number (0x4000-0x7FFF), or -1. |
| `void C_UnregisterMessage(int32_t msg_no)` | Call it in `MOD_EXIT`. |
| `void C_SetMessage(int32_t msg_no, const char* text)` | Replaces the text of any message, including the game's own. |
| `void C_ResetMessage(int32_t msg_no)` | Restores a built-in message changed with `C_SetMessage`. Call it in `MOD_EXIT`. |
| `int C_EncodeText(const char* text, uint8_t* out, int capacity)` | Encodes text into the game's character set, for example for `mMsg_SET_FREE_STR`. |

Custom furniture and item names use the same encoding, so they can contain accented letters too.

## Custom furniture (`port/api/furniture.h`)

Fill a `C_FurnitureDef` and register it. The `profile` member is the same `aFTR_PROFILE` the game uses for its own furniture, so models, rigs and the optional `ct`/`move`/`draw`/`dt` callbacks work the same way.

```c
C_ASSET(sTable, "data/model/int_ari_table01/int_ari_table01_01_model");
C_ASSET(sTableT, "data/model/int_ari_table01/int_ari_table01_01T_model");

static C_FurnitureDef sTableDef = {
    "mymod.table", "my table", mIN_ARTICLE_A, 4000, aFTR_SET_TYPE_SURFACE, mHsRm_HUUSUI_GREEN, FALSE, FALSE,
    { (Gfx*)sTable, (Gfx*)sTableT, NULL, NULL, NULL, NULL, NULL, NULL, 20.0f, 0.01f,
      aFTR_SHAPE_TYPEA, mCoBG_FTR_TYPEA, 0, 0, 0, 0, NULL },
};
```

| Field | Description |
|---|---|
| `id` | Unique key. The item number assigned to it is stored in `custom_furniture.json` next to the game, so saved rooms keep the right furniture across sessions. Never reuse an id for a different piece. |
| `name` | Name shown in game, up to 16 characters. |
| `article` | `mIN_ARTICLE_*`, used in sentences like "You got a my table". |
| `price` | Price in Bells. |
| `set_type` | `aFTR_SET_TYPE_NORMAL`, `_SURFACE` (other furniture can sit on it) or `_ON_SURFACE` (it can sit on a surface). |
| `huusui_type`, `has_face`, `is_lucky` | Feng shui colour and Happy Room Academy flags. |
| `profile` | Models (`opaque0/1`, `translucent0/1`), `texture`/`palette`, `rig`, `height`, `scale`, `shape` (`aFTR_SHAPE_TYPEA` 1x1, `_TYPEB_*` 2x1, `_TYPEC` 2x2), `move_bg_type` (matching `mCoBG_FTR_TYPE*`), contact and interaction flags and the callback table. |
| `not_for_sale` | Optional. The catalog shows "Not for Sale" and it can't be ordered. |

| Function | Description |
|---|---|
| `mActor_name_t C_RegisterFurniture(const C_FurnitureDef* def)` | Returns the south-facing item number. The east, north and west facings are the next three numbers. |
| `void C_UnregisterFurniture(mActor_name_t item)` | Call it in `MOD_EXIT`. Placed copies draw as the placeholder until the mod is loaded again. |

Give the item to the player with the game's own `mPr_SetFreePossessionItem(Common_Get(now_private), item, mPr_ITEM_COND_NORMAL)`.

The game reads a custom piece's model, size, name, article, price, Happy Room Academy and feng shui data from its definition. It shows up in the catalog once the player owns it, and can be added to Nook's stock (see [Shop and catalog](#shop-and-catalog-portapishoph)). If a save holds furniture whose mod isn't installed, it draws as the placeholder and shows as an unknown item.

## Custom items (`port/api/item.h`)

Fill a `C_ItemDef` and register it. Custom items live in the unused end of the game's miscellaneous item category, so the game treats them as plain items in the pockets, on the ground and at Nook's.

```c
C_ASSET(sGemIcon, "data/model/inv_mwin13/inv_mwin_sango_tex");
C_ASSET(sGemPalette, "data/model/inv_mwin13/inv_mwin_sango_pal");

static void Gem_use(mActor_name_t item, int slot) {
    mPr_SetPossessionItem(Common_Get(now_private), slot, EMPTY_NO, mPr_ITEM_COND_NORMAL);
}

static C_ItemDef sGemDef = {
    "mymod.gem", "gem", mIN_ARTICLE_A, 2500, mNT_ITEM_TYPE_SHELLA, sGemIcon, sGemPalette, &Gem_use,
};
```

| Field | Description |
|---|---|
| `id` | Unique key. The item number assigned to it is stored in `custom_items.json` next to the game. Never reuse an id for a different item. |
| `name` | Name shown in game, up to 16 characters. |
| `article` | `mIN_ARTICLE_*`. |
| `price` | Price in Bells. 0 means Nook takes it for free. |
| `shape` | `mNT_ITEM_TYPE_*`, the model used when the item lies on the ground or is in the player's hand (for example `mNT_ITEM_TYPE_BAG`, `_BOX`, `_PRESENT`, `_SHELLA`). 0 uses the generic one. |
| `icon_tex`, `icon_pal` | Inventory icon: a 32x32 CI4 texture and its 16 colour palette. Use the game's own icons through `C_ASSET` (the `inv_mwin_*_tex` / `inv_mwin_*_pal` assets). NULL uses a default icon. |
| `on_use` | Optional. Adds "Use" to the pocket menu; called with the item and its pocket slot after the menu closes. |
| `not_for_sale` | Optional. The catalog shows "Not for Sale" and it can't be ordered. |

| Function | Description |
|---|---|
| `mActor_name_t C_RegisterItem(const C_ItemDef* def)` | Returns the item number, or `EMPTY_NO`. Up to 207 custom items fit (`C_ITEM_FIRST` to `C_ITEM_LAST`). |
| `void C_UnregisterItem(mActor_name_t item)` | Call it in `MOD_EXIT`. Copies in the pockets or on the ground show as unknown until the mod is loaded again. |

Indoors, custom items only offer Grab and Use, since rooms have no model to show them on the floor. Outdoors, items with `on_use` offer Grab, Use, Drop and Quit; items without it get the regular item menu. They don't appear in quests or on the shop floor, but once the player owns one it is listed on the catalog's Items page and can be ordered.

## Shop and catalog (`port/api/shop.h`)

```c
C_AddShopStock(sTable, 50);   // on sale on about half of the days
```

| Function | Description |
|---|---|
| `int C_AddShopStock(mActor_name_t item, int chance)` | Puts a furniture item, custom or built-in, in Nook's furniture rotation. `chance` is the percentage of days it is on sale; it takes the place of one of the day's regular pieces. Returns `FALSE` for anything that isn't furniture. |
| `void C_RemoveShopStock(mActor_name_t item)` | Call it in `MOD_EXIT`. Copies already on sale stay until the next restock. |
| `int C_RestockShop(void)` | Rolls the stock again, as on a new day. Returns `FALSE` while the player is inside a shop, or before a game is loaded. |
| `void C_AddToCatalog(mActor_name_t item)` | Adds an item to the current player's catalog as if they had owned it. |
| `int C_IsInCatalog(mActor_name_t item)` | Whether the item is in the current player's catalog. |

Stock is rolled when Nook restocks, so a newly added item shows up from the next day or after `C_RestockShop`. Only furniture can go on the shop floor: each display spot draws its own models, and custom items have none.

Custom furniture the player has owned is listed at the end of the catalog's Furniture page, and custom items at the end of the Items page. Both can be ordered at their `price` and arrive by mail the next day, like built-in orders. Which custom pieces each player has owned is kept in `custom_collection.json` next to the game, by town and player, since the save file has no room for them. Entries whose mod isn't loaded are hidden until it is.

`ShopStockGenerated` and `CatalogPageBuild` in `src/port/events/EVENTS.md` give full control over the stock and the catalog pages when the functions above aren't enough.

## NES games (`port/api/famicom.h`)

Register an iNES (`.nes`) ROM and you get a ROM number that the game's emulator can run. Put the file in the mod's `assets/` folder and register it by its path in the archive, or pass the bytes directly:

```c
static int sMyGame;

MOD_INIT() {
    sMyGame = C_RegisterFamicomGameFile("mymod.game", "famicom/mygame.nes");
}

MOD_EXIT() {
    C_UnregisterFamicomGame(sMyGame);
}
```

To play it, make a custom furniture piece that calls `C_FamicomFurnitureMove` from its move callback and sets `aFTR_INTERACTION_FAMICOM` in its profile. Switching it on asks the usual "Would you like to play...?" question, using the furniture's name:

```c
static void MyNes_mv(FTR_ACTOR* ftr_actor, ACTOR* my_room, GAME* game, u8* data) {
    C_FamicomFurnitureMove(ftr_actor, my_room, game, sMyGame);
}

static aFTR_vtable_c sMyNesFuncs = { NULL, &MyNes_mv, &MyNes_dw, NULL, NULL };
```

The example's "mod NES" draws the game's own NES model and plays a tiny demo ROM built in `main.c`, which changes the screen colour when A is pressed.

| Function | Description |
|---|---|
| `int C_RegisterFamicomGame(const char* id, const uint8_t* rom, uint32_t size)` | Copies the ROM and returns its number (64-127), or -1. |
| `int C_RegisterFamicomGameFile(const char* id, const char* path)` | Same, reading the ROM from a file in a mod archive. |
| `void C_UnregisterFamicomGame(int rom_no)` | Call it in `MOD_EXIT`. |
| `void C_FamicomFurnitureMove(FTR_ACTOR* ftr_actor, ACTOR* my_room, GAME* game, int rom_no)` | Makes a furniture piece an NES that plays `rom_no`. |

`id` names the game's save file (`save/famicom/Bellwood_<id>.bin`), so keep it the same between versions of the mod. Games with the battery flag in their iNES header keep their 8 KB battery RAM there. ROM numbers are handed out in load order, so look them up at startup rather than hard-coding them. The `FamicomStart` event can also redirect any of the game's own NES furniture to a registered ROM.

Quit a game with L, R and Z, like the built-in ones.

## Assets

`C_ASSET(name, path)` declares a game asset by its path inside the game archive, so it can be used as a `Gfx*`, texture or palette. The paths match the `*_PATH` macros in the game's `include/assets` headers.
