# Bellwood Event Reference

Events are dispatched via `CALL_EVENT` or `CALL_CANCELLABLE_EVENT` and consumed via `REGISTER_LISTENER`. All events fire on the game thread.

**Cancellable events**: a listener can set `event->Cancelled = true` to block the game's default behaviour. The exact effect is described per event. Non-cancellable events carry data for observation only.

**Mutable pointer fields**: fields typed as a pointer (e.g. `int32_t* msgNo`) let listeners write through them to change what the game uses. You do not need to cancel the event to redirect a value; just write to `*ev->field`.

**Polled events**: events marked _(polled)_ are detected by comparing game state once per play frame, so they fire on the frame after the change and do not fire outside the play state.

```cpp
#include "port/events/Events.h"

REGISTER_LISTENER(EventName, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
    EventName* ev = (EventName*)event;
    // read / modify / cancel
    event->Cancelled = true;
});
```

Game structs are passed through forward-declared tags (`struct actor_s*`, `struct player_actor_s*`, `struct game_play_s*`, `struct door_data_s*`). Include the matching game header (`m_actor.h`, `m_player.h`, `m_play.h`, `m_scene.h`) to read them.

---

## Engine Events

### `EngineReady`
Fires once after the engine, the resource manager and the GUI are initialised.

---

### `KeyboardInput`
```c
uint32_t scancode;
```
Fires when a keyboard key is pressed (desktop only).

---

### `GameLoopTick`
Fires every main-loop iteration, before the game frame runs. Fires even while the game is not running yet (disc extraction, setup screens).

---

### `GameFrameUpdate`
Fires once per game logic frame, before the current game state runs.

---

### `GameStateChanged`
```c
int32_t prev;
int32_t now;
```
Fires after a new game state has been constructed. Values are `BellwoodGameState` (`BW_GAME_STATE_PLAY`, `BW_GAME_STATE_TRADEMARK`, ...); `prev` is `BW_GAME_STATE_NONE` on boot. Moving between scenes reconstructs the play state, so `BW_GAME_STATE_PLAY -> BW_GAME_STATE_PLAY` is normal.

---

### `GameReset`
Fires when the game is soft reset or rebooted, before the running state is torn down.

---

### `ControllerRead`
```c
OSContPad* pads;
int32_t count;
```
Fires after the controllers are read, before the game sees the new input. Write through `pads` to change what the game reads this frame.

---

## Actor Events

### `ActorProfileLookup`
```c
int16_t profile;
struct actor_dlftbl_s* table;
```
Fires when an actor is spawned with a profile number past the built-in table. Set `table` to the profile's table entry to make the spawn succeed. Custom actors registered through `port/api/actor.h` are answered here.

---

### `NpcProfileLookup`
```c
uint16_t name;
int16_t profile;
```
Fires when an NPC is set up whose special NPC name the game has no actor for. Set `profile` to spawn it; left at -1 it isn't spawned. Visitors from `port/api/visitor.h` answer here.

---

### `MaskNpcLookup`
```c
uint16_t name;
void* mask;
```
Fires when the game looks for the stand-in of a special NPC name and has no mask NPC registered for it. Set `mask` to a `mNpc_MaskNpc_c` to give the name a villager's look, as the game does for its own guests.

---

### `CustomNpcTalk`
```c
struct actor_s* actor;
int32_t msgNo;
```
Fires when the player talks to an NPC spawned through `port/api/visitor.h`. Set `msgNo` to the message to show.

---

### `ActorSpawn` _(cancellable)_
```c
int16_t* profile;
float* pos;      // x, y, z
int16_t* rot;    // x, y, z
uint16_t* name;
int16_t* arg;
```
Fires before an actor is allocated. Write through the pointers to spawn something else or somewhere else; a profile that doesn't exist is ignored.
**Cancel**: the actor is not created and the spawn returns `NULL`.

---

### `ActorInit`
```c
struct actor_s* actor;
```
Fires after an actor's constructor has run. Actors whose data bank is still loading are constructed on a later frame, so this can fire after `ActorSpawn` by a few frames.

---

### `ActorUpdate` _(cancellable)_
```c
struct actor_s* actor;
```
Fires every frame before an actor's update function.
**Cancel**: the actor does not update this frame.

---

### `ActorDraw` _(cancellable)_
```c
struct actor_s* actor;
```
Fires every frame before an actor is drawn.
**Cancel**: the actor and its shadow are not drawn this frame.

---

### `ActorDestroy`
```c
struct actor_s* actor;
```
Fires when an actor is destroyed, before its save and destructor functions run.

---

## Furniture Events

### `FurnitureLookup`
```c
int32_t index;
const struct C_FurnitureDef_s* furniture;
```
Fires whenever the game needs data for a furniture index past the built-in table (model, size, name, price and so on). Set `furniture` to the definition that owns the index. Custom furniture registered through `port/api/furniture.h` is answered here; unanswered indices draw as the placeholder and show as unknown items.

---

## Item Events

### `ItemLookup`
```c
uint16_t item;
const struct C_ItemDef_s* definition;
```
Fires whenever the game needs data for an item in the custom item range, `C_ITEM_FIRST` to `C_ITEM_LAST` (name, price, icon, ground model, pocket menu). Set `definition` to the definition that owns the item. Custom items registered through `port/api/item.h` are answered here; unanswered items show as unknown.

---

### `ItemCollected`
```c
uint16_t item;
```
Fires whenever the player gets an item and the game records it for the catalog: when it lands in the pockets unwrapped, when clothes are worn, when wallpaper or carpet is applied, and so on. It fires for every item, built-in or custom, except fish and insects. Custom furniture and custom items are recorded per player from here and listed in the catalog.

---

### `ItemUse`
```c
uint16_t item;
int32_t slot;
```
Fires when "Use" is picked for a custom item in the pockets. The pocket menu is already closing. Items registered through `port/api/item.h` with an `on_use` callback get it called from here.

---

### `MuseumDonatorLookup`
```c
uint16_t item;
int donator;
```
Fires when the museum checks an item outside its own fossils, art, fish and insects. A listener that owns the item sets `donator`: 0 when nobody donated it yet, 1 to 4 for the player who did, 5 for a player who was removed. Left at -1, the museum doesn't take the item. Custom fish and insects answer here.

---

### `MuseumDonate`
```c
uint16_t item;
int donator;
bool accepted;
```
Fires when a player donates an item the museum doesn't know. Set `accepted` when the donation is recorded.

---

### `MuseumDonatorRemoved`
```c
int donator;
```
Fires when a player is removed from the town, after the museum marks their own donations as from a removed player.

---

## Interior Events

### `InteriorLookup`
```c
int32_t index;
uint8_t wall;
const struct C_InteriorDef_s* definition;
const uint8_t* data;
```
Fires whenever the game needs data for a floor index from `C_CARPET_FIRST` or a wall index from `C_WALLPAPER_FIRST` up to 255 (texture, name, price), in rooms, the catalog, the pockets and Nook's shop. The index is also the low byte of the item number. `wall` is `TRUE` for a wallpaper. Set `definition` to the carpet or wallpaper that owns the index, and `data` to its 16 colour RGBA5551 palette followed by its 64x64 CI4 tiles, four for a floor and two for a wall. Carpets and wallpapers registered through `port/api/interior.h` are answered here; unanswered indices show the default floor or wall and unknown items.

---

## Creature Events

### `CreatureLookup`
```c
uint8_t insect;
int32_t index;
int32_t furniture;
const struct C_CreatureDef_s* definition;
int32_t foundIndex;
int32_t foundFurniture;
int32_t catchMessage;
```
Fires whenever the game needs data for a fish or insect from index `C_CREATURE_FIRST` (0x2D) up to 255: its name, price, icon, the built-in fish or insect it behaves like, its catch message and its placed version. It is looked up either by `index` (the low byte of the item number, with `insect` telling fish from insects) or, with `index` at -1, by the `furniture` index of its placed version; then the listener also sets `insect`. Set `definition`, `foundIndex`, `foundFurniture` and `catchMessage` for the creature that owns it. Fish and insects registered through `port/api/creature.h` are answered here.

---

### `FishSpawnList`
```c
int32_t water;
struct gyoei_spawn_info_weight_f_s* entries;
int32_t* count;
int32_t capacity;
```
Fires every time the game picks a fish for an acre, after it lists the fish that can appear there. `water` is one of `aSOG_RANGE_PROC_*` (river, sea, offing, pond, fishing tourney, island). `entries` are `aSOG_gyoei_spawn_info_weight_f_c` with a fish type, an `aSOG_SPAWN_AREA_*` and a weight; append to them and increase `*count`, up to `capacity`. Fish registered through `port/api/creature.h` are added here.

---

### `InsectSpawnList`
```c
uint8_t island;
struct insect_spawn_info_f_s* entries;
int32_t* count;
int32_t capacity;
```
Fires when the game rebuilds the list of insects that can appear, which happens when the time of day moves to its next period (six a day). `entries` are `aSOI_insect_spawn_info_f_c` with an insect type, an `aSOI_SPAWN_AREA_*` and a weight; append to them and increase `*count`, up to `capacity`. Insects registered through `port/api/creature.h` are added here.

---

## Clothing Events

### `ClothLookup`
```c
uint16_t item;
int32_t furniture;
const struct C_ClothDef_s* definition;
uint16_t foundItem;
int32_t foundFurniture;
```
Fires whenever the game needs data for a shirt in the custom clothing range, `C_CLOTH_FIRST` (0x2B10) to `C_CLOTH_LAST` (0x2BFF): its name, price, and the mannequin that shows it in a room, the catalog or Nook's shop. It is looked up either by `item` or, with `item` at `EMPTY_NO`, by the `furniture` index of its mannequin. Set `definition`, `foundItem` and `foundFurniture` for the shirt that owns it. Shirts registered through `port/api/cloth.h` are answered here. Their textures live at texture index 264 and up, after the built-in shirts and the 8 Able Sisters designs, where the player, villagers and mannequins read them.

---

## Villager Events

### `VillagerLookup`
```c
uint16_t npcId;
const struct C_VillagerDef_s* definition;
const uint8_t* name;
const uint8_t* catchphrase;
const uint16_t* room;
const uint16_t* roomTop;
```
Fires whenever the game needs data for a villager id from `C_VILLAGER_FIRST` (0xE0EE) to `C_VILLAGER_LAST` (0xE0FE): personality, name, catchphrase, model, animations, house, room and the move-in rules. Set `definition` to the villager that owns the id, `name` and `catchphrase` to `ANIMAL_NAME_LEN` and `ANIMAL_CATCHPHRASE_LEN` bytes in the game's text encoding, and `room` and `roomTop` to its own 16x16 room layouts (the floor, and the items on top of furniture) or leave them NULL for the base's. Villagers registered through `port/api/villager.h` are answered here; a town villager whose id isn't answered shows as the base villager Bob.

---

### `VillagerMovedIn` _(polled)_
```c
uint16_t npcId;
int32_t slot;
```
Fires when a villager starts living in town: a new one moving in, one arriving from another town, or one swapped in with `C_ReplaceVillager`. `slot` is its index in the town's villager list.

---

### `VillagerMovedOut` _(polled)_
```c
uint16_t npcId;
int32_t slot;
```
Fires when a villager stops living in town.

---

### `NpcTalkRequest` _(cancellable)_
```c
struct actor_s* npc;
uint16_t npcId;
```
Fires every frame an NPC is ready to be talked to. Cancel to keep the player from talking to it.

---

### `NpcTalkStart`
```c
struct actor_s* npc;
uint16_t npcId;
```
Fires when the player starts talking to an NPC.

---

## Structure Events

### `StructureLookup`
```c
uint16_t item;
const struct C_StructureDef_s* definition;
int16_t profile;
const uint16_t* room;
const uint16_t* roomTop;
const uint32_t* roomCollision;
int16_t wall;
int16_t floor;
const uint8_t* name;
uint16_t resident;
int16_t entryX;
int16_t entryZ;
```
Fires whenever the game needs data for a structure item from `C_STRUCTURE_FIRST` (0x5900) to `C_STRUCTURE_LAST` (0x5FFF): when the town spawns it, every frame it updates and draws, and while its room is built and played. Set `definition` to the structure that owns the item and `profile` to the actor profile that draws it. Set `name` to its name for the town map (8 bytes in the game's encoding, or NULL to leave it off). For a structure with a room, set `room` and `roomTop` to its 16x16 layouts, `wall` and `floor` to the wallpaper and carpet indices, `roomCollision` to 16x16 collision units when the room has its own model (NULL keeps the villager house room's), `resident` to the villager found in it, and `entryX` and `entryZ` to where the player appears, in world units. Structures registered through `port/api/structure.h` are answered here; an item that isn't answered stays in the town's map but nothing is spawned for it.

---

## Shop Events

### `ShopStockGenerated`
```c
uint16_t* items;
int32_t count;
int32_t capacity;
int32_t furnitureCount;
uint16_t* rareItem;
int32_t shopLevel;
```
Fires after Nook rolls the shop's stock, on a new day and when the shop is upgraded. It doesn't fire on raffle days. Write to `items` or `*rareItem` to change what is for sale. The first `furnitureCount` slots hold furniture. Replace items with ones of the same kind, since each kind has its own display spots in the shop. `rareItem` is `NULL` when the shop level has no rare item. `shopLevel` goes from 0 (Nook's Cranny) to 3 (Nookington's). `C_AddShopStock` from `port/api/shop.h` is handled here.

---

### `CatalogPageBuild`
```c
int32_t page;
uint16_t* items;
int32_t* count;
int32_t capacity;
```
Fires for every catalog page while the catalog opens, after the built-in entries the player has collected are listed. `page` is one of `mCL_MENU_*` from `m_catalog_ovl.h`. Append item numbers to `items` and increase `*count` to list more entries, up to `capacity`. Entries are drawn by item type: furniture with its model, wallpaper, carpet, stationery and music with their usual previews, and custom items with their icon. Collected custom furniture, carpets, wallpapers and items are added to their pages here. Extra entries don't count toward completing a page.

---

## Player Events

### `PlayerStateRequest` _(cancellable)_
```c
struct player_actor_s* player;
int32_t* index;
int32_t priority;
```
Fires when something requests a new player main state (`mPlayer_INDEX_*`). Write `*index` to request a different state.
**Cancel**: the request is dropped.

---

### `PlayerStateChanged`
```c
struct player_actor_s* player;
int32_t prev;
int32_t now;
```
Fires after the player switches to a new main state.

---

### `PlayerMoneyChanged` _(polled)_
```c
uint32_t prev;
uint32_t now;
```
Fires when the amount of Bells in the player's wallet changes.

---

### `PlayerInventoryChanged` _(polled)_
```c
int32_t slot;
uint16_t prev;
uint16_t now;
```
Fires once for every pocket slot whose item changed.

---

### `PlayerAcreChanged` _(polled)_
```c
int32_t prevX;
int32_t prevZ;
int32_t x;
int32_t z;
```
Fires when the player walks into another acre of an outdoor field. After a scene load the previous acre is `-1, -1`.

---

### `FishCaught`
```c
uint16_t item;
```
Fires when the player catches a fish with the rod, as the catch is shown. Built-in and custom fish.

---

### `InsectCaught`
```c
uint16_t item;
```
Fires when the player catches an insect with the net, as the catch is shown. Built-in and custom insects; not the spirits.

---

### `PlayerDig`
```c
const float* pos;
uint16_t item;
```
Fires when the player digs a hole with the shovel. `item` is what came out of the ground, `EMPTY_NO` for nothing.

---

### `PlayerShakeTree`
```c
uint16_t tree;
int32_t utX;
int32_t utZ;
```
Fires when the player starts shaking a tree. `utX` and `utZ` are the tile in town units.

---

### `LetterReceived` _(polled)_
```c
int32_t player;
int32_t slot;
```
Fires when a letter lands in the mailbox of player 0-3: from the post office, a villager, the game or a mod.

---

### `LoanChanged` _(polled)_
```c
int32_t player;
uint32_t previous;
uint32_t loan;
```
Fires when a player's loan changes: a payment at the post office, or a new loan for a bigger house.

---

### `HouseSizeChanged` _(polled)_
```c
int32_t player;
int32_t previous;
int32_t size;
```
Fires when a player's house changes size (`mHm_HOMESIZE_*`).

---

### `PlayerSound`
```c
int32_t kind;
uint16_t id;
float x;
float y;
float z;
```
Fires when the player makes a sound at a place: footsteps, tools, shaking trees and so on. `kind` is `PLAYER_SOUND_EFFECT` (a sound effect, `id` as for `sAdo_OngenTrgStart`), `PLAYER_SOUND_STEP` (a footstep outdoors, `id` the ground's walk sound) or `PLAYER_SOUND_ROOM_STEP` (a footstep indoors, `id` the floor). Sounds only the player hears, such as the camera moving, don't fire it.

---

### `PlayerEffect`
```c
int32_t effect;
float x;
float y;
float z;
int32_t prio;
int16_t angle;
uint16_t item;
int16_t arg0;
int16_t arg1;
```
Fires when the player makes an effect (`eEC_EFFECT_*`): footprints, dust, splashes, tool swings and so on. The fields are the arguments of the effect clip's `effect_make_proc`.

---

## Game Events

### `SceneChangeRequest` _(cancellable)_
```c
struct door_data_s* door;
```
Fires when the game asks to leave the current scene through a door or exit. Write through `door` (`Door_data_c`) to change the destination.
**Cancel**: the player stays in the scene.

---

### `SceneLoaded`
```c
struct game_play_s* play;
int32_t scene;
```
Fires at the end of play initialisation, once the scene and its actors are set up.

---

### `SceneUnloaded`
```c
struct game_play_s* play;
int32_t scene;
```
Fires when the play state is being torn down, before anything is freed.

---

### `PlayUpdate`
```c
struct game_play_s* play;
```
Fires once per play frame, before actors and the field update.

---

### `FamicomStart` _(cancellable)_
```c
int16_t* romId;
```
Fires before a Famicom (NES) game is launched from furniture. Write `*romId` to launch a different game, including one registered through `port/api/famicom.h`.
**Cancel**: nothing is launched.

---

### `FamicomRomLookup`
```c
int32_t romId;
const uint8_t* rom;
uint32_t size;
const uint8_t* tags;
uint32_t tagsSize;
const char* saveName;
```
Fires when a ROM id in the custom range (`C_FAMICOM_FIRST` to `C_FAMICOM_LAST`, 64-127) is started. Set `rom`, `size` and `saveName` to run the game; its save data is kept in `save/famicom/<saveName>.bin`. `tags` describes what to save, in the game's NES tag format. Games registered through `port/api/famicom.h` are answered here.

---

### `NewGameWelcome` _(cancellable)_
Fires when K.K. starts his welcome on a new game.
**Cancel**: the welcome and the sound setup talk are skipped and the game starts straight away; sound and voice settings keep their defaults.

---

### `TrainRideStart` _(cancellable)_
```c
bool newTown;
```
Fires when Rover starts the train ride conversation. `newTown` is false for a new resident joining an existing town.
**Cancel**: Rover starts at the name question and the train arrives right after the last answer.

---

### `MessageLoad`
```c
int32_t* msgNo;
```
Fires before a message is read from the message table. Write `*msgNo` to load a different message; out of range values are ignored.

---

### `MessageLookup`
```c
int32_t msgNo;
const uint8_t* text;
uint32_t size;
```
Fires when a message number in the custom range (`C_MESSAGE_FIRST` to `C_MESSAGE_LAST`, 0x4000-0x7FFF) is loaded. Set `text` and `size` to the encoded message, which must end with an end code. Messages registered through `port/api/message.h` are answered here. `MessageLoaded` still fires afterwards.

---

### `MessageLoaded`
```c
int32_t msgNo;
uint8_t* text;
uint32_t size;
uint32_t capacity;
```
Fires after a message is copied into the text buffer, before its length is measured. Rewrite `text` in place to change what is shown; keep it inside `capacity` and terminated with an end code.

---

### `MessageOpened`
```c
struct actor_s* speaker;
int32_t msgNo;
```
Fires when a dialogue window starts opening. `speaker` may be `NULL`.

---

### `MessageClosed`
```c
int32_t msgNo;
```
Fires when a dialogue window starts closing.

---

### `ChoiceSelected`
```c
int32_t msgNo;  // message showing the choices
int choice;     // 0 for the first choice
```
Fires when the player picks one of the choices a message offers.

---

### `ChoiceStringLookup`
```c
int32_t strNo;
const uint8_t* text;  // set by the listener that owns the choice text
uint32_t size;        // size of text in bytes
```
Fires when a choice text past the game's own is loaded for a SETSELSTR control code. Set text and size to the encoded text, up to 16 bytes.

---

### `SubmenuOpen`
```c
int32_t* type;
int32_t param0;
int32_t param1;
```
Fires when the game asks for a menu (`mSM_OVL_*`: inventory, map, letters, shops, ...). Write `*type` to open a different menu.

---

### `SubmenuOpened`
```c
int32_t type;
```
Fires once a menu is linked in and starts running.

---

### `SubmenuClosed`
```c
int32_t type;
```
Fires when a menu closes and control goes back to the player.

---

### `ClockRead`
```c
struct OSRTCTime* time;
```
Fires whenever the game reads the system clock. Write through `time` to change what the game takes as now; growth, seasons and the daily changes follow. `C_SetTimeOffset` in `port/api/town.h` is built on it.

---

### `TimeHourChanged`
```c
int32_t prev;
int32_t now;
```
Fires when the in-game clock reaches a new hour.

---

### `TimeDayChanged`
```c
int32_t year;
int32_t month;
int32_t day;
```
Fires when the in-game date changes, including the first clock read after boot.

---

### `WeatherChanged` _(polled)_
```c
int32_t prev;
int32_t now;
int32_t prevIntensity;
int32_t intensity;
```
Fires when the town weather (`mEnv_WEATHER_*`) or its intensity changes.

---

## Save Events

### `SaveLoaded`
```c
bool hasTown;
```
Fires after the home save slot has been read into memory. `hasTown` is false for a new game.

---

### `SaveRequest` _(cancellable)_
```c
int32_t slot;  // 0 for the home file
bool hasTown;  // the file holds a town, not only passports
```
Fires before a save file is written: the town, a passport or both.
**Cancel**: the file is not written, and the game carries on as if it had been. Use it to keep a visited town (`port/api/visit.h`) from being saved.

---

### `SaveWritten`
```c
int32_t mode;
```
Fires after the town has been written to disk. `mode` is the game's save mode; `0` ends the session (save and quit).

---

### `SaveImported`
```c
const char* path;
```
Fires after a GameCube memory card save has been imported.

---

### `ResetDetected` _(cancellable)_
```c
struct private_s* player;
```
Fires when the town loads and the player last quit or reset without saving.
**Cancel**: Mr. Resetti does not visit and the reset is not counted.

---

## Field Events

### `FieldItemChanged` _(polled)_
```c
int32_t block_x;
int32_t block_z;
int32_t ut_x;
int32_t ut_z;
uint16_t prev;
uint16_t now;
int32_t cause; // FIELD_CHANGE_*
```
Fires when the item on a tile of the scene changes: in town, on the island or in a room, where the room is block 0, 0. `cause` tells what changed it, from what the player was doing on or next to the tile:

| `cause` | Meaning |
|---|---|
| `FIELD_CHANGE_GAME` | Not the player: growth, villagers, the day changing, events, mods |
| `FIELD_CHANGE_DROP` | Put down or thrown |
| `FIELD_CHANGE_PICK_UP` | Picked up, including digging something up |
| `FIELD_CHANGE_BURY` | Buried or planted in a hole |
| `FIELD_CHANGE_DIG` | Hole dug |
| `FIELD_CHANGE_FILL` | Hole filled |
| `FIELD_CHANGE_CHOP` | Hit with the axe |
| `FIELD_CHANGE_SHAKE` | Tree shaken, so something fell |
| `FIELD_CHANGE_WEED` | Weed pulled |
| `FIELD_CHANGE_FURNITURE` | Furniture pushed, pulled, turned or picked up |

---

## Satella Events

### `SatellaConnected`
Fires once Satella is connected and has accepted the player key: at startup, and again after the connection drops and comes back. Relay channels are subscribed again by then; rooms are joined again right after.

---

### `SatellaDisconnected`
Fires when the connection to Satella drops. It reconnects on its own.

---

## Audio Events

### `PlayBgmEvent` _(cancellable)_
```c
uint8_t* id;
```
Fires before background music starts. Write `*id` to play a different track.
**Cancel**: the music does not start.

---

### `StopBgmEvent` _(cancellable)_
```c
uint16_t* id;
```
Fires before background music stops.
**Cancel**: the music keeps playing.

---

### `PlaySystemSfxEvent` _(cancellable)_
```c
uint16_t* id;
```
Fires before a system (menu / UI) sound effect plays. Write `*id` to play a different sound (`NA_SE_*`).
**Cancel**: the sound does not play.

---

### `PlaySfxEvent` _(cancellable)_
```c
uint16_t* id;
const float* pos;  // x, y, z
```
Fires before a positional sound effect plays. Write `*id` to play a different sound (`NA_SE_*`).
**Cancel**: the sound does not play.

---

## Render Events

### `RenderGamePre`
```c
struct game_play_s* play;
```
Fires after the play frame's display lists are reset, before the scene is drawn.

---

### `RenderGamePost`
```c
struct game_play_s* play;
```
Fires after the play frame has been drawn, menus included.
