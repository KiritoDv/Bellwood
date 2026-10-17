#include "port/famicom/famicomPriv.h"
#include "port/famicom/NesCore.h"

#include "ac_asset.h"
#include "game.h"
#include "graph.h"
#include "jaudio_NES/emusound.h"
#include "m_personal_id.h"

#include <string.h>

#define FAMICOM_FRAME_WIDTH 640
#define FAMICOM_FRAME_HEIGHT 480
#define FAMICOM_STICK_THRESHOLD 32
#define FAMICOM_DISK_SPEEDUP 4

#define FAMICOM_SCREEN_TOP 8
#define FAMICOM_SCREEN_LINES 228
#define FAMICOM_SCALE 2
#define FAMICOM_STRIP_LINES 8
#define FAMICOM_STRIPS ((FAMICOM_SCREEN_LINES + FAMICOM_STRIP_LINES - 1) / FAMICOM_STRIP_LINES)
#define FAMICOM_DRAW_X ((FAMICOM_FRAME_WIDTH - NES_SCREEN_WIDTH * FAMICOM_SCALE) / 2)
#define FAMICOM_DRAW_Y ((FAMICOM_FRAME_HEIGHT - FAMICOM_SCREEN_LINES * FAMICOM_SCALE) / 2)
#define FAMICOM_TEXTURE_SIZE (NES_SCREEN_WIDTH * FAMICOM_SCREEN_LINES * sizeof(u16))

#define FAMICOM_NOISE_PATH "famicom/noise"

static const char* const l_rom_paths[FAMICOM_INTERNAL_ROM_NUM] = {
    "famicom/rom/01_nes_cluclu3.bin",  "famicom/rom/02_usa_balloon.nes",  "famicom/rom/03_nes_donkey1_3.bin",
    "famicom/rom/04_usa_jr_math.nes",  "famicom/rom/05_pinball_1.nes",    "famicom/rom/06_nes_tennis3.bin",
    "famicom/rom/07_usa_golf.nes",     "famicom/rom/08_punch_wh.nes",     "famicom/rom/09_usa_baseball_1.nes",
    "famicom/rom/10_cluclu_1.qd",      "famicom/rom/11_usa_donkey3.nes",  "famicom/rom/12_donkeyjr_1.nes",
    "famicom/rom/13_soccer.nes",       "famicom/rom/14_exbike.nes",       "famicom/rom/15_usa_wario.nes",
    "famicom/rom/16_usa_icecl.nes",    "famicom/rom/17_nes_mario1_2.bin", "famicom/rom/18_smario_0.nes",
    "famicom/rom/19_usa_zelda1_1.nes",
};

static const u8 l_nes_palette[64 * 2] = {
    0xc2, 0x10, 0x80, 0x17, 0x98, 0x17, 0xc0, 0x14, 0xdc, 0x0d, 0xd8, 0x03, 0xd8, 0x00, 0xc8, 0x80, 0xbc, 0xa0, 0x80,
    0xe0, 0x81, 0x21, 0x80, 0xe4, 0x80, 0xac, 0x80, 0x00, 0x80, 0x00, 0x80, 0x00, 0xe7, 0x39, 0x81, 0x7f, 0xa0, 0xff,
    0xd8, 0xd9, 0xfc, 0xd5, 0xfc, 0xcb, 0xfc, 0xc3, 0xe9, 0x20, 0xe1, 0x80, 0x9d, 0xe0, 0x8e, 0x02, 0x82, 0x4c, 0x82,
    0x18, 0x88, 0x42, 0x80, 0x00, 0x80, 0x00, 0xff, 0xff, 0x82, 0x5f, 0xb6, 0x1f, 0xe9, 0xbf, 0xfd, 0xd9, 0xfd, 0xb3,
    0xfd, 0xeb, 0xfe, 0x4b, 0xfe, 0x86, 0xd2, 0xe0, 0xab, 0x6d, 0xa7, 0x55, 0x83, 0x7f, 0xb1, 0x8c, 0x80, 0x00, 0x80,
    0x00, 0xff, 0xff, 0xc2, 0xff, 0xde, 0xff, 0xea, 0xff, 0xfe, 0xfd, 0xfe, 0xf9, 0xff, 0x16, 0xff, 0x35, 0xff, 0x74,
    0xe7, 0x93, 0xd7, 0xb6, 0xd7, 0xdd, 0xdb, 0xbf, 0xef, 0x7b, 0x80, 0x00, 0x80, 0x00,
};

static const u8 l_save_data_name[FAMICOM_SAVE_DATA_NAME_LEN] = { 'G', 'A', 'F', 'E', 'F', 'S', 'C', 0x1A };
static const u8 l_card_save_data_name[FAMICOM_SAVE_DATA_NAME_LEN] = { 'G', 'A', 'F', 'E', 'F', 'S', 'E', 0x1A };

FamicomCommon famicomCommon;
u8 famicomCommonSave[FAMICOM_SAVE_TOTAL_SIZE];
Famicom_MallocInfo* my_malloc_current = NULL;
u8 save_game_image = FALSE;

static NesCore* l_core = NULL;
static FamicomNesState l_nes_state;
static u8* l_noise = NULL;
static int l_rom_loaded = FALSE;
static FAMICOM_GETSAVECHAN_PROC l_getSaveChan_proc = NULL;

static u8 l_texture[2][FAMICOM_TEXTURE_SIZE] ATTRIBUTE_ALIGN(8);
static Gfx l_display_list[2][16 + FAMICOM_STRIPS * 16];
static int l_draw_buffer = 0;

static void* my_malloc(size_t size, int align) {
    return (*my_malloc_current->malloc_align)(size, align);
}

static void my_free(void* ptr) {
    if (ptr != NULL) {
        (*my_malloc_current->free)(ptr);
    }
}

static u8 calcSum(const u8* data, size_t len) {
    u8 checksum = 0;
    size_t i;

    for (i = 0; i != len; i++) {
        checksum += data[i];
    }

    return checksum;
}

static u16 save_header_size_get(const FamicomSaveDataHeader* header) {
    const u8* p = (const u8*)&header->size;
    return (u16)((p[0] << 8) | p[1]);
}

static void save_header_size_set(FamicomSaveDataHeader* header, u16 size) {
    u8* p = (u8*)&header->size;
    p[0] = (u8)(size >> 8);
    p[1] = (u8)size;
}

static void famicom_save_data_setup(FamicomSaveDataHeader* header, u32 size, const u8* identifier) {
    u8* data;
    u32 i;

    if (memcmp(header->name, identifier, FAMICOM_SAVE_DATA_NAME_LEN) != 0 ||
        calcSum((u8*)header, FAMICOM_SAVE_HEADER_SIZE) != 0 || header->_08 != 1 || header->_09 != 0 ||
        header->headerSize != FAMICOM_SAVE_HEADER_SIZE || save_header_size_get(header) != size) {
        header->_08 = 1;
        header->_09 = 0;
        header->headerSize = FAMICOM_SAVE_HEADER_SIZE;
        save_header_size_set(header, (u16)size);
        memcpy(header->name, identifier, FAMICOM_SAVE_DATA_NAME_LEN);
        header->checksum -= calcSum((u8*)header, FAMICOM_SAVE_HEADER_SIZE);
    }

    data = (u8*)header + header->headerSize;
    for (i = 0; i < PLAYER_NUM; i++) {
        int version_tag = data[1] >> 4;
        int version_no = data[1] & 0xF;

        if (data[0] != 'D' || calcSum(data, size) != 0 || version_tag != 3 || version_no > 1 || data[3] != (u8)i) {
            data[0] = 'D';
            data[1] = '1';
            data[3] = (u8)i;
            data[2] -= calcSum(data, size);
        }

        data += size;
    }
}

static void famicom_save_data_init(FamicomSaveDataHeader* header, u32 size, const u8* identifier) {
    memset(header, 0, FAMICOM_SAVE_HEADER_SIZE + size * PLAYER_NUM);
    header->no_save = TRUE;
    famicom_save_data_setup(header, size, identifier);
}

static int famicom_save_data_check(const FamicomSaveDataHeader* header, int ignore_player_no, const u8* identifier) {
    const u8* data;
    u16 size;
    int i;

    if (memcmp(header->name, identifier, FAMICOM_SAVE_DATA_NAME_LEN) != 0 ||
        calcSum((const u8*)header, FAMICOM_SAVE_HEADER_SIZE) != 0) {
        return -1;
    }
    if (!((header->_08 == 1 && header->_09 == 0) || (header->_08 == 0 && header->_09 == 1)) ||
        header->headerSize != FAMICOM_SAVE_HEADER_SIZE) {
        return -1;
    }

    size = save_header_size_get(header);
    data = (const u8*)header + header->headerSize;
    for (i = 0; i < PLAYER_NUM; i++) {
        if (i != ignore_player_no && (data[0] != 'D' || calcSum(data, size) != 0)) {
            return -1;
        }
        data += size;
    }

    return 0;
}

static void famicom_save_common_setup(void) {
    famicomCommon.save_data_name = l_save_data_name;
    famicomCommon.save_data_single_size = FAMICOM_SAVE_SINGLE_SIZE;
    famicomCommon.save_data_total_size = FAMICOM_SAVE_TOTAL_SIZE;
    famicomCommon.save_data_header = (FamicomSaveDataHeader*)famicomCommonSave;
}

extern int famicom_getErrorChan(void) {
    return 0;
}

extern void famicom_setCallback_getSaveChan(FAMICOM_GETSAVECHAN_PROC proc) {
    l_getSaveChan_proc = proc;
}

extern int famicom_mount_archive_end_check(void) {
    return TRUE;
}

extern void famicom_mount_archive(void) {
}

static void famicom_free_buffers(void) {
    if (l_core != NULL) {
        NesCore_Destroy(l_core);
        l_core = NULL;
    }
    if (my_malloc_current != NULL) {
        my_free(famicomCommon.highscore_flagsp);
        my_free(famicomCommon.bbramp);
        my_free(famicomCommon.nesromp);
        my_free(famicomCommon.card_tagsp);
    }
    famicomCommon.card_tagsp = NULL;
    famicomCommon.highscore_flagsp = NULL;
    famicomCommon.bbramp = NULL;
    famicomCommon.nesromp = NULL;
    famicomCommon.sp = NULL;
}

extern int famicom_init(int rom_idx, Famicom_MallocInfo* malloc_info, int player_no) {
    size_t rom_size;

    memset(&famicomCommon, 0, sizeof(famicomCommon));
    my_malloc_current = malloc_info;
    l_rom_loaded = FALSE;
    l_noise = NULL;

    famicomCommon.save_pl_no = (player_no >= 0 && player_no < PLAYER_NUM) ? player_no : 0;

    if (rom_idx < 0) {
        FamicomCardGame game;

        if (!FamicomCard_Load(-rom_idx - 1, &game)) {
            return -1;
        }
        famicomCommon.nesrom_memcard = TRUE;
        famicomCommon.rom_no = (u8)-rom_idx;
        memcpy(famicomCommon.famicom_save_name, game.save_name, sizeof(famicomCommon.famicom_save_name));
        rom_size = game.rom_size;
        FamicomCard_Release();
    } else if (rom_idx == 0 || rom_idx > FAMICOM_INTERNAL_ROM_NUM) {
        return -1;
    } else {
        famicomCommon.rom_no = (u8)rom_idx;
        if (save_game_image == FALSE) {
            famicom_save_common_setup();
        }
        rom_size = ASSET_SIZE(l_rom_paths[rom_idx - 1]);
    }

    l_noise = (u8*)LOAD_ASSET_RAW(FAMICOM_NOISE_PATH);
    if (rom_size == 0 || l_noise == NULL || ASSET_SIZE(FAMICOM_NOISE_PATH) <= NES_FDS_BIOS_SIZE) {
        l_noise = NULL;
        return -1;
    }

    famicomCommon.nesrom_size = rom_size;
    famicomCommon.nesromp = (u8*)my_malloc(rom_size, 32);
    famicomCommon.bbramp = (u8*)my_malloc(NES_SRAM_SIZE, 32);
    if (famicomCommon.nesromp == NULL || famicomCommon.bbramp == NULL) {
        famicom_free_buffers();
        l_noise = NULL;
        return -1;
    }

    EmuSound_Start(l_noise + NES_FDS_BIOS_SIZE);
    return 0;
}

static int famicom_card_rom_load(void) {
    FamicomCardGame game;

    if (!FamicomCard_Load(famicomCommon.rom_no - 1, &game) || game.rom_size != famicomCommon.nesrom_size) {
        FamicomCard_Release();
        return -1;
    }

    nesinfo_init();
    memcpy(famicomCommon.nesromp, game.rom, game.rom_size);
    if (game.tags_size != 0) {
        famicomCommon.card_tagsp = (u8*)my_malloc(game.tags_size, 4);
        if (famicomCommon.card_tagsp == NULL) {
            FamicomCard_Release();
            return -1;
        }
        memcpy(famicomCommon.card_tagsp, game.tags, game.tags_size);
    }
    nesinfo_tags_start = famicomCommon.card_tagsp;
    nesinfo_tags_end = famicomCommon.card_tagsp + game.tags_size;
    nesinfo_tags_size = (u32)game.tags_size;
    FamicomCard_Release();
    return 0;
}

static void famicom_card_save_setup(u32 max_ofs) {
    FamicomSaveDataHeader* header;

    if (famicomCommon.save_pl_no < 0) {
        return;
    }

    famicomCommon.save_data_name = l_card_save_data_name;
    famicomCommon.save_data_single_size = max_ofs + 0xB;
    famicomCommon.save_data_total_size = FAMICOM_SAVE_HEADER_SIZE + famicomCommon.save_data_single_size * PLAYER_NUM;
    header = (FamicomSaveDataHeader*)FamicomCard_SaveBuffer(famicomCommon.save_data_total_size);
    memset(header, 0, famicomCommon.save_data_total_size);
    famicomCommon.save_data_header = header;

    if (FamicomCard_ReadSave(famicomCommon.famicom_save_name, header, famicomCommon.save_data_total_size) !=
            (int)famicomCommon.save_data_total_size ||
        famicom_save_data_check(header, -1, famicomCommon.save_data_name) != 0) {
        famicom_save_data_init(header, famicomCommon.save_data_single_size, famicomCommon.save_data_name);
    }

    famicomCommon.internal_save_datap =
        (u8*)header + FAMICOM_SAVE_HEADER_SIZE + famicomCommon.save_data_single_size * famicomCommon.save_pl_no;
}

static int famicom_rom_load(void) {
    u32 max_ofs = 0;
    int highscore_num;

    if (famicomCommon.nesrom_memcard) {
        if (famicom_card_rom_load() != 0) {
            return -1;
        }
    } else {
        const char* path = l_rom_paths[famicomCommon.rom_no - 1];
        void* rom = LOAD_ASSET_RAW(path);

        if (rom == NULL || ASSET_SIZE(path) != famicomCommon.nesrom_size) {
            return -1;
        }

        nesinfo_init();
        memcpy(famicomCommon.nesromp, rom, famicomCommon.nesrom_size);
        nesinfo_tags_set(famicomCommon.rom_no - 1);
    }
    nesinfo_data_size = (u32)famicomCommon.nesrom_size;
    nesinfo_data_start = famicomCommon.nesromp;
    nesinfo_data_end = famicomCommon.nesromp + famicomCommon.nesrom_size;

    tcs_bad = FALSE;
    ics_bad = FALSE;
    nesinfo_tag_process1(NULL, 0, &max_ofs);
    if (tcs_bad || ics_bad) {
        return -1;
    }

    if (famicomCommon.nesrom_memcard) {
        famicom_card_save_setup(max_ofs);
    } else if (famicomCommon.save_data_header != NULL) {
        famicomCommon.internal_save_datap =
            famicomCommonSave + FAMICOM_SAVE_HEADER_SIZE + FAMICOM_SAVE_SINGLE_SIZE * famicomCommon.save_pl_no;
    }
    if (famicomCommon.internal_save_datap != NULL) {
        nesinfo_tag_process1(famicomCommon.internal_save_datap, 1, NULL);
    }

    highscore_num = nesinfo_get_highscore_num();
    famicomCommon.highscore_flagsp = (u8*)my_malloc(highscore_num > 0 ? highscore_num : 1, 32);
    if (famicomCommon.highscore_flagsp == NULL) {
        return -1;
    }
    highscore_setup_flags(famicomCommon.highscore_flagsp);
    nesinfo_tag_process2();

    l_core = NesCore_Create();
    if (NesCore_Load(l_core, famicomCommon.nesromp, famicomCommon.nesrom_size, famicomCommon.bbramp, l_noise) !=
        NES_LOAD_OK) {
        NesCore_Destroy(l_core);
        l_core = NULL;
        return -1;
    }

    l_nes_state.wram = NesCore_GetRam(l_core);
    famicomCommon.sp = &l_nes_state;
    return 0;
}

extern int famicom_rom_load_check(void) {
    if (!l_rom_loaded) {
        if (famicomCommon.nesromp == NULL || famicom_rom_load() != 0) {
            return -1;
        }
        l_rom_loaded = TRUE;
    }

    return famicomCommon.rom_no;
}

extern int famicom_cleanup(void) {
    int res = FALSE;

    if (l_rom_loaded && save_game_image == FALSE) {
        NesCore_SyncDisk(l_core);

        if (famicomCommon.internal_save_datap != NULL) {
            nesinfo_tag_process3(famicomCommon.internal_save_datap);
        }

        if (famicomCommon.save_data_header != NULL) {
            if (famicom_save_data_check(famicomCommon.save_data_header, famicomCommon.save_pl_no,
                                        famicomCommon.save_data_name) == 0) {
                famicom_save_data_setup(famicomCommon.save_data_header, famicomCommon.save_data_single_size,
                                        famicomCommon.save_data_name);
            } else {
                res = TRUE;
            }
        }
    }

    if (l_noise != NULL) {
        EmuSound_Exit();
        l_noise = NULL;
    }

    famicom_free_buffers();
    l_rom_loaded = FALSE;
    my_malloc_current = NULL;
    return res;
}

static u8 famicom_pad_buttons(const pad_t* pad) {
    u16 button = pad->now.button;
    int stick_x = pad->now.stick_x;
    int stick_y = pad->now.stick_y;
    u8 nes = 0;

    if (button & BUTTON_A) {
        nes |= NES_BUTTON_A;
    }
    if (button & BUTTON_B) {
        nes |= NES_BUTTON_B;
    }
    if (button & (BUTTON_X | BUTTON_Z)) {
        nes |= NES_BUTTON_SELECT;
    }
    if (button & (BUTTON_Y | BUTTON_START)) {
        nes |= NES_BUTTON_START;
    }
    if ((button & BUTTON_DUP) || stick_y > FAMICOM_STICK_THRESHOLD) {
        nes |= NES_BUTTON_UP;
    }
    if ((button & BUTTON_DDOWN) || stick_y < -FAMICOM_STICK_THRESHOLD) {
        nes |= NES_BUTTON_DOWN;
    }
    if ((button & BUTTON_DLEFT) || stick_x < -FAMICOM_STICK_THRESHOLD) {
        nes |= NES_BUTTON_LEFT;
    }
    if ((button & BUTTON_DRIGHT) || stick_x > FAMICOM_STICK_THRESHOLD) {
        nes |= NES_BUTTON_RIGHT;
    }

    return nes;
}

static Gfx* famicom_draw(void) {
    const u16* frame = NesCore_GetFrame(l_core) + FAMICOM_SCREEN_TOP * NES_SCREEN_WIDTH;
    u8* texture;
    Gfx* gfx;
    int i;

    l_draw_buffer ^= 1;
    texture = l_texture[l_draw_buffer];
    for (i = 0; i < NES_SCREEN_WIDTH * FAMICOM_SCREEN_LINES; i++) {
        const u8* c = &l_nes_palette[(frame[i] & 0x3F) * 2];
        u16 rgb555 = (u16)((c[0] << 8) | c[1]);
        u16 rgba5551 = (u16)(((rgb555 & 0x7FFF) << 1) | 1);

        texture[i * 2 + 0] = (u8)(rgba5551 >> 8);
        texture[i * 2 + 1] = (u8)rgba5551;
    }

    gfx = l_display_list[l_draw_buffer];
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(gfx++, (GPACK_RGBA5551(0, 0, 0, 1) << 16) | GPACK_RGBA5551(0, 0, 0, 1));
    gDPFillRectangle(gfx++, 0, 0, FAMICOM_FRAME_WIDTH - 1, FAMICOM_FRAME_HEIGHT - 1);
    gDPPipeSync(gfx++);
    gDPSetCycleType(gfx++, G_CYC_1CYCLE);
    gDPSetRenderMode(gfx++, G_RM_OPA_SURF, G_RM_OPA_SURF2);
    gDPSetCombineMode(gfx++, G_CC_DECALRGB, G_CC_DECALRGB);
    gDPSetTexturePersp(gfx++, G_TP_NONE);
    gDPSetTextureFilter(gfx++, G_TF_POINT);
    gDPSetTextureLUT(gfx++, G_TT_NONE);
    gDPSetAlphaCompare(gfx++, G_AC_NONE);

    for (i = 0; i < FAMICOM_STRIPS; i++) {
        int line = i * FAMICOM_STRIP_LINES;
        int rows =
            FAMICOM_SCREEN_LINES - line < FAMICOM_STRIP_LINES ? FAMICOM_SCREEN_LINES - line : FAMICOM_STRIP_LINES;
        u8* strip = texture + line * NES_SCREEN_WIDTH * sizeof(u16);

        gSPInvalidateTexCache(gfx++, strip);
        gDPLoadTextureBlock(gfx++, strip, G_IM_FMT_RGBA, G_IM_SIZ_16b, NES_SCREEN_WIDTH, rows, 0,
                            G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMIRROR | G_TX_CLAMP, G_TX_NOMASK, G_TX_NOMASK,
                            G_TX_NOLOD, G_TX_NOLOD);
        gSPTextureRectangle(gfx++, FAMICOM_DRAW_X << 2, (FAMICOM_DRAW_Y + line * FAMICOM_SCALE) << 2,
                            (FAMICOM_DRAW_X + NES_SCREEN_WIDTH * FAMICOM_SCALE) << 2,
                            (FAMICOM_DRAW_Y + (line + rows) * FAMICOM_SCALE) << 2, G_TX_RENDERTILE, 0, 0,
                            (1 << 10) / FAMICOM_SCALE, (1 << 10) / FAMICOM_SCALE);
    }

    gDPPipeSync(gfx++);
    gSPEndDisplayList(gfx++);
    return l_display_list[l_draw_buffer];
}

extern void famicom_1frame(void) {
    const pad_t* pad = &gamePT->pads[0];
    u16 reset_combo = BUTTON_L | BUTTON_R;
    int held = (pad->now.button & reset_combo) == reset_combo;
    int i;

    if (l_core == NULL) {
        return;
    }

    NesCore_SetButtons(l_core, 0, famicom_pad_buttons(&gamePT->pads[0]));
    NesCore_SetButtons(l_core, 1, famicom_pad_buttons(&gamePT->pads[1]));

    if (held && (pad->on.button & BUTTON_START)) {
        nesinfo_update_highscore(famicomCommon.internal_save_datap, 1);
        NesCore_Reset(l_core, FALSE);
    }

    if (held && (pad->now.button & BUTTON_START)) {
        for (i = 0; i < 262; i++) {
            Sound_Write(0, 0, (u16)(i * 114));
        }
    } else {
        int frames = NesCore_DiskBusy(l_core) ? FAMICOM_DISK_SPEEDUP : 1;

        for (i = 0; i < frames; i++) {
            NesCore_RunFrame(l_core, i == 0);
        }
    }

    nesinfo_update_highscore(famicomCommon.internal_save_datap, 0);
    gAC_PendingDL = famicom_draw();
}

extern int famicom_internal_data_load(void) {
    famicom_save_common_setup();

    if (FamicomPort_ReadSave(famicomCommonSave, FAMICOM_SAVE_TOTAL_SIZE) != FAMICOM_SAVE_TOTAL_SIZE ||
        famicom_save_data_check(famicomCommon.save_data_header, -1, famicomCommon.save_data_name) != 0) {
        famicom_save_data_init(famicomCommon.save_data_header, FAMICOM_SAVE_SINGLE_SIZE, famicomCommon.save_data_name);
    }

    return FAMICOM_RESULT_OK;
}

extern int famicom_internal_data_save(void) {
    famicom_save_common_setup();

    if (famicom_save_data_check(famicomCommon.save_data_header, -1, famicomCommon.save_data_name) != 0) {
        return FAMICOM_RESULT_BROKEN;
    }

    famicomCommon.save_data_header->no_save = FALSE;
    famicom_save_data_setup(famicomCommon.save_data_header, FAMICOM_SAVE_SINGLE_SIZE, famicomCommon.save_data_name);
    if (!FamicomPort_WriteSave(famicomCommonSave, FAMICOM_SAVE_TOTAL_SIZE)) {
        return FAMICOM_RESULT_NOSPACE;
    }

    return FAMICOM_RESULT_OK;
}

extern int famicom_external_data_save(void) {
    FamicomSaveDataHeader* header = famicomCommon.save_data_header;

    if (!famicomCommon.nesrom_memcard || header == NULL) {
        return FAMICOM_RESULT_OK;
    }
    if (famicom_save_data_check(header, -1, famicomCommon.save_data_name) != 0) {
        return FAMICOM_RESULT_BROKEN;
    }

    header->no_save = FALSE;
    famicom_save_data_setup(header, famicomCommon.save_data_single_size, famicomCommon.save_data_name);
    if (!FamicomCard_WriteSave(famicomCommon.famicom_save_name, header, famicomCommon.save_data_total_size)) {
        return FAMICOM_RESULT_NOSPACE;
    }

    return FAMICOM_RESULT_OK;
}

extern int famicom_external_data_save_check(void) {
    return FAMICOM_RESULT_OK;
}

extern int famicom_get_disksystem_titles(int* n_games, char* title_name_bufp, int namebuf_size) {
    int count = FamicomCard_Count();

    if (title_name_bufp != NULL && namebuf_size >= FAMICOM_MORI_NAME_LEN) {
        count = FamicomCard_Titles((u8*)title_name_bufp, namebuf_size / FAMICOM_MORI_NAME_LEN);
    }
    if (n_games != NULL) {
        *n_games = count;
    }
    return TRUE;
}
