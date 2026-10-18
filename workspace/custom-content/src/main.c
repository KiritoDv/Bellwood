#include "mod.h"

#include "port/api/actor.h"
#include "port/api/famicom.h"
#include "port/api/furniture.h"
#include "port/api/item.h"
#include "port/api/message.h"
#include "port/api/ui.h"
#include "port/events/Events.h"
#include "graph.h"
#include "m_common_data.h"
#include "m_controller.h"
#include "m_demo.h"
#include "m_huusui_room.h"
#include "m_item_name.h"
#include "m_player_lib.h"
#include "m_private.h"
#include "m_rcp.h"
#include "sys_matrix.h"
#include <string.h>

C_ASSET(sSpinnerModel, "data/model/obj_e_ghog/obj_e_ghog_model");
C_ASSET(sSpinnerModelT, "data/model/obj_e_ghog/obj_e_ghog_modelT");
C_ASSET(sTableModel, "data/model/int_ari_table01/int_ari_table01_01_model");
C_ASSET(sTableModelT, "data/model/int_ari_table01/int_ari_table01_01T_model");
C_ASSET(sGemIcon, "data/model/inv_mwin13/inv_mwin_sango_tex");
C_ASSET(sGemPalette, "data/model/inv_mwin13/inv_mwin_sango_pal");
C_ASSET(sNesModel, "data/model/int_tak_nes01/int_tak_nes01_on_model");
C_ASSET(sNesModelT, "data/model/int_tak_nes01/int_tak_nes01_onT_model");
C_ASSET(sNesGameModel, "data/model/int_tak_nes01/int_tak_nes01_game_on_model");
C_ASSET(sNesTexture, "data/model/int_tak_nes01/int_tak_nes_ds_tex_rgb_i4");
C_ASSET(sNesPalette, "data/model/int_tak_nes01/int_tak_nes_cl_pal");

typedef struct {
    ACTOR actor_class;
    s16 timer;
} SPINNER_ACTOR;

static int32_t sGreeting = -1;

static void Spinner_ct(ACTOR* actor, GAME* game) {
    actor->world.position.y += 20.0f;
}

static void Spinner_mv(ACTOR* actor, GAME* game) {
    SPINNER_ACTOR* spinner = (SPINNER_ACTOR*)actor;

    spinner->timer++;
    if (mDemo_IS_ACTOR_TALKING(actor)) {
        return;
    }

    actor->shape_info.rotation.y += 0x300;
    if (actor->player_distance_xz < 80.0f && chkTrigger(BUTTON_A)) {
        C_ActorSpeak(actor, sGreeting);
    }
}

static void Spinner_dw(ACTOR* actor, GAME* game) {
    GRAPH* graph = game->graph;

    _texture_z_light_fog_prim_npc(graph);

    OPEN_DISP(graph);
    gSPMatrix(NEXT_POLY_OPA_DISP, _Matrix_to_Mtx_new(graph), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPDisplayList(NEXT_POLY_OPA_DISP, (Gfx*)sSpinnerModelT);
    gSPDisplayList(NEXT_POLY_OPA_DISP, (Gfx*)sSpinnerModel);
    CLOSE_DISP(graph);
}

static ACTOR_PROFILE sSpinnerProfile = {
    0,
    ACTOR_PART_ITEM,
    ACTOR_STATE_TA_SET,
    EMPTY_NO,
    ACTOR_OBJ_BANK_KEEP,
    sizeof(SPINNER_ACTOR),
    &Spinner_ct,
    mActor_NONE_PROC1,
    &Spinner_mv,
    &Spinner_dw,
    NULL,
};

static C_FurnitureDef sModTableDef = {
    "example.mod_table",
    "mod table",
    mIN_ARTICLE_A,
    4000,
    aFTR_SET_TYPE_SURFACE,
    mHsRm_HUUSUI_GREEN,
    FALSE,
    FALSE,
    {
        (Gfx*)sTableModel,
        (Gfx*)sTableModelT,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        20.0f,
        0.01f,
        aFTR_SHAPE_TYPEA,
        mCoBG_FTR_TYPEA,
        0,
        0,
        0,
        0,
        NULL,
    },
};

static void Gem_use(mActor_name_t item, int slot) {
    Private_c* priv = Common_Get(now_private);

    if (priv == NULL) {
        return;
    }
    priv->inventory.wallet += 1000;
    if (priv->inventory.wallet > mPr_WALLET_MAX) {
        priv->inventory.wallet = mPr_WALLET_MAX;
    }
    mPr_SetPossessionItem(priv, slot, EMPTY_NO, mPr_ITEM_COND_NORMAL);
}

static C_ItemDef sGemDef = {
    "example.lucky_gem", "lucky gem", mIN_ARTICLE_A, 2500, mNT_ITEM_TYPE_SHELLA, sGemIcon, sGemPalette, &Gem_use,
};

static const u8 sColorDemoCode[] = {
    0x78, 0xD8, 0xA2, 0x40, 0x8E, 0x17, 0x40, 0xA2, 0xFF, 0x9A, 0xE8, 0x8E, 0x00, 0x20, 0x8E, 0x01, 0x20, 0x8E, 0x10,
    0x40, 0x2C, 0x02, 0x20, 0x10, 0xFB, 0x2C, 0x02, 0x20, 0x10, 0xFB, 0xA9, 0x21, 0x85, 0x00, 0xA9, 0x0A, 0x8D, 0x01,
    0x20, 0x2C, 0x02, 0x20, 0x10, 0xFB, 0xA9, 0x3F, 0x8D, 0x06, 0x20, 0xA9, 0x00, 0x8D, 0x06, 0x20, 0xA5, 0x00, 0x8D,
    0x07, 0x20, 0xA9, 0x00, 0x8D, 0x06, 0x20, 0x8D, 0x06, 0x20, 0x8D, 0x05, 0x20, 0x8D, 0x05, 0x20, 0xA9, 0x01, 0x8D,
    0x16, 0x40, 0xA9, 0x00, 0x8D, 0x16, 0x40, 0xAD, 0x16, 0x40, 0x29, 0x01, 0xA8, 0x45, 0x01, 0x84, 0x01, 0x25, 0x01,
    0xF0, 0xC6, 0xA5, 0x00, 0x18, 0x69, 0x01, 0x29, 0x3F, 0x85, 0x00, 0x4C, 0x27, 0xC0, 0x40,
};

static u8 sColorDemoRom[16 + 0x4000 + 0x2000];
static int sColorDemo = -1;

static void BuildColorDemo(void) {
    static const u8 header[] = { 'N', 'E', 'S', 0x1A, 1, 1, 0x02, 0 };
    u8* prg = sColorDemoRom + 16;

    memcpy(sColorDemoRom, header, sizeof(header));
    memcpy(prg, sColorDemoCode, sizeof(sColorDemoCode));
    prg[0x3FFA] = 0x6D;
    prg[0x3FFB] = 0xC0;
    prg[0x3FFC] = 0x00;
    prg[0x3FFD] = 0xC0;
    prg[0x3FFE] = 0x6D;
    prg[0x3FFF] = 0xC0;
}

static void ModNes_mv(FTR_ACTOR* ftr_actor, ACTOR* my_room, GAME* game, u8* data) {
    C_FamicomFurnitureMove(ftr_actor, my_room, game, sColorDemo);
}

static void ModNes_dw(FTR_ACTOR* ftr_actor, ACTOR* my_room, GAME* game, u8* data) {
    OPEN_DISP(game->graph);
    gSPMatrix(NEXT_POLY_OPA_DISP, _Matrix_to_Mtx_new(game->graph), G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gSPSegment(NEXT_POLY_OPA_DISP, G_MWO_SEGMENT_8, sNesTexture);
    gSPSegment(NEXT_POLY_OPA_DISP, G_MWO_SEGMENT_9, sNesPalette);
    gSPDisplayList(NEXT_POLY_OPA_DISP, (Gfx*)sNesModel);
    gSPDisplayList(NEXT_POLY_OPA_DISP, (Gfx*)sNesModelT);
    gSPDisplayList(NEXT_POLY_OPA_DISP, (Gfx*)sNesGameModel);
    CLOSE_DISP(game->graph);
}

static aFTR_vtable_c sModNesFuncs = { NULL, &ModNes_mv, &ModNes_dw, NULL, NULL };

static C_FurnitureDef sModNesDef = {
    "example.mod_nes",
    "mod NES",
    mIN_ARTICLE_A,
    5000,
    aFTR_SET_TYPE_ON_SURFACE,
    mHsRm_HUUSUI_NONE,
    FALSE,
    FALSE,
    { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 18.0f, 0.01f, aFTR_SHAPE_TYPEA, mCoBG_FTR_TYPEA, 0, 0, 0,
      aFTR_INTERACTION_FAMICOM, &sModNesFuncs },
};

static s16 sSpinner = -1;
static mActor_name_t sModTable = EMPTY_NO;
static mActor_name_t sGem = EMPTY_NO;
static mActor_name_t sModNes = EMPTY_NO;
static ListenerID sPlayUpdateListener;
static int sSpawnRequested = FALSE;

static void OnPlayUpdate(IEvent* event) {
    PlayUpdate* update = (PlayUpdate*)event;
    ACTOR* player;

    if (!sSpawnRequested) {
        return;
    }
    sSpawnRequested = FALSE;

    player = GET_PLAYER_ACTOR_ACTOR(update->play);
    if (player != NULL) {
        C_SpawnActor(sSpinner, player->world.position.x, player->world.position.y, player->world.position.z + 60.0f, 0,
                     0);
    }
}

static void OnSpawnPressed(void) {
    sSpawnRequested = TRUE;
}

static void OnGivePressed(void) {
    if (sModTable != EMPTY_NO && Common_Get(now_private) != NULL) {
        mPr_SetFreePossessionItem(Common_Get(now_private), sModTable, mPr_ITEM_COND_NORMAL);
    }
}

static void OnGiveGemPressed(void) {
    if (sGem != EMPTY_NO && Common_Get(now_private) != NULL) {
        mPr_SetFreePossessionItem(Common_Get(now_private), sGem, mPr_ITEM_COND_NORMAL);
    }
}

static void OnGiveNesPressed(void) {
    if (sModNes != EMPTY_NO && Common_Get(now_private) != NULL) {
        mPr_SetFreePossessionItem(Common_Get(now_private), sModNes, mPr_ITEM_COND_NORMAL);
    }
}

static void SetupUI(void) {
    C_WidgetConfig spawn = { 0 };
    C_WidgetConfig give = { 0 };
    C_WidgetConfig gem = { 0 };
    C_WidgetConfig nes = { 0 };

    spawn.type = C_WIDGET_BUTTON;
    spawn.callback = OnSpawnPressed;
    spawn.opts.generic.tooltip = "Spawns the spinner in front of the player.";

    give.type = C_WIDGET_BUTTON;
    give.callback = OnGivePressed;
    give.opts.generic.tooltip = "Puts a mod table in your pockets.";

    gem.type = C_WIDGET_BUTTON;
    gem.callback = OnGiveGemPressed;
    gem.opts.generic.tooltip = "Puts a lucky gem in your pockets. Use it to break it into 1,000 Bells.";

    nes.type = C_WIDGET_BUTTON;
    nes.callback = OnGiveNesPressed;
    nes.opts.generic.tooltip = "Puts an NES that plays a small demo in your pockets.";

    C_AddSidebarEntry("Custom Content", 1);
    C_AddWidget("Custom Content", 1, "Spawn Spinner", &spawn);
    C_AddWidget("Custom Content", 1, "Give Mod Table", &give);
    C_AddWidget("Custom Content", 1, "Give Lucky Gem", &gem);
    C_AddWidget("Custom Content", 1, "Give Mod NES", &nes);
}

MOD_INIT() {
    sGreeting = C_RegisterMessage("Hi, <<STR_PLAYERNAME>>!\nI'm a podium from a mod.<<BTN>>\n"
                                  "<<MSGCLEAR>>Come back and talk to me\nany time!");
    sSpinner = C_RegisterActor(&sSpinnerProfile);
    sModTable = C_RegisterFurniture(&sModTableDef);
    sGem = C_RegisterItem(&sGemDef);
    BuildColorDemo();
    sColorDemo = C_RegisterFamicomGame("example.color_demo", sColorDemoRom, sizeof(sColorDemoRom));
    sModNes = C_RegisterFurniture(&sModNesDef);
    sPlayUpdateListener = REGISTER_LISTENER(PlayUpdate, EVENT_PRIORITY_NORMAL, OnPlayUpdate);
    SetupUI();
}

MOD_EXIT() {
    C_RemoveSidebarEntry("Custom Content");
    UNREGISTER_LISTENER(PlayUpdate, sPlayUpdateListener);
    C_UnregisterFurniture(sModNes);
    C_UnregisterFamicomGame(sColorDemo);
    C_UnregisterItem(sGem);
    C_UnregisterFurniture(sModTable);
    C_UnregisterActor(sSpinner);
    C_UnregisterMessage(sGreeting);
}
