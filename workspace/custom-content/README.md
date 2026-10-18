# Custom Content Example

Adds a custom actor (a spinning Groundhog Day podium that greets you when you press A next to it), new furniture (a "mod table" and a "mod NES" that plays a small demo ROM), a new item (a "lucky gem" that breaks into 1,000 Bells when used), a new carpet, wallpaper and shirt, a new fish and insect (a "rainbow koi" in the river and a "mod dragonfly" from May to October), two new villagers (Mint, a peppy green dog who wears the mod tee, and Bolt, a lazy robot with a model and walk of his own), and two buildings you can put in town and walk into (a kiosk with a furnished room, and a workshop whose room has a model and size of its own) to Bellwood. All of them can be triggered from **Mods > Custom Content** in the menu. The mod table, carpet, wallpaper and tee are also on sale at Nook's on half of the days, and everything shows up in the catalog once you own it. The gem, koi and dragonfly icons, the carpet, the wallpaper and the tee are drawn from the PNGs in `src/textures`, and Bolt and the buildings from the glTF models in `src/models`.

Build and install it like the template:

```sh
cmake -B build -S .
cmake --build build --target package_mod
```

Copy `CustomContent.o2r` into Bellwood's `mods/` folder.

Everything a mod registers with these APIs is unregistered when it unloads, after its `MOD_EXIT` and before its code is freed, so `MOD_EXIT` doesn't need to undo `MOD_INIT`. The `C_Unregister*` functions are for taking content away while the mod runs. The same goes for its event listeners, menu entries and GUI callbacks, and for anything registered from its actor and furniture callbacks. Furniture placed in the room being shown is destroyed with the old code and constructed again with the new one when the mod reloads.

The numbers given to content ids are kept next to the game in `custom_<kind>.json`, and in each town file under the ids it uses. When a town saved by another install loads, or one whose `custom_*.json` files were deleted, its content is renumbered to match. A town that uses content no loaded mod registers shows a warning naming the mods.

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
```

| Function | Description |
|---|---|
| `int16_t C_RegisterActor(const ACTOR_PROFILE* profile)` | Copies the profile and returns its new profile number, or -1. Up to 64 custom actors can be registered. |
| `void C_UnregisterActor(int16_t profile)` | Runs the destructor of every live actor using the profile, deletes them and frees the number. |
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
| `void C_UnregisterMessage(int32_t msg_no)` | Frees a message number. |
| `void C_SetMessage(int32_t msg_no, const char* text)` | Replaces the text of any message, including the game's own. |
| `void C_ResetMessage(int32_t msg_no)` | Restores a built-in message changed with `C_SetMessage`. |
| `int C_EncodeText(const char* text, uint8_t* out, int capacity)` | Encodes text into the game's character set, for example for `mMsg_SET_FREE_STR`. |

A question offers 2 to 4 choices and goes on to the message picked. The podium asks one:

```c
static const char* const answers[] = { "Spin faster!", "Just saying hi" };
int32_t next[2] = { C_RegisterMessage("Wheee!"), C_RegisterMessage("Come back any time!") };
sGreeting = C_RegisterQuestion("What brings you here?", answers, next, 2);
```

| Function | Description |
|---|---|
| `int32_t C_RegisterQuestion(const char* text, const char* const* choices, const int32_t* next_msgs, int count)` | Returns the message number, or -1. Picking choice *i* goes on to `next_msgs[i]`, or ends the conversation for -1. |
| `int32_t C_RegisterChoice(const char* text)`, `void C_UnregisterChoice(int32_t choice_no)` | A choice text of up to 16 characters, for writing the tags by hand: `<<SETSELSTR2 [40 00 40 01]>><<BTN>><<OPENCHOICE>><<SETNEXTMSG0 [40 05]>><<SETNEXTMSG1 [40 06]>><<FORCENEXT>>`, as the game's own messages do. |
| `int C_GetLastChoice(void)` | The choice picked last, from 0. The `ChoiceSelected` event reports each pick with the message it was made in. |

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
| `id` | Unique key. The item number assigned to it is stored in `custom_furniture.json` next to the game and in the town files that use it, so saved rooms keep the right furniture across sessions. Never reuse an id for a different piece. |
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
| `void C_UnregisterFurniture(mActor_name_t item)` | Placed copies draw as the placeholder until it is registered again. |

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
| `id` | Unique key. The item number assigned to it is stored in `custom_items.json` next to the game and in the town files that use it. Never reuse an id for a different item. |
| `name` | Name shown in game, up to 16 characters. |
| `article` | `mIN_ARTICLE_*`. |
| `price` | Price in Bells. 0 means Nook takes it for free. |
| `shape` | `mNT_ITEM_TYPE_*`, the model used when the item lies on the ground or is in the player's hand (for example `mNT_ITEM_TYPE_BAG`, `_BOX`, `_PRESENT`, `_SHELLA`). 0 uses the generic one. |
| `icon_tex`, `icon_pal` | Inventory icon: a 32x32 CI4 texture and its 16 colour palette. Use your own PNG through `#asset` with `CI4` and `CI4_PAL` (see [Textures](#textures)), or the game's own icons through `C_ASSET` (the `inv_mwin_*_tex` / `inv_mwin_*_pal` assets). NULL uses a default icon. |
| `on_use` | Optional. Adds "Use" to the pocket menu; called with the item and its pocket slot after the menu closes. |
| `not_for_sale` | Optional. The catalog shows "Not for Sale" and it can't be ordered. |
| `umbrella_tex`, `umbrella_pal` | Optional. A 32x32 CI4 canopy laid out like an Able Sisters umbrella design, and its palette. The item becomes an umbrella the player can equip and open in the rain, like the example's tee umbrella. |

| Function | Description |
|---|---|
| `mActor_name_t C_RegisterItem(const C_ItemDef* def)` | Returns the item number, or `EMPTY_NO`. Up to 207 custom items fit (`C_ITEM_FIRST` to `C_ITEM_LAST`). |
| `void C_UnregisterItem(mActor_name_t item)` | Copies in the pockets or on the ground show as unknown until it is registered again. |

Indoors, custom items only offer Grab and Use, since rooms have no model to show them on the floor. Outdoors, items with `on_use` offer Grab, Use, Drop and Quit; items without it get the regular item menu. They don't appear in quests or on the shop floor, but once the player owns one it is listed on the catalog's Items page and can be ordered.

## Shop and catalog (`port/api/shop.h`)

```c
C_AddShopStock(sTable, 50);   // on sale on about half of the days
```

| Function | Description |
|---|---|
| `int C_AddShopStock(mActor_name_t item, int chance)` | Puts furniture, a carpet, a wallpaper or a shirt, custom or built-in, in Nook's rotation. `chance` is the percentage of days it is on sale; it takes the place of one of the day's regular items of the same kind. Returns `FALSE` for any other kind of item. |
| `void C_RemoveShopStock(mActor_name_t item)` | Takes an item out of the rotation. Copies already on sale stay until the next restock. |
| `int C_RestockShop(void)` | Rolls the stock again, as on a new day. Returns `FALSE` while the player is inside a shop, or before a game is loaded. |
| `void C_AddToCatalog(mActor_name_t item)` | Adds an item to the current player's catalog as if they had owned it. |
| `int C_IsInCatalog(mActor_name_t item)` | Whether the item is in the current player's catalog. |

Stock is rolled when Nook restocks, so a newly added item shows up from the next day or after `C_RestockShop`. Only furniture, carpets, wallpapers and shirts can go on the shop floor: each display spot draws its own models, and custom items have none. Nook shows a custom carpet or wallpaper on the shop's floor or wall when the player asks to see it, like the built-in ones.

Custom furniture the player has owned is listed at the end of the catalog's Furniture page, custom carpets, wallpapers and shirts at the end of their own pages, and custom items at the end of the Items page. All of them can be ordered at their `price` and arrive by mail the next day, like built-in orders. Which custom pieces each player has owned is kept in the town file, next to the player, and travels with them in their passport. Entries from the older `custom_collection.json` next to the game are read once and saved into the towns from then on. Entries whose mod isn't loaded are hidden until it is.

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
| `id` | Unique key. The item number assigned to it is stored in `custom_carpets.json` or `custom_wallpapers.json` next to the game and in the town files that use it, so rooms keep the right floor and wall across sessions. Never reuse an id for a different design. |
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
| `void C_UnregisterCarpet(mActor_name_t item)`, `void C_UnregisterWallpaper(mActor_name_t item)` | Rooms using it show the default floor or wall until it is registered again. |

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
| `id` | Unique key. The item number assigned to it is stored in `custom_clothes.json` next to the game and in the town files that use it. Never reuse an id for a different shirt. |
| `name`, `article` | Name shown in game, up to 16 characters, and its `mIN_ARTICLE_*`. |
| `price` | Price in Bells. |
| `texture`, `palette` | A 32x32 CI4 texture and its 16 colour palette, laid out like an Able Sisters design: the texture wraps around the body, with the chest in the middle. |
| `not_for_sale` | Optional. The catalog shows "Not for Sale" and it can't be ordered. |

| Function | Description |
|---|---|
| `mActor_name_t C_RegisterCloth(const C_ClothDef* def)` | Returns the item number, or `EMPTY_NO`. Up to 240 custom shirts fit. |
| `void C_UnregisterCloth(mActor_name_t item)` | Copies in the pockets show as unknown until it is registered again. |

The game's clothing numbers are all taken, so custom shirts use the free end of the diary category (0x2B10 to 0x2BFF) and the game treats them as clothing everywhere: the player wears them from the pockets or the inventory background, Nook sells them on a mannequin and lets the player try them on, they can be placed at home on a mannequin, listed and ordered in the catalog, and given or mailed to villagers, who wear them. Villagers don't pick custom shirts on their own, and quests and Blanca only use the built-in ones.

## Villagers (`port/api/villager.h`)

```c
static C_VillagerDef sMintDef;

sMintDef.id = "mymod.mint";
sMintDef.name = "Mint";
sMintDef.catchphrase = "minty";
sMintDef.base = NPC_BOB;
sMintDef.looks = mNpc_LOOKS_KO_GIRL;
sMintDef.palette = sMintPalette;
sMintDef.house_type = 2;
sMintDef.house_palette = 3;
sMintDef.wallpaper = EMPTY_NO;
sMintDef.carpet = EMPTY_NO;
sMintDef.talk_type = -1;
sMint = C_RegisterVillager(&sMintDef);
```

A custom villager starts from one of the 236 built-in villagers, its `base`: the base's species model, animations, house and room layout, and everything below can be changed, down to the model itself. Dialogue in this game depends on the personality, so a custom villager talks like the other villagers of its personality, with its own name and catchphrase.

| Field | Description |
|---|---|
| `id` | Unique key. The villager id assigned to it is stored in `custom_villagers.json` next to the game and in the town files that use it. Never reuse an id for a different villager. |
| `name` | Name shown in game, up to 8 characters. |
| `catchphrase` | Added to the end of sentences, up to 10 characters. Villagers can still change it over time, like built-in ones. |
| `base` | `NPC_*` id of the built-in villager it is built on, from `m_name_table.h`. |
| `looks` | Personality, `mNpc_LOOKS_*` from `m_npc_personal_id.h`: normal, peppy, lazy, jock, cranky or snooty. It also decides whether it is a boy or a girl. |
| `cloth` | Shirt worn when moving in, built-in or custom. `EMPTY_NO` for the base's. |
| `texture` | Body texture replacing the base's, in the same size and format (for example the 2048x1 CI4 strip of most villagers). NULL for the base's. Raw arrays must be 2-byte aligned, like `C_ASSET` and `#asset` paths. |
| `palette` | 16 colour palette replacing the base's, as an asset or raw big-endian RGBA5551 bytes. Swapping only the palette recolours the base. NULL for the base's. |
| `voice` | Voice number, 0 for the base's. |
| `house_type`, `house_palette` | House shape and colours, 0 to 4 each, -1 for the base's. |
| `wallpaper`, `carpet` | Wallpaper and carpet of its room, built-in or custom items, `EMPTY_NO` for the base's. |
| `starter` | Can be one of the first villagers of a new town. Otherwise it only moves in later. |
| `room`, `room_top` | Its own room layout: two 16x16 grids of item numbers, the furniture on the floor and the items on top of it. The room is the top-left 8x8: `RSV_WALL_NO` along the edges, `RSV_NO` for empty floor, the door gap at (3,7) and (4,7) with `EXIT_DOOR` below it at (3,8) and (4,8). Custom furniture works. NULL for the base's. |
| `skeleton` | A model replacing the base's: a `cKF_Skeleton_R_c` from the game or the mod archive (`C_ASSET`). It must use the villager rig, 26 joints in the same order as every built-in villager, so the shared animations, held items and accessories work on it. NULL for the base's. |
| `eyes`, `mouths` | 8 eye and 6 mouth textures in the model's format. NULL for the base's. |
| `scale`, `eye_height`, `col_radius`, `col_height` | Model scale (most villagers use 0.0095), height of the eyes, and the collision cylinder. 0 for the base's. |
| `talk_type`, `mouth_anim` | Whether the mouth moves when talking (0 no, 1 or 2 the two styles) and which of the two mouth animations plays (1 or 2). -1 and 0 for the base's. |
| `animations` | `aNPC_ANIM_NUM` entries (from `ac_npc_anim_def.h`) of `aNPC_Animation_c`, each replacing one of the shared animations for this villager, NULL entries keep the base's. The example makes Mint run instead of walking with `sMintAnimations[aNPC_ANIM_WALK1] = sRunAnimation`. NULL for the base's. |

| Function | Description |
|---|---|
| `mActor_name_t C_RegisterVillager(const C_VillagerDef* def)` | Returns the villager id, or `EMPTY_NO`. Up to 17 custom villagers fit. |
| `void C_UnregisterVillager(mActor_name_t npc_id)` | A town it lives in shows it as the base villager Bob until it is registered again. |
| `int C_ReplaceVillager(mActor_name_t old_npc_id, mActor_name_t new_npc_id)` | Moves a villager, built-in or custom, into the house of a town villager in place of them, as if they had just moved in. The town shows it the next time it loads. |

The example turns Mint into a green dog: the dog species' skeleton, texture, eyes and mouths from the game's own assets, with a recoloured palette, in a room of her own with two mod tables and the mod carpet and wallpaper.

### Models and animations of your own

```c
static const u8 sBoltModel[] = { #asset "models/bolt.gltf" NPC_MODEL };
static const u8 sBoltWalk[] = { #asset "models/bolt.gltf" NPC_ANIM };

sBoltAnimations[aNPC_ANIM_WALK1] = sBoltWalk;
sBoltDef.skeleton = sBoltModel;
sBoltDef.animations = sBoltAnimations;
```

`#asset "file.gltf" NPC_MODEL` turns a glTF model into a villager model when the mod is built (Pillow is required), and `#asset "file.gltf" NPC_ANIM` turns the file's first animation into a villager animation (`tools/npc_model.py anim` can pick one by name). The model becomes a skeleton, a mesh per joint and a CI4 texture per material, all inside the mod archive, and the villager draws with them and with the base's eyes and mouths.

A model must use the villager rig:
- 26 joints named `root`, `pelvis`, `hip_l`, `leg_l`, `foot_l`, `toe_l`, `hip_r`, `leg_r`, `foot_r`, `toe_r`, `tail_base`, `tail_1`, `tail_2`, `chest`, `shoulder_l`, `arm_l`, `hand_l`, `shoulder_r`, `arm_r`, `hand_r`, `hand_r_end`, `neck`, `neck_2`, `neck_end`, `head` and `head_end`, parented as in the built-in villagers, with bones along +X and no rotation in the rest pose; a parent node above `root` may carry the 90° turn that stands the rig up.
- 1 unit in the file is 1 mm in the game (a villager is about 1.5 units tall), and the joint positions come from the file.
- Meshes are skinned to the joints (each vertex follows its heaviest joint) or parented under a joint; up to 64x32 texels per texture, reduced to 16 colours. A material named `eye` or `mouth` shows the villager's animated eyes or mouth instead of a texture of its own.

`python3 tools/npc_model.py rig <ac.o2r> <species> rig.gltf` writes a reference rig with the meshes and textures of a built-in species (`cat_1`, `dog_1`, ...) from your own extracted game archive, to start from in Blender; `tools/npc_model.py model` and `anim` are the converters the build runs. `tools/make_bolt.py` in this folder builds the example's robot from boxes, with a walk that swings his arms and legs.

Animations are keyed per frame at 30 frames a second from the glTF's rotation (and `root` translation) channels, in the game's rotation order, so they play on any villager with the rig. Eye and mouth sequences and footstep sounds aren't carried over.

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
| `id` | Unique key. The item number assigned to it is stored in `custom_fish.json` or `custom_insects.json` next to the game and in the town files that use it. Never reuse an id for a different creature. |
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
| `void C_UnregisterFish(mActor_name_t item)`, `C_UnregisterInsect` | Caught copies show as unknown until it is registered again. |
| `int C_GetMuseumDonator(mActor_name_t item)` | Who gave a fish or insect, built-in or custom, to the museum: 0 for nobody yet, 1 to 4 for a player, 5 for a player who left. |

Custom fish and insects are caught with the rod or the net, show their catch text, go into the pockets, can be released, sold to Nook, or placed at home, where they show as the base's aquarium or cage and count as fish or insects at the Happy Room Academy. Blathers takes them for the museum and talks about them like their base; who gave each one is kept in the town file. Some things stay with the 40 built-in ones:
- The museum's tanks and cases only show the built-in ones.
- The fish and insect pages in the pockets, the golden net and rod, and "caught them all" only count the built-in ones.
- Villagers don't ask for custom ones in trades, and Chip's fishing tourney only judges the bass.

`FishSpawnList` and `InsectSpawnList` in `src/port/events/EVENTS.md` show the full lists of candidates and let a mod change them.

## Structures (`port/api/structure.h`)

```c
static const u8 sKioskModel[] = { #asset "models/kiosk.gltf" MODEL };
static const u8 sKioskGlass[] = { #asset "models/kiosk.gltf" MODEL_XLU };
static const u8 sKioskWinterModel[] = { #asset "models/kiosk_winter.gltf" MODEL };

sKioskRoomDef.layout = sKioskRoom;
sKioskRoomDef.wallpaper = sWallpaper;
sKioskRoomDef.carpet = sCarpet;

sKioskDef.id = "example.kiosk";
sKioskDef.name = "Kiosk";
sKioskDef.model = sKioskModel;
sKioskDef.model_xlu = sKioskGlass;
sKioskDef.winter_model = sKioskWinterModel;
sKioskDef.width = 3;
sKioskDef.depth = 2;
sKioskDef.room = &sKioskRoomDef;
sKiosk = C_RegisterStructure(&sKioskDef);

C_PlaceStructure(sKiosk, block_x, block_z, ut_x, ut_z);
```

A structure is a building or landmark on the town map, like the game's houses, shops and statues. Its item number goes into the town's map, so it stays where it was placed: the town saves it and spawns it whenever its acre is near, and on wide windows it is there before you cross into its acre, like the built-in buildings.

The tile that holds the item number is the anchor, the middle of the structure's front row. The footprint covers `width` tiles centred on the anchor (an even width reaches one tile further east) and `depth` rows from the anchor back to the north. The footprint is raised so nobody walks into it, and nothing can be dropped or grow there. A structure with a room has its door in the front row, in the anchor's column or `door_x` columns east (negative: west) of it: walking into it from the tile in front opens the room, and that tile is kept free too.

A structure with a `name` is listed on the town map, in the panel of the acre it stands in, with a marker where it stands. An acre lists up to four rows, its villagers first; the acre of the players' houses and acres that show a shop, the police station or another of the game's buildings don't list it.

| Field | Description |
|---|---|
| `id` | Unique key. The item number assigned to it is stored in `custom_structures.json` next to the game and in the town files that use it. Never reuse an id for a different structure. |
| `name` | Shown on the town map, up to 8 characters. NULL keeps it off the map. |
| `model`, `model_xlu` | Display lists of the structure, opaque and translucent, drawn with their origin at the centre of the anchor tile on the ground. `#asset "file.gltf" MODEL` and `MODEL_XLU` make them from a glTF file (see below). `model_xlu` can be NULL. |
| `winter_model`, `winter_model_xlu` | Models used in winter, for a snowy roof. NULL keeps `model` or `model_xlu`. |
| `width`, `depth` | Footprint in tiles, 1 to 9 each. |
| `door_x` | Column of the door in the front row, counted from the anchor: 0 for the anchor's, 1 for the next one east, -1 west. It must be inside the footprint. |
| `room` | The room behind the door, a `C_StructureRoomDef`. NULL if it can't be entered. |

The room is a villager house room: the game's 8x8 room with walls, wallpaper and carpet, or a room with a model of your own.

| Field | Description |
|---|---|
| `layout`, `layout_top` | 16x16 item numbers like a villager's room: the furniture on the floor and the items on top of it. `RSV_WALL_NO` marks walls, `RSV_NO` free floor, and `EXIT_DOOR` the tiles the player leaves through, below the gap in the front wall. Custom furniture works. NULL `layout` is an empty room with walls around it and the exit in the middle of the front wall; NULL `layout_top` has nothing on top. |
| `wallpaper`, `carpet` | Wallpaper and carpet items, built-in or custom. `EMPTY_NO` for the first of each. |
| `model`, `model_xlu` | Room model drawn instead of the walls and floor, from `#asset "file.gltf" ROOM_MODEL` and `ROOM_MODEL_XLU`. NULL for the game's room. |
| `width`, `depth` | Room size in tiles with a model, walls included: up to 16 across and 15 deep. 0 for 8. The camera follows the player in rooms larger than the screen. |
| `resident` | A villager (built-in or custom) found in the room while they live in town, standing by the door when you come in. `EMPTY_NO` for nobody. |

The player comes in through the gap in the front wall: the `EXIT_DOOR` tiles of the front-most row that has any are the way out, and the player appears in the row above them, in the middle of the gap. `layout = NULL` makes a two-tile gap in the middle of the front wall, like the game's rooms; a gap of two tiles means walking out near its middle, so make it wider (more open wall tiles with `EXIT_DOOR` below) for an easier exit. A layout without any `EXIT_DOOR` gets the middle exit added. A room with a model takes its collision from the layout: inside `width` x `depth`, every tile is floor except the `RSV_WALL_NO` ones, so walls and solid parts of the model (the workshop's workbench) are marked in the layout. The room's light comes on at night like a villager's.

| Function | Description |
|---|---|
| `mActor_name_t C_RegisterStructure(const C_StructureDef* def)` | Returns the structure's item number (0x5900 to 0x5FFF), or `EMPTY_NO`. |
| `void C_UnregisterStructure(mActor_name_t item)` | Where it stands the town keeps its place, empty, until it is registered again. |
| `int C_CanPlaceStructure(mActor_name_t item, int block_x, int block_z, int ut_x, int ut_z)` | Whether it fits with its anchor on tile `ut_x`, `ut_z` (0 to 15) of acre `block_x` (1 to 5), `block_z` (1 to 6): the footprint and the door tile are empty, flat grass or soil inside that acre, and the player isn't standing there. Only in town. |
| `int C_PlaceStructure(...)` | Places it there if it fits. In the player's acre it appears right away. |
| `int C_PlaceStructureAt(mActor_name_t item, float x, float z)` | Same, with the anchor on the tile under a world position. |
| `int C_RemoveStructure(int block_x, int block_z, int ut_x, int ut_z)` | Removes the structure anchored there and frees its tiles. |
| `int C_FindPlacedStructure(mActor_name_t item, int* block_x, int* block_z, int* ut_x, int* ut_z)` | Finds where a structure stands. Any of the pointers can be NULL. |

The example's **Build Kiosk** and **Build Workshop** buttons place them on the free spot nearest to the tiles in front of you (see `BuildNearPlayer` in `main.c`), and **Remove Structures** takes both away. The kiosk has glass windows and a snowy roof in winter, and its room is the game's room with the mod carpet and wallpaper and two mod tables. The workshop's door is in its east column, and its room is 10x9 tiles with its own floor, walls, workbench and glass window, the mod NES, and Bolt at work once he lives in town (**Invite Bolt**).

### Structure and room models

`#asset "file.gltf" MODEL` and `ROOM_MODEL` turn a glTF file into a display list when the mod is built (Pillow is required). Every mesh in the file is used, in world units: a tile is 40 units across, y is up and z points south, towards the camera.

- A structure's origin is the centre of its anchor tile on the ground. Its front wall, with the door, faces south at z = 20, and nothing may lie farther than 327 units from the origin.
- A room's origin is its north-west corner at floor height: tile (x, z) of the layout covers x to x + 40 and z to z + 40. The game's rooms have no front wall, so the camera sees in. Nothing may lie farther than 2047 units from the origin.
- Each material's base colour texture becomes a CI4 texture of up to 64x64 texels with its own 16 colours, repeating across the UVs; a material without a texture gets its base colour. Lighting comes from the vertex normals.
- Materials with `alphaMode` `BLEND` are translucent: `MODEL_XLU` and `ROOM_MODEL_XLU` make a display list of just those, drawn after the opaque ones with the material's base colour alpha, and `MODEL` and `ROOM_MODEL` leave them out. Use both declarations on the same file.

`tools/npc_model.py static` and `room` (with `--xlu` for the translucent part) are the converters the build runs, and `tools/make_structures.py` in this folder builds the example's models from boxes.

## NES games (`port/api/famicom.h`)

Register an iNES (`.nes`) ROM and you get a ROM number that the game's emulator can run. Put the file in the mod's `assets/` folder and register it by its path in the archive, or pass the bytes directly:

```c
static int sMyGame;

MOD_INIT() {
    sMyGame = C_RegisterFamicomGameFile("mymod.game", "famicom/mygame.nes");
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
| `void C_UnregisterFamicomGame(int rom_no)` | Frees the ROM number. |
| `void C_FamicomFurnitureMove(FTR_ACTOR* ftr_actor, ACTOR* my_room, GAME* game, int rom_no)` | Makes a furniture piece an NES that plays `rom_no`. |

`id` names the game's save file (`save/famicom/Bellwood_<id>.bin`), so keep it the same between versions of the mod. Games with the battery flag in their iNES header keep their 8 KB battery RAM there. ROM numbers are handed out in load order, so look them up at startup rather than hard-coding them. The `FamicomStart` event can also redirect any of the game's own NES furniture to a registered ROM.

Quit a game with L, R and Z, like the built-in ones.

## Visitors (`port/api/visitor.h`)

A visitor is a character with a villager's look who doesn't live in town, like the game's own guests. It uses the look's model, name, voice and personality; a custom villager gives a visitor a look and name of its own. Bolt visits the example's town every afternoon:

```c
C_VisitorDef def = { 0 };
def.look = sBolt;
def.schedule = C_VISITOR_WALK_WANDER;
def.talk_msg = C_RegisterMessage("Beep boop!");
sBoltVisitor = C_RegisterVisitor(&def);

C_VisitDef visit = { 0 };
visit.hours = 0x00FFF000;   // noon to midnight, in a different acre each day
C_ScheduleVisitor(sBoltVisitor, &visit);
```

| Field | Description |
|---|---|
| `look` | Villager id, built-in or custom. |
| `cloth` | Shirt, built-in or custom, `EMPTY_NO` for the look's. |
| `schedule` | `C_VISITOR_STAND`, `C_VISITOR_WANDER` (looks around its spot) or `C_VISITOR_WALK_WANDER` (walks around the acre). |
| `talk_msg` | Message shown when the player talks to it. |
| `on_talk` | Optional. Called when the player talks to it; returns the message to show, so it can change with the day or a question's answer. |

| Function | Description |
|---|---|
| `mActor_name_t C_RegisterVisitor(const C_VisitorDef* def)` | Returns the visitor's name (0xDF00-0xDF3F), or `EMPTY_NO`. Up to 64 visitors. |
| `void C_UnregisterVisitor(mActor_name_t visitor)` | Also removes it from the scene. |
| `int C_SpawnVisitor(mActor_name_t visitor, int block_x, int block_z, int ut_x, int ut_z)` | Puts it at a tile of an acre now. It appears once its clothes are loaded, a few frames later, and leaves with the acre. |
| `ACTOR* C_FindVisitor(mActor_name_t visitor)`, `void C_RemoveVisitor(mActor_name_t visitor)` | |
| `int C_ScheduleVisitor(mActor_name_t visitor, const C_VisitDef* visit)` | Makes it come to town by itself at the times `visit` allows: `months`, `days` of the month, `weekdays` and `hours` as bit masks, 0 for any. It stands in acre `block_x`, `block_z`, or a different acre each day when they are 0, on tile `ut_x`, `ut_z` or a free one when they are -1. It is there whenever the player walks into that acre at those times. |
| `void C_UnscheduleVisitor(mActor_name_t visitor)`, `int C_GetVisitorAcre(mActor_name_t visitor, int* block_x, int* block_z)` | Where a scheduled visitor is now, `FALSE` when it isn't visiting. |

## Letters (`port/api/mail.h`)

```c
C_LetterDef letter = { 0 };
letter.header = "Dear \n,";
letter.body = "Thanks for trying the example!";
letter.footer = "The mod team";
letter.sender = "Mod Team";
letter.present = sGem;
C_SendLetter(&letter, -1);
```

| Field | Description |
|---|---|
| `header` | Up to 24 characters. The recipient's name goes where it has a `\n`, or after it. |
| `body`, `footer` | Up to 192 and 32 characters. Dialogue tags work. |
| `sender` | Name shown as the sender, up to 8 characters. |
| `present` | Item attached, `EMPTY_NO` for none. |
| `paper` | `ITM_PAPER*` stationery, `EMPTY_NO` for the first design. |

`int C_SendLetter(const C_LetterDef* def, int player)` puts it in the mailbox of player 0 to 3, or -1 for the one playing. When the mailbox is full the post office keeps it for the next delivery. Returns `FALSE` when it can't be sent.

## Sound and music (`port/api/sound.h`)

Sounds are WAV, MP3 or FLAC files in the mod's `assets/` folder. They are decoded when registered and mixed into the game's output at 32 kHz, following the music and sound effect volume settings.

```c
sChime = C_RegisterSound("sound/chime.wav");
sTheme = C_RegisterSound("sound/theme.mp3");
C_PlaySound(sChime, 1.0f, 0.0f);
C_ReplaceBgm(BGM_FIELD_00, sTheme, 0);   // loops from the start
```

| Function | Description |
|---|---|
| `int C_RegisterSound(const char* path)`, `C_RegisterSoundData(data, size)` | Returns a sound number, or -1. |
| `void C_UnregisterSound(int sound)`, `uint32_t C_SoundLength(int sound)` | Length in samples at 32 kHz. |
| `int C_PlaySound(int sound, float volume, float pan)` | A sound effect, volume 0 to 1 and pan -1 to 1. Returns a voice. `C_StopSound`, `C_IsSoundPlaying` take it. |
| `void C_PlayMusic(int sound, int32_t loop_start)`, `C_StopMusic` | Music of the mod's own; the game's music is silent while it plays. `loop_start` is the sample it jumps back to at the end, -1 to play once. |
| `void C_ReplaceBgm(uint8_t bgm, int sound, int32_t loop_start)` | Plays the sound whenever the game starts that background music, until the game stops or changes it. -1 gives the game's music back. |

Sounds stay decoded in memory, about 7.5 MB per minute.

## Town data (`port/api/savedata.h`)

Values a mod keeps for a town go in the town's save file, under keys of the mod's own, like ids. The town's values are read before the `SaveLoaded` event fires and written whenever the game saves the town; a new town starts with none.

```c
static void OnSaveLoaded(IEvent* event) {
    C_SaveDataSetInt("mymod.visits", C_SaveDataGetInt("mymod.visits", 0) + 1);
}
```

| Function | Description |
|---|---|
| `int C_SaveDataHas(const char* key)`, `void C_SaveDataRemove(const char* key)` | Checks for or removes a value. |
| `int32_t C_SaveDataGetInt(const char* key, int32_t defaultValue)`, `C_SaveDataSetInt` | Whole numbers. |
| `float C_SaveDataGetFloat(const char* key, float defaultValue)`, `C_SaveDataSetFloat` | Decimal numbers. |
| `const char* C_SaveDataGetString(const char* key, const char* defaultValue)`, `C_SaveDataSetString` | UTF-8 text. The string stays valid until the key is set again or another town loads. |
| `uint32_t C_SaveDataGetBytes(const char* key, void* out, uint32_t capacity)`, `C_SaveDataSetBytes` | Raw bytes. Get copies up to `capacity` bytes and returns the stored size. |

## Player and town (`port/api/player.h`, `port/api/town.h`)

Mods can call the game's own functions and read its data directly, but those follow the decompilation and can change. These keep their signatures across versions.

| Function | Description |
|---|---|
| `ACTOR* C_GetPlayer(void)` | The player's actor, or NULL outside of gameplay. |
| `int C_GetPlayerPosition(float* x, float* y, float* z)` | Returns `FALSE` outside of gameplay. |
| `int C_GiveItem(mActor_name_t item)` | Puts the item in the first free pocket and returns the pocket, or -1 when the pockets are full. |
| `int C_TakeItem(mActor_name_t item)` | Takes one out of the pockets. Returns `FALSE` when the player doesn't carry it. |
| `int C_CountItem(mActor_name_t item)` | How many the player carries. |
| `uint32_t C_GetWallet(void)`, `void C_SetWallet(uint32_t bells)` | Bells in the wallet, up to 99,999. |
| `int C_GetScene(void)` | The `SCENE_*` being played, or -1. |
| `int C_InTown(void)` | `TRUE` when the player is outside in town. |
| `void C_GetTime(C_Time* time)` | The game's date and time. |
| `int C_GetTownVillagers(mActor_name_t* ids, int capacity)` | Writes the ids of the villagers living in town and returns how many live there. |

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
