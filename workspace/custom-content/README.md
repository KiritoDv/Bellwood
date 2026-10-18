# Custom Content Example

Adds a custom actor (a spinning Groundhog Day podium that greets you when you press A next to it), new furniture (a "mod table" and a "mod NES" that plays a small demo ROM), a new item (a "lucky gem" that breaks into 1,000 Bells when used), a new carpet, wallpaper and shirt, a new fish and insect (a "rainbow koi" in the river and a "mod dragonfly" from May to October), and a new villager (Mint, a peppy green cat who wears the mod tee) to Bellwood. All of them can be triggered from **Mods > Custom Content** in the menu. The mod table, carpet, wallpaper and tee are also on sale at Nook's on half of the days, and everything shows up in the catalog once you own it. The gem, koi and dragonfly icons, the carpet, the wallpaper and the tee are drawn from the PNGs in `src/textures`.

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
| `icon_tex`, `icon_pal` | Inventory icon: a 32x32 CI4 texture and its 16 colour palette. Use your own PNG through `#asset` with `CI4` and `CI4_PAL` (see [Textures](#textures)), or the game's own icons through `C_ASSET` (the `inv_mwin_*_tex` / `inv_mwin_*_pal` assets). NULL uses a default icon. |
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
| `int C_AddShopStock(mActor_name_t item, int chance)` | Puts furniture, a carpet, a wallpaper or a shirt, custom or built-in, in Nook's rotation. `chance` is the percentage of days it is on sale; it takes the place of one of the day's regular items of the same kind. Returns `FALSE` for any other kind of item. |
| `void C_RemoveShopStock(mActor_name_t item)` | Call it in `MOD_EXIT`. Copies already on sale stay until the next restock. |
| `int C_RestockShop(void)` | Rolls the stock again, as on a new day. Returns `FALSE` while the player is inside a shop, or before a game is loaded. |
| `void C_AddToCatalog(mActor_name_t item)` | Adds an item to the current player's catalog as if they had owned it. |
| `int C_IsInCatalog(mActor_name_t item)` | Whether the item is in the current player's catalog. |

Stock is rolled when Nook restocks, so a newly added item shows up from the next day or after `C_RestockShop`. Only furniture, carpets, wallpapers and shirts can go on the shop floor: each display spot draws its own models, and custom items have none. Nook shows a custom carpet or wallpaper on the shop's floor or wall when the player asks to see it, like the built-in ones.

Custom furniture the player has owned is listed at the end of the catalog's Furniture page, custom carpets, wallpapers and shirts at the end of their own pages, and custom items at the end of the Items page. All of them can be ordered at their `price` and arrive by mail the next day, like built-in orders. Which custom pieces each player has owned is kept in `custom_collection.json` next to the game, by town and player, since the save file has no room for them. Entries whose mod isn't loaded are hidden until it is.

`ShopStockGenerated` and `CatalogPageBuild` in `src/port/events/EVENTS.md` give full control over the stock and the catalog pages when the functions above aren't enough.

## Carpets and wallpapers (`port/api/interior.h`)

```c
static const u8 sTilesTexture[] = { #asset "textures/tiles.png" CI4 };
static const u8 sTilesPalette[] = { #asset "textures/tiles.png" CI4_PAL };

static C_InteriorDef sTilesDef = {
    "mymod.tiles", "tiled floor", mIN_ARTICLE_A, 1800, sTilesTexture, sTilesPalette,
};

sTiles = C_RegisterCarpet(&sTilesDef);
```

| Field | Description |
|---|---|
| `id` | Unique key. The item number assigned to it is stored in `custom_carpets.json` or `custom_wallpapers.json` next to the game, so rooms keep the right floor and wall across sessions. Never reuse an id for a different design. |
| `name` | Name shown in game, up to 16 characters. |
| `article` | `mIN_ARTICLE_*`. |
| `price` | Price in Bells. |
| `texture` | A 16 colour (CI4) texture. A carpet is 128x128 and holds the top-left quarter of the room's floor: the top-left 64x64 is the corner, the top-right 64x64 repeats along the top and bottom edges, the bottom-left 64x64 along the left and right edges, and the bottom-right 64x64 fills the middle. A wallpaper is 64x128, repeated along the wall. A single 64x64 texture is also accepted and used for every part. Raw arrays must be the full size. |
| `palette` | Its 16 colour palette. |
| `not_for_sale` | Optional. The catalog shows "Not for Sale" and it can't be ordered. |

| Function | Description |
|---|---|
| `mActor_name_t C_RegisterCarpet(const C_InteriorDef* def)` | Returns the item number, or `EMPTY_NO` if the texture can't be loaded. Up to 161 custom carpets fit. |
| `mActor_name_t C_RegisterWallpaper(const C_InteriorDef* def)` | Returns the item number, or `EMPTY_NO`. Up to 176 custom wallpapers fit. |
| `void C_UnregisterCarpet(mActor_name_t item)`, `void C_UnregisterWallpaper(mActor_name_t item)` | Call them in `MOD_EXIT`. |

Custom carpets and wallpapers work like the built-in ones: the player uses them from the pockets in their house or island cottage, and the one they replace goes back into the pockets. They count as common designs at the Happy Room Academy, and walking on a custom carpet sounds like walking on a design from the Able Sisters. If a room uses a carpet or wallpaper whose mod isn't loaded, it shows the default floor or wall until the mod is back, without losing it.

## Clothes (`port/api/cloth.h`)

```c
static const u8 sTeeTexture[] = { #asset "textures/tee.png" CI4 };
static const u8 sTeePalette[] = { #asset "textures/tee.png" CI4_PAL };

static C_ClothDef sTeeDef = {
    "mymod.tee", "striped tee", mIN_ARTICLE_A, 1200, sTeeTexture, sTeePalette,
};

sTee = C_RegisterCloth(&sTeeDef);
```

| Field | Description |
|---|---|
| `id` | Unique key. The item number assigned to it is stored in `custom_clothes.json` next to the game. Never reuse an id for a different shirt. |
| `name`, `article` | Name shown in game, up to 16 characters, and its `mIN_ARTICLE_*`. |
| `price` | Price in Bells. |
| `texture`, `palette` | A 32x32 CI4 texture and its 16 colour palette, laid out like an Able Sisters design: the texture wraps around the body, with the chest in the middle. |
| `not_for_sale` | Optional. The catalog shows "Not for Sale" and it can't be ordered. |

| Function | Description |
|---|---|
| `mActor_name_t C_RegisterCloth(const C_ClothDef* def)` | Returns the item number, or `EMPTY_NO`. Up to 240 custom shirts fit. |
| `void C_UnregisterCloth(mActor_name_t item)` | Call it in `MOD_EXIT`. |

The game's clothing numbers are all taken, so custom shirts use the free end of the diary category (0x2B10 to 0x2BFF) and the game treats them as clothing everywhere: the player wears them from the pockets or the inventory background, Nook sells them on a mannequin and lets the player try them on, they can be placed at home on a mannequin, listed and ordered in the catalog, and given or mailed to villagers, who wear them. Villagers don't pick custom shirts on their own, and quests and Blanca only use the built-in ones.

## Villagers (`port/api/villager.h`)

```c
static C_VillagerDef sMintDef = {
    "mymod.mint", "Mint", "minty", NPC_BOB, mNpc_LOOKS_KO_GIRL, EMPTY_NO, NULL, sMintPalette, 0, 2, 3, -1, -1, FALSE,
};

sMint = C_RegisterVillager(&sMintDef);
```

A custom villager is built on one of the 236 built-in villagers, its `base`: it uses the base's species, model, animations and room layout, and everything below can be changed. Dialogue in this game depends on the personality, so a custom villager talks like the other villagers of its personality, with its own name and catchphrase.

| Field | Description |
|---|---|
| `id` | Unique key. The villager id assigned to it is stored in `custom_villagers.json` next to the game. Never reuse an id for a different villager. |
| `name` | Name shown in game, up to 8 characters. |
| `catchphrase` | Added to the end of sentences, up to 10 characters. Villagers can still change it over time, like built-in ones. |
| `base` | `NPC_*` id of the built-in villager it is built on, from `m_name_table.h`. |
| `looks` | Personality, `mNpc_LOOKS_*` from `m_npc_personal_id.h`: normal, peppy, lazy, jock, cranky or snooty. It also decides whether it is a boy or a girl. |
| `cloth` | Shirt worn when moving in, built-in or custom. `EMPTY_NO` for the base's. |
| `texture` | Body texture replacing the base's, in the same size and format (for example the 2048x1 CI4 strip of most villagers). NULL for the base's. Raw arrays must be 2-byte aligned, like `C_ASSET` and `#asset` paths. |
| `palette` | 16 colour palette replacing the base's, as an asset or raw big-endian RGBA5551 bytes. Swapping only the palette recolours the base. NULL for the base's. |
| `voice` | Voice number, 0 for the base's. |
| `house_type`, `house_palette` | House shape and colours, 0 to 4 each, -1 for the base's. |
| `wall`, `floor` | Wallpaper and carpet of its room, 0 to 66, -1 for the base's. |
| `starter` | Can be one of the first villagers of a new town. Otherwise it only moves in later. |

| Function | Description |
|---|---|
| `mActor_name_t C_RegisterVillager(const C_VillagerDef* def)` | Returns the villager id, or `EMPTY_NO`. Up to 17 custom villagers fit. |
| `void C_UnregisterVillager(mActor_name_t npc_id)` | Call it in `MOD_EXIT`. A town it lives in shows it as the base villager Bob until it is registered again. |
| `int C_ReplaceVillager(mActor_name_t old_npc_id, mActor_name_t new_npc_id)` | Moves a villager, built-in or custom, into the house of a town villager in place of them, as if they had just moved in. The town shows it the next time it loads. |

Custom villagers move in like built-in ones: when a house is free, the game picks among the villagers of the needed personality that haven't lived in town yet, custom ones included. They walk around, talk, have a house you can visit, wear and get given clothes, and write letters. The island and e-Reader cards only use built-in villagers.

## Fish and insects (`port/api/creature.h`)

```c
#include "ac_gyoei.h"            // aGYO_TYPE_*
#include "ac_set_ovl_gyoei.h"    // aSOG_SPAWN_AREA_*

static const u8 sKoiIcon[] = { #asset "textures/koi.png" CI4 };
static const u8 sKoiPalette[] = { #asset "textures/koi.png" CI4_PAL };

static C_CreatureDef sKoiDef = {
    "mymod.rainbow_koi", "rainbow koi", mIN_ARTICLE_A, 3000,
    aGYO_TYPE_KOI, aSOG_SPAWN_AREA_RIVER, 10, 0, 0, FALSE,
    "I caught a rainbow koi!", sKoiIcon, sKoiPalette, NULL,
};

sKoi = C_RegisterFish(&sKoiDef);
```

A custom fish or insect is built on one of the 40 built-in ones, its `base`. It moves, flees, bites or sings like the base, has the base's size and shadow, and is kept at home in the base's aquarium or cage. Everything else comes from the definition.

| Field | Description |
|---|---|
| `id` | Unique key. The item number assigned to it is stored in `custom_fish.json` or `custom_insects.json` next to the game. Never reuse an id for a different creature. |
| `name`, `article` | Name shown in game, up to 16 characters, and its `mIN_ARTICLE_*`. |
| `price` | Value in Bells. Nook pays a quarter of it, like for the built-in ones. |
| `base` | `aGYO_TYPE_*` from `ac_gyoei.h` for fish, `aINS_INSECT_TYPE_*` from `ac_insect_h.h` for insects. |
| `place` | Where it appears. Fish: `aSOG_SPAWN_AREA_RIVER`, `_POOL` (river pools), `_WATERFALL`, `_POND`, `_SEA`, `_RIVER_MOUTH` or `_OFFING`, from `ac_set_ovl_gyoei.h`. Insects: `aSOI_SPAWN_AREA_*` from `ac_set_ovl_insect.h`, such as `_ON_TREE`, `_ON_FLOWER`, `_FLYING`, `_ON_GROUND`, `_UNDER_ROCK` or `_ON_WATER`. Pick a place the base can live in, since the base decides how it behaves there: a cicada needs a tree. |
| `weight` | How often it is picked against the other candidates. Common built-in fish and insects use about 10 to 30, rare ones 1 to 3. 0 means it never appears by itself. |
| `months` | Bit 0 is January and bit 11 December. 0 for all year. |
| `hours` | Bit 0 is midnight and bit 23 11 PM. 0 for all day. The game rebuilds its insect list six times a day, so for insects the hours apply from the next of those changes (4 AM, 8 AM, 4 PM, 5 PM, 7 PM and 11 PM). |
| `island` | Also appears on the island. |
| `catch_text` | Shown when it is caught; dialogue tags work (see [Dialogue](#dialogue-portapimessageh)). NULL for "I caught a *name*!". |
| `icon_tex`, `icon_pal` | 32x32 CI4 pocket icon and its 16 colour palette. NULL for the base's icon. |
| `model` | Optional display lists that replace the base's model, in the same number and order. Fish have three, one per animation frame. Most insects have four (two frames, two parts each); the mole cricket has two and the bagworm and spider six. NULL for the base's model. |

| Function | Description |
|---|---|
| `mActor_name_t C_RegisterFish(const C_CreatureDef* def)`, `C_RegisterInsect` | Returns the item number, or `EMPTY_NO`. Up to 211 custom fish and 211 custom insects fit. |
| `void C_UnregisterFish(mActor_name_t item)`, `C_UnregisterInsect` | Call them in `MOD_EXIT`. |

Custom fish and insects are caught with the rod or the net, show their catch text, go into the pockets, can be released, sold to Nook, or placed at home, where they show as the base's aquarium or cage and count as fish or insects at the Happy Room Academy. Some things stay with the 40 built-in ones:
- Blathers hands custom ones back instead of taking them for the museum.
- The fish and insect pages in the pockets, the golden net and rod, and "caught them all" only count the built-in ones.
- Villagers don't ask for custom ones in trades, and Chip's fishing tourney only judges the bass.

`FishSpawnList` and `InsectSpawnList` in `src/port/events/EVENTS.md` show the full lists of candidates and let a mod change them.

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

### Textures

`#asset` turns a PNG next to the source file into a texture in the game's own formats when the mod is built (Pillow is required):

```c
static const u8 sIcon[] = { #asset "textures/icon.png" CI4 };
static const u8 sIconPalette[] = { #asset "textures/icon.png" CI4_PAL };
```

| Format | Use |
|---|---|
| (none) or `RGBA32` | Full colour, decoded when the game loads it. |
| `RGBA16` | 5 bits per channel and 1 bit of alpha. |
| `CI4`, `CI8` | 16 or 256 colours from a palette. Item icons, carpets and wallpapers are `CI4`. |
| `CI4_PAL`, `CI8_PAL` | The palette for the same PNG's `CI4` or `CI8` texture. |
| `I4`, `I8` | Greyscale. |
| `IA4`, `IA8`, `IA16` | Greyscale with alpha. |

For `CI4` and `CI8`, a PNG that already has a palette of up to 16 or 256 colours keeps those colours in their order. Any other PNG is reduced to 16 or 256 colours, with transparent pixels on colour 0. Colours are stored with 5 bits per channel, so they can shift slightly.
