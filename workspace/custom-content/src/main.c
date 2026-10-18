#include "mod.h"

#include "port/api/actor.h"
#include "port/api/cloth.h"
#include "port/api/creature.h"
#include "port/api/famicom.h"
#include "port/api/furniture.h"
#include "port/api/interior.h"
#include "port/api/item.h"
#include "port/api/message.h"
#include "port/api/shop.h"
#include "port/api/villager.h"
#include "port/api/ui.h"
#include "port/events/Events.h"
#include "ac_set_ovl_gyoei.h"
#include "ac_set_ovl_insect.h"
#include "graph.h"
#include "m_common_data.h"
#include "m_controller.h"
#include "m_demo.h"
#include "m_huusui_room.h"
#include "m_item_name.h"
#include "m_npc.h"
#include "ac_npc_anim_def.h"
#include "m_player_lib.h"
#include "m_private.h"
#include "m_rcp.h"
#include "sys_matrix.h"
#include <string.h>

C_ASSET(sSpinnerModel, "data/model/obj_e_ghog/obj_e_ghog_model");
C_ASSET(sSpinnerModelT, "data/model/obj_e_ghog/obj_e_ghog_modelT");
C_ASSET(sTableModel, "data/model/int_ari_table01/int_ari_table01_01_model");
C_ASSET(sTableModelT, "data/model/int_ari_table01/int_ari_table01_01T_model");
C_ASSET(sNesModel, "data/model/int_tak_nes01/int_tak_nes01_on_model");
C_ASSET(sNesModelT, "data/model/int_tak_nes01/int_tak_nes01_onT_model");
C_ASSET(sNesGameModel, "data/model/int_tak_nes01/int_tak_nes01_game_on_model");
C_ASSET(sNesTexture, "data/model/int_tak_nes01/int_tak_nes_ds_tex_rgb_i4");
C_ASSET(sNesPalette, "data/model/int_tak_nes01/int_tak_nes_cl_pal");

static const u8 sGemIcon[] = { #asset "textures/lucky_gem.png" CI4 };
static const u8 sGemPalette[] = { #asset "textures/lucky_gem.png" CI4_PAL };
static const u8 sCarpetTexture[] = { #asset "textures/mod_carpet.png" CI4 };
static const u8 sCarpetPalette[] = { #asset "textures/mod_carpet.png" CI4_PAL };
static const u8 sWallpaperTexture[] = { #asset "textures/mod_wallpaper.png" CI4 };
static const u8 sWallpaperPalette[] = { #asset "textures/mod_wallpaper.png" CI4_PAL };
static const u8 sKoiIcon[] = { #asset "textures/rainbow_koi.png" CI4 };
static const u8 sKoiPalette[] = { #asset "textures/rainbow_koi.png" CI4_PAL };
static const u8 sDragonflyIcon[] = { #asset "textures/mod_dragonfly.png" CI4 };
static const u8 sDragonflyPalette[] = { #asset "textures/mod_dragonfly.png" CI4_PAL };
static const u8 sTeeTexture[] = { #asset "textures/mod_tee.png" CI4 };
static const u8 sTeePalette[] = { #asset "textures/mod_tee.png" CI4_PAL };

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
    TRUE,
};

static C_InteriorDef sCarpetDef = {
    "example.mod_carpet", "mod carpet", mIN_ARTICLE_A, 1800, sCarpetTexture, sCarpetPalette,
};

static C_InteriorDef sWallpaperDef = {
    "example.mod_wallpaper", "mod wallpaper", mIN_ARTICLE_A, 1500, sWallpaperTexture, sWallpaperPalette,
};

static C_CreatureDef sKoiDef = {
    "example.rainbow_koi",
    "rainbow koi",
    mIN_ARTICLE_A,
    3000,
    aGYO_TYPE_KOI,
    aSOG_SPAWN_AREA_RIVER,
    10,
    0,
    0,
    FALSE,
    "I caught a rainbow koi!\nIt shimmers in every colour!",
    sKoiIcon,
    sKoiPalette,
    NULL,
};

static C_CreatureDef sDragonflyDef = {
    "example.mod_dragonfly",
    "mod dragonfly",
    mIN_ARTICLE_A,
    800,
    aINS_INSECT_TYPE_DARNER_DRAGONFLY,
    aSOI_SPAWN_AREA_FLYING,
    10,
    0x3F0,
    0x7FF00,
    FALSE,
    NULL,
    sDragonflyIcon,
    sDragonflyPalette,
    NULL,
};

C_ASSET(sDogSkeleton, "data/npc/model/mdl/dog_1/cKF_bs_r_dog_1");
C_ASSET(sDogTexture, "data/npc/model/tex/dog_1/dog_1_tmem_txt");
C_ASSET(sDogEye1, "data/npc/model/tex/dog_1/dog_1_eye1_TA_tex_txt");
C_ASSET(sDogEye2, "data/npc/model/tex/dog_1/dog_1_eye2_TA_tex_txt");
C_ASSET(sDogEye3, "data/npc/model/tex/dog_1/dog_1_eye3_TA_tex_txt");
C_ASSET(sDogEye4, "data/npc/model/tex/dog_1/dog_1_eye4_TA_tex_txt");
C_ASSET(sDogEye5, "data/npc/model/tex/dog_1/dog_1_eye5_TA_tex_txt");
C_ASSET(sDogEye6, "data/npc/model/tex/dog_1/dog_1_eye6_TA_tex_txt");
C_ASSET(sDogEye7, "data/npc/model/tex/dog_1/dog_1_eye7_TA_tex_txt");
C_ASSET(sDogEye8, "data/npc/model/tex/dog_1/dog_1_eye8_TA_tex_txt");
C_ASSET(sDogMouth1, "data/npc/model/tex/dog_1/dog_1_mouth1_TA_tex_txt");
C_ASSET(sDogMouth2, "data/npc/model/tex/dog_1/dog_1_mouth2_TA_tex_txt");
C_ASSET(sDogMouth3, "data/npc/model/tex/dog_1/dog_1_mouth3_TA_tex_txt");
C_ASSET(sDogMouth4, "data/npc/model/tex/dog_1/dog_1_mouth4_TA_tex_txt");
C_ASSET(sDogMouth5, "data/npc/model/tex/dog_1/dog_1_mouth5_TA_tex_txt");
C_ASSET(sDogMouth6, "data/npc/model/tex/dog_1/dog_1_mouth6_TA_tex_txt");
C_ASSET(sRunAnimation, "data/npc/npc_1_run1/cKF_ba_r_npc_1_run1");

static const u8 sBoltModel[] = { #asset "models/bolt.gltf" NPC_MODEL };
static const u8 sBoltWalk[] = { #asset "models/bolt.gltf" NPC_ANIM };

static const void* sDogEyes[C_VILLAGER_EYES] = {
    sDogEye1, sDogEye2, sDogEye3, sDogEye4, sDogEye5, sDogEye6, sDogEye7, sDogEye8,
};
static const void* sDogMouths[C_VILLAGER_MOUTHS] = {
    sDogMouth1, sDogMouth2, sDogMouth3, sDogMouth4, sDogMouth5, sDogMouth6,
};
static const void* sMintAnimations[aNPC_ANIM_NUM];
static mActor_name_t sMintRoom[C_VILLAGER_ROOM_SIZE];
static mActor_name_t sMintRoomTop[C_VILLAGER_ROOM_SIZE];

static const u8 sMintPalette[] = {
    0x66, 0xA9, 0x7E, 0xED, 0xB7, 0xB5, 0xD2, 0x11, 0xFB, 0x9D, 0x90, 0x03, 0x1B, 0x13, 0x34, 0x9B,
    0xFF, 0xFF, 0xBD, 0x69, 0x59, 0xCD, 0x41, 0x09, 0x82, 0x89, 0xBB, 0x45, 0x62, 0x93, 0x20, 0x81,
};

static C_VillagerDef sMintDef;
static C_VillagerDef sBoltDef;
static const void* sBoltAnimations[aNPC_ANIM_NUM];

static C_ClothDef sTeeDef = {
    "example.mod_tee", "mod tee", mIN_ARTICLE_A, 1200, sTeeTexture, sTeePalette,
};

static s16 sSpinner = -1;
static mActor_name_t sModTable = EMPTY_NO;
static mActor_name_t sGem = EMPTY_NO;
static mActor_name_t sModNes = EMPTY_NO;
static mActor_name_t sCarpet = EMPTY_NO;
static mActor_name_t sWallpaper = EMPTY_NO;
static mActor_name_t sKoi = EMPTY_NO;
static mActor_name_t sDragonfly = EMPTY_NO;
static mActor_name_t sTee = EMPTY_NO;
static mActor_name_t sMint = EMPTY_NO;
static mActor_name_t sBolt = EMPTY_NO;
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

static void OnGiveInteriorPressed(void) {
    if (Common_Get(now_private) == NULL) {
        return;
    }
    if (sCarpet != EMPTY_NO) {
        mPr_SetFreePossessionItem(Common_Get(now_private), sCarpet, mPr_ITEM_COND_NORMAL);
    }
    if (sWallpaper != EMPTY_NO) {
        mPr_SetFreePossessionItem(Common_Get(now_private), sWallpaper, mPr_ITEM_COND_NORMAL);
    }
}

static void OnGiveCreaturesPressed(void) {
    if (Common_Get(now_private) == NULL) {
        return;
    }
    if (sKoi != EMPTY_NO) {
        mPr_SetFreePossessionItem(Common_Get(now_private), sKoi, mPr_ITEM_COND_NORMAL);
    }
    if (sDragonfly != EMPTY_NO) {
        mPr_SetFreePossessionItem(Common_Get(now_private), sDragonfly, mPr_ITEM_COND_NORMAL);
    }
}

static void OnGiveTeePressed(void) {
    if (sTee != EMPTY_NO && Common_Get(now_private) != NULL) {
        mPr_SetFreePossessionItem(Common_Get(now_private), sTee, mPr_ITEM_COND_NORMAL);
    }
}

static void SetupMintRoom(mActor_name_t table) {
    int x;
    int z;

    for (z = 0; z < 16; z++) {
        for (x = 0; x < 16; x++) {
            int inside = x < 8 && z < 8;
            int wall = inside && (x == 0 || x == 7 || z == 0 || z == 7);

            sMintRoom[z * 16 + x] = wall ? RSV_WALL_NO : RSV_NO;
            sMintRoomTop[z * 16 + x] = RSV_NO;
        }
    }
    sMintRoom[7 * 16 + 3] = RSV_NO;
    sMintRoom[7 * 16 + 4] = RSV_NO;
    sMintRoom[8 * 16 + 3] = EXIT_DOOR;
    sMintRoom[8 * 16 + 4] = EXIT_DOOR;

    sMintRoom[1 * 16 + 1] = table;
    sMintRoom[1 * 16 + 3] = FTR_START(FTR_NOG_TRI_CLOCK01);
    sMintRoom[1 * 16 + 4] = FTR_START(FTR_NOG_TRI_AUDIO01);
    sMintRoom[1 * 16 + 6] = table;
    sMintRoom[3 * 16 + 1] = FTR_START(FTR_NOG_TRI_BED01) + 3;
    sMintRoom[4 * 16 + 6] = FTR_START(FTR_NOG_TRI_SOFA01) + 3;
    sMintRoom[6 * 16 + 1] = FTR_START(FTR_NOG_TRI_CHAIR01);
    sMintRoom[6 * 16 + 6] = FTR_START(FTR_KOB_NCUBE);
    sMintRoomTop[1 * 16 + 4] = ITM_MINIDISK28;
    sMintRoomTop[1 * 16 + 6] = ITM_MINIDISK28;
}

static void SetupMint(void) {
    SetupMintRoom(sModTable);
    sMintAnimations[aNPC_ANIM_WALK1] = sRunAnimation;

    sMintDef.id = "example.mint";
    sMintDef.name = "Mint";
    sMintDef.catchphrase = "minty";
    sMintDef.base = NPC_BOB;
    sMintDef.looks = mNpc_LOOKS_KO_GIRL;
    sMintDef.cloth = sTee;
    sMintDef.texture = sDogTexture;
    sMintDef.palette = sMintPalette;
    sMintDef.house_type = 2;
    sMintDef.house_palette = 3;
    sMintDef.wallpaper = sWallpaper;
    sMintDef.carpet = sCarpet;
    sMintDef.room = sMintRoom;
    sMintDef.room_top = sMintRoomTop;
    sMintDef.skeleton = sDogSkeleton;
    sMintDef.eyes = sDogEyes;
    sMintDef.mouths = sDogMouths;
    sMintDef.eye_height = 37;
    sMintDef.col_radius = 20;
    sMintDef.col_height = 45;
    sMintDef.talk_type = -1;
    sMintDef.animations = sMintAnimations;
}

static void SetupBolt(void) {
    sBoltAnimations[aNPC_ANIM_WALK1] = sBoltWalk;

    sBoltDef.id = "example.bolt";
    sBoltDef.name = "Bolt";
    sBoltDef.catchphrase = "beep";
    sBoltDef.base = NPC_BOB;
    sBoltDef.looks = mNpc_LOOKS_BOY;
    sBoltDef.cloth = EMPTY_NO;
    sBoltDef.house_type = 4;
    sBoltDef.house_palette = 0;
    sBoltDef.wallpaper = EMPTY_NO;
    sBoltDef.carpet = EMPTY_NO;
    sBoltDef.skeleton = sBoltModel;
    sBoltDef.eye_height = 45;
    sBoltDef.col_radius = 24;
    sBoltDef.col_height = 50;
    sBoltDef.talk_type = 0;
    sBoltDef.animations = sBoltAnimations;
}

static void InviteVillager(mActor_name_t npc_id) {
    int i;

    if (npc_id == EMPTY_NO || Common_Get(now_private) == NULL) {
        return;
    }
    for (i = 0; i < ANIMAL_NUM_MAX; i++) {
        mActor_name_t resident = Save_Get(animals[i]).id.npc_id;

        if (ITEM_NAME_GET_TYPE(resident) == NAME_TYPE_NPC && !C_IsCustomVillager(resident) &&
            C_ReplaceVillager(resident, npc_id)) {
            break;
        }
    }
}

static void OnInviteBoltPressed(void) {
    InviteVillager(sBolt);
}

static void OnInviteMintPressed(void) {
    InviteVillager(sMint);
}

static void OnRestockPressed(void) {
    C_RestockShop();
}

static void SetupUI(void) {
    C_WidgetConfig spawn = { 0 };
    C_WidgetConfig give = { 0 };
    C_WidgetConfig gem = { 0 };
    C_WidgetConfig nes = { 0 };
    C_WidgetConfig interior = { 0 };
    C_WidgetConfig creatures = { 0 };
    C_WidgetConfig tee = { 0 };
    C_WidgetConfig mint = { 0 };
    C_WidgetConfig bolt = { 0 };
    C_WidgetConfig restock = { 0 };

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

    interior.type = C_WIDGET_BUTTON;
    interior.callback = OnGiveInteriorPressed;
    interior.opts.generic.tooltip = "Puts the mod carpet and mod wallpaper in your pockets. Use them in your house.";

    creatures.type = C_WIDGET_BUTTON;
    creatures.callback = OnGiveCreaturesPressed;
    creatures.opts.generic.tooltip = "Puts a rainbow koi and a mod dragonfly in your pockets. Both can also be caught.";

    tee.type = C_WIDGET_BUTTON;
    tee.callback = OnGiveTeePressed;
    tee.opts.generic.tooltip = "Puts a mod tee in your pockets. Drag it onto your character to wear it.";

    mint.type = C_WIDGET_BUTTON;
    mint.callback = OnInviteMintPressed;
    mint.opts.generic.tooltip = "Mint moves into the first villager's house. Leave the town and come back to see it.";

    bolt.type = C_WIDGET_BUTTON;
    bolt.callback = OnInviteBoltPressed;
    bolt.opts.generic.tooltip = "Bolt, a robot with his own model and walk, moves into a built-in villager's house.";

    restock.type = C_WIDGET_BUTTON;
    restock.callback = OnRestockPressed;
    restock.opts.generic.tooltip =
        "Rolls Nook's stock again. The mod table, carpet, wallpaper and tee are each on sale half of the days.";

    C_AddSidebarEntry("Custom Content", 1);
    C_AddWidget("Custom Content", 1, "Spawn Spinner", &spawn);
    C_AddWidget("Custom Content", 1, "Give Mod Table", &give);
    C_AddWidget("Custom Content", 1, "Give Lucky Gem", &gem);
    C_AddWidget("Custom Content", 1, "Give Mod NES", &nes);
    C_AddWidget("Custom Content", 1, "Give Mod Carpet and Wallpaper", &interior);
    C_AddWidget("Custom Content", 1, "Give Mod Fish and Insect", &creatures);
    C_AddWidget("Custom Content", 1, "Give Mod Tee", &tee);
    C_AddWidget("Custom Content", 1, "Invite Mint", &mint);
    C_AddWidget("Custom Content", 1, "Invite Bolt", &bolt);
    C_AddWidget("Custom Content", 1, "Restock Shop", &restock);
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
    sCarpet = C_RegisterCarpet(&sCarpetDef);
    sWallpaper = C_RegisterWallpaper(&sWallpaperDef);
    sKoi = C_RegisterFish(&sKoiDef);
    sDragonfly = C_RegisterInsect(&sDragonflyDef);
    sTee = C_RegisterCloth(&sTeeDef);
    SetupMint();
    sMint = C_RegisterVillager(&sMintDef);
    SetupBolt();
    sBolt = C_RegisterVillager(&sBoltDef);
    C_AddShopStock(sModTable, 50);
    C_AddShopStock(sCarpet, 50);
    C_AddShopStock(sWallpaper, 50);
    C_AddShopStock(sTee, 50);
    sPlayUpdateListener = REGISTER_LISTENER(PlayUpdate, EVENT_PRIORITY_NORMAL, OnPlayUpdate);
    SetupUI();
}

MOD_EXIT() {
    C_RemoveSidebarEntry("Custom Content");
    UNREGISTER_LISTENER(PlayUpdate, sPlayUpdateListener);
    C_RemoveShopStock(sModTable);
    C_RemoveShopStock(sCarpet);
    C_RemoveShopStock(sWallpaper);
    C_RemoveShopStock(sTee);
    C_UnregisterVillager(sBolt);
    C_UnregisterVillager(sMint);
    C_UnregisterCloth(sTee);
    C_UnregisterWallpaper(sWallpaper);
    C_UnregisterInsect(sDragonfly);
    C_UnregisterFish(sKoi);
    C_UnregisterCarpet(sCarpet);
    C_UnregisterFurniture(sModNes);
    C_UnregisterFamicomGame(sColorDemo);
    C_UnregisterItem(sGem);
    C_UnregisterFurniture(sModTable);
    C_UnregisterActor(sSpinner);
    C_UnregisterMessage(sGreeting);
}
