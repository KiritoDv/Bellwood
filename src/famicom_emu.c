
#include "famicom_emu.h"

#include "Famicom/famicom.h"
#include "_mem.h"
#include "gc_compat.h"
#include "ac_port.h"
#include <stdlib.h>
#include "m_common_data.h"
#include "m_debug.h"
#include "m_malloc.h"
#include "m_scene.h"

#define FAMICOM_XFB_WIDTH 640
#define FAMICOM_XFB_HEIGHT 480

static int famicom_done = FALSE;
static int famicom_done_countdown = 0;
static void* freeXfbBase = NULL;
static u32 freeXfbSize = 0;

static void my_alloc_init(GAME* game, void* start, size_t size) {
    u32 freebytes;
    uintptr_t alloc;
    uintptr_t aligned;
    u32 tsize;

    freebytes = game_getFreeBytes(game);
    alloc = (uintptr_t)THA_alloc16(&game->tha, freebytes);
    aligned = ALIGN_NEXT(alloc, 16);
    tsize = aligned - alloc;

    zelda_InitArena((void*)aligned, freebytes - tsize);

    if ((start != NULL) && (size != 0)) {
        zelda_AddBlockArena(start, size);
    }
}

static void my_alloc_cleanup() {
    zelda_CleanupArena();
}

static int my_zelda_getmemblocksize(void* ptr) {
    return zelda_GetMemBlockSize(ptr);
}

static int my_zelda_gettotalfreesize() {
    return zelda_GetTotalFreeSize();
}

static void* my_zelda_malloc_align(size_t size, u32 align) {
    void* aligned = zelda_malloc_align(size, align);

    if (aligned != NULL) {
        memset(aligned, 0xFF, size);
    }
    return aligned;
}

static void my_zelda_free(void* ptr) {
    zelda_free(ptr);
}

Famicom_MallocInfo my_malloc_func = {
    my_zelda_malloc_align,
    my_zelda_free,
    my_zelda_getmemblocksize,
    my_zelda_gettotalfreesize,
};

extern void famicom_emu_main(GAME* famicom) {
    int padid;
    pad_t* current_pad;
    u32 combo;

    if (!famicom_done) {
        if (famicom_rom_load_check() < 0) {
            Common_Set(my_room_message_control_flags, Common_Get(my_room_message_control_flags) | 1);
            famicom_done = TRUE;
            famicom_done_countdown = 0;
        } else {
            for (padid = 0; padid < 4; padid++) {
                current_pad = &gamePT->pads[padid];
                combo = current_pad->now.button | current_pad->on.button;

                if (combo == (BUTTON_L | BUTTON_R | BUTTON_Z) || combo == (BUTTON_L | BUTTON_R | BUTTON_X | BUTTON_Y)) {
                    famicom_done = TRUE;
                    famicom_done_countdown = 60;
                    AC_FadeOut(famicom_done_countdown);
                    break;
                }
            }
        }
    }
    
    if (famicom_done) {
        if (famicom_done_countdown == 0) {
            return_emu_game(famicom);
        } else {
            famicom_done_countdown--;
        }
    }

    famicom->disable_display = 1;

    if (!famicom_done) {
        famicom_1frame();
    }
}

extern void famicom_emu_init(GAME* game) {
    int rom_id;
    u8 player;
    int debug;

    famicom_done = FALSE;
    famicom_done_countdown = 0;
    game_resize_hyral(game, 0);
    Common_Set(famicom_287F9, 0);

    rom_id = Common_Get(current_famicom_rom);
    player = Common_Get(player_no);
    save_game_image = (GETREG(HREG, 4) == 1);
    debug = GETREG(HREG, 3);

    if ((rom_id > 0) && (debug != 0)) {
        rom_id = debug > 0 ? debug : 0;
    }

    game->exec = famicom_emu_main;
    game->cleanup = famicom_emu_cleanup;

    while (sAdo_SubGameOK() == FALSE) {
        VIWaitForRetrace();
        sAdo_GameFrame();
    }

    freeXfbSize = FAMICOM_XFB_WIDTH * FAMICOM_XFB_HEIGHT * sizeof(u16);
    freeXfbBase = malloc(freeXfbSize);

    my_alloc_init(game, freeXfbBase, freeXfbSize);

    if (famicom_init(rom_id, &my_malloc_func, player) != 0) {
        Common_Set(my_room_message_control_flags, Common_Get(my_room_message_control_flags) | 1);
        return_emu_game(game);
    }
}

extern void famicom_emu_cleanup(GAME* game) {
    AC_FadeIn(1);

    if (famicom_cleanup() != 0) {
        Common_Set(my_room_message_control_flags, Common_Get(my_room_message_control_flags) | 2);
    }

    my_alloc_cleanup();

    if (freeXfbBase != NULL) {
        free(freeXfbBase);
        freeXfbBase = NULL;
        freeXfbSize = 0;
    }
    sAdo_SubGameEnd();
}

extern void* famicom_gba_getImage(u32 rom_id, u32* size) {
    return NULL;
}

extern void famicom_gba_removeImage(void* p) {
}
