#include "graph.h"

#include "audio.h"
#include "famicom_emu.h"
#include "first_game.h"
#include "game.h"
#include "irqmgr.h"
#include "libc64/malloc.h"
#include "ac_port.h"
#include "jsyswrap.h"
#include "libu64/debug.h"
#include "libultra/libultra.h"
#include "libultra/initialize.h"
#include "m_bgm.h"
#include "m_debug.h"
#include "m_game_dlftbls.h"
#include "m_play.h"
#include "m_prenmi.h"
#include "m_select.h"
#include "m_trademark.h"
#include "m_vibctl.h"
#include "player_select.h"
#include "padmgr.h"
#include "gc_compat.h"
#include "save_menu.h"
#include "second_game.h"
#include "sys_dynamic.h"
#include "sys_ucode.h"
#include "zurumode.h"
#include "port/events/Events.h"

GRAPH graph_class;

static int skip_frame; // TODO: this is actually declared in graph_main
#if VERSION != VER_GAFU01_00
u8 SoftResetEnable;
#endif
static int frame; // TODO: this is actually declared in graph_task_set00

#define CONSTRUCT_THA_GA(tha_ga, name, name2) (THA_GA_ct((tha_ga), sys_dynamic.name, name2##_SIZE * sizeof(Gfx)))

static void graph_setup_double_buffer(GRAPH* this) {
    bzero(&sys_dynamic, sizeof(dynamic_t));
    sys_dynamic.start_magic = SYSDYNAMIC_START_MAGIC;
    sys_dynamic.end_magic = SYSDYNAMIC_END_MAGIC;

    CONSTRUCT_THA_GA(&this->bg_opaque_thaga, new0, NEW0);
    CONSTRUCT_THA_GA(&this->bg_translucent_thaga, new1, NEW1);
    CONSTRUCT_THA_GA(&this->polygon_opaque_thaga, poly_opa, POLY_OPA);
    CONSTRUCT_THA_GA(&this->polygon_translucent_thaga, poly_xlu, POLY_XLU);
    CONSTRUCT_THA_GA(&this->overlay_thaga, overlay, OVERLAY);
    CONSTRUCT_THA_GA(&this->work_thaga, work, WORK);
    CONSTRUCT_THA_GA(&this->font_thaga, font, FONT);
    CONSTRUCT_THA_GA(&this->shadow_thaga, shadow, SHADOW);
    CONSTRUCT_THA_GA(&this->light_thaga, light, LIGHT);

    this->Gfx_list10 = sys_dynamic.new0;
    this->Gfx_list11 = sys_dynamic.new1;
    this->Gfx_list00 = sys_dynamic.poly_opa;
    this->Gfx_list01 = sys_dynamic.poly_xlu;
    this->Gfx_list04 = sys_dynamic.overlay;
    this->Gfx_list05 = sys_dynamic.work;
    this->Gfx_list07 = sys_dynamic.font;
    this->Gfx_list08 = sys_dynamic.shadow;
    this->Gfx_list09 = sys_dynamic.light;

    this->gfxsave = NULL;
}

#define ARE_INIT_PROCS_EQUAL(proc0, proc1) (((void (*)(GAME*))proc0) == ((void (*)(GAME*))proc1))
static DLFTBL_GAME* game_get_next_game_dlftbl(GAME* game) {
    void (*next_game_init_proc)(GAME*) = game_get_next_game_init(game);

    if (ARE_INIT_PROCS_EQUAL(next_game_init_proc, first_game_init)) {
        return &game_dlftbls[0];
    } else if (ARE_INIT_PROCS_EQUAL(next_game_init_proc, select_init)) {
        return &game_dlftbls[1];
    } else if (ARE_INIT_PROCS_EQUAL(next_game_init_proc, play_init)) {
        return &game_dlftbls[2];
    } else if (ARE_INIT_PROCS_EQUAL(next_game_init_proc, second_game_init)) {
        return &game_dlftbls[3];
    } else if (ARE_INIT_PROCS_EQUAL(next_game_init_proc, trademark_init)) {
        return &game_dlftbls[5];
    } else if (ARE_INIT_PROCS_EQUAL(next_game_init_proc, player_select_init)) {
        return &game_dlftbls[6];
    } else if (ARE_INIT_PROCS_EQUAL(next_game_init_proc, save_menu_init)) {
        return &game_dlftbls[7];
    } else if (ARE_INIT_PROCS_EQUAL(next_game_init_proc, famicom_emu_init)) {
        return &game_dlftbls[8];
    } else if (ARE_INIT_PROCS_EQUAL(next_game_init_proc, prenmi_init)) {
        return &game_dlftbls[9];
    }

    return NULL;
}

extern void graph_ct(GRAPH* this) {
    bzero(this, sizeof(GRAPH));
    this->frame_counter = 0;
    this->cfb_bank = 0;
    SETREG(SREG, 33, GETREG(SREG, 33) & ~2);
    SETREG(SREG, 33, GETREG(SREG, 33) & ~1);
    zurumode_init();
    GRAPH_SET_DOING_POINT(this, CT);
}

extern void graph_dt(GRAPH* this) {
    GRAPH_SET_DOING_POINT(this, DT);
    zurumode_cleanup();
}

Gfx* gAC_PendingDL;

static void graph_task_set00(GRAPH* this) {
    GRAPH_SET_DOING_POINT(this, WAIT_TASK);
    GRAPH_SET_DOING_POINT(this, WAIT_TASK_FINISHED);
    if (ResetStatus < IRQ_RESET_DELAY) {
        this->last_dl = this->Gfx_list05;
        if (this->taskEndCallback != NULL) {
            this->taskEndCallback(this, this->taskEndData);
        }

        if (ResetStatus < IRQ_RESET_DELAY) {
            gAC_PendingDL = this->Gfx_list05; /* work data */
            frame++;
        }
    }
}

static int graph_draw_finish(GRAPH* this) {
    int err;
    OPEN_DISP(this);

    gSPBranchList(NOW_WORK_DISP++, this->Gfx_list10);
    gSPBranchList(NOW_BG_OPA_DISP++, this->Gfx_list08);
    gSPBranchList(NOW_SHADOW_DISP++, this->Gfx_list11);
    gSPBranchList(NOW_BG_XLU_DISP++, this->Gfx_list00);
    gSPBranchList(NOW_POLY_OPA_DISP++, this->Gfx_list01);
    gSPBranchList(NOW_POLY_XLU_DISP++, this->Gfx_list09);
    gSPBranchList(NOW_LIGHT_DISP++, this->Gfx_list07);
    gSPBranchList(NOW_FONT_DISP++, this->Gfx_list04);
    gDPPipeSync(NOW_OVERLAY_DISP++);
    gDPFullSync(NOW_OVERLAY_DISP++);
    gSPEndDisplayList(NOW_OVERLAY_DISP++);

    CLOSE_DISP(this);
    err = FALSE;

    SYSDYNAMIC_OPEN();
    if (!SYSDYNAMIC_CHECK_START()) {
#if VERSION == VER_GAFU01_00
        _dbg_hungup(__FILE__, 416);
#elif VERSION == VER_GAFE01_00
        _dbg_hungup(__FILE__, 417);
#endif
    }

    if (!SYSDYNAMIC_CHECK_END()) {
        err = TRUE;
#if VERSION == VER_GAFU01_00
        _dbg_hungup(__FILE__, 424);
#elif VERSION == VER_GAFE01_00
        _dbg_hungup(__FILE__, 425);
#endif
    }
    SYSDYNAMIC_CLOSE();

    if (THA_GA_isCrash(&this->polygon_opaque_thaga)) {
        err = TRUE;
    }

    if (THA_GA_isCrash(&this->polygon_translucent_thaga)) {
        err = TRUE;
    }

    if (THA_GA_isCrash(&this->overlay_thaga)) {
        err = TRUE;
    }

    if (THA_GA_isCrash(&this->font_thaga)) {
        err = TRUE;
    }

    if (THA_GA_isCrash(&this->shadow_thaga)) {
        err = TRUE;
    }

    if (THA_GA_isCrash(&this->light_thaga)) {
        err = TRUE;
    }

    if (THA_GA_isCrash(&this->bg_opaque_thaga)) {
        err = TRUE;
    }

    if (THA_GA_isCrash(&this->bg_translucent_thaga)) {
        err = TRUE;
    }

    return err;
}

static void do_soft_reset(GAME* game) {
    SoftResetEnable = FALSE;
    mBGM_reset();
    mVibctl_reset();
    sAdo_SoftReset();
    ResetTime = osGetTime();
    ResetStatus = IRQ_RESET_PRENMI;
}

static void reset_check(GRAPH* this, GAME* game) {
    if (SoftResetEnable && osShutdown) {
        do_soft_reset(game);
    }
}

// Aus version removes debug frame skip logic
#if VERSION >= VER_GAFU01_00
static void graph_main(GRAPH* this, GAME* game) {
    game->disable_prenmi = FALSE;
    graph_setup_double_buffer(this);
    game_get_controller(game);
    game->disable_display = FALSE;
    GRAPH_SET_DOING_POINT(this, GAME_MAIN);
    game_main(game);
    GRAPH_SET_DOING_POINT(this, GAME_MAIN_FINISHED);
    if (ResetStatus < IRQ_RESET_DELAY) {
        if (game->disable_display == FALSE) {
            if (graph_draw_finish(this) == FALSE) {
                GRAPH_SET_DOING_POINT(this, TASK_SET);
                graph_task_set00(this);
                GRAPH_SET_DOING_POINT(this, TASK_SET_FINISHED);
                this->frame_counter++;

                if ((GETREG(SREG, 33) & 1) != 0) {
                    SETREG(SREG, 33, GETREG(SREG, 33) & ~1);
                }
            }
        }
    }

    if (GETREG(SREG, 20) < 2) {
        GRAPH_SET_DOING_POINT(this, AUDIO);
        sAdo_GameFrame();
        GRAPH_SET_DOING_POINT(this, AUDIO_FINISHED);
    }

    reset_check(this, game);

    if (ResetStatus == IRQ_RESET_PRENMI && game->disable_prenmi == FALSE) {
        GAME_GOTO_NEXT(game, prenmi, PRENMI);
    }
}
#else
static void graph_main(GRAPH* this, GAME* game) {
    game->disable_prenmi = FALSE;
    graph_setup_double_buffer(this);
    game_get_controller(game);
    game->disable_display = FALSE;
    GRAPH_SET_DOING_POINT(this, GAME_MAIN);
    game_main(game);
    GRAPH_SET_DOING_POINT(this, GAME_MAIN_FINISHED);
    if (ResetStatus < IRQ_RESET_DELAY) {
        if (skip_frame < GETREG(SREG, 3)) {
            skip_frame++;
            this->frame_counter++;
        } else if (game->disable_display == FALSE) {
            skip_frame = 0;
            if (graph_draw_finish(this) == FALSE) {
                GRAPH_SET_DOING_POINT(this, TASK_SET);
                graph_task_set00(this);
                GRAPH_SET_DOING_POINT(this, TASK_SET_FINISHED);
                this->frame_counter++;

                if ((GETREG(SREG, 33) & 1) != 0) {
                    SETREG(SREG, 33, GETREG(SREG, 33) & ~1);
                }
            }
        }
    }

    if (GETREG(SREG, 20) < 2) {
        GRAPH_SET_DOING_POINT(this, AUDIO);
        sAdo_GameFrame();
        GRAPH_SET_DOING_POINT(this, AUDIO_FINISHED);
    }

    reset_check(this, game);

    if (ResetStatus == IRQ_RESET_PRENMI && game->disable_prenmi == FALSE) {
        GAME_GOTO_NEXT(game, prenmi, PRENMI);
    }
}
#endif

typedef enum {
    RUN_FRAME_BOOT,
    RUN_FRAME_NEW_GAME,
    RUN_FRAME_RUN,
    RUN_FRAME_DONE,
} RunFrameState;

static struct {
    RunFrameState state;
    DLFTBL_GAME* dlftbl;
    GAME* game;
} l_run_frame;

extern int AC_RunFrame(void) {
    GRAPH* __graph = &graph_class;

    if (l_run_frame.state == RUN_FRAME_BOOT) {
        graph_ct(__graph);
        l_run_frame.dlftbl = &game_dlftbls[0];
        l_run_frame.state = RUN_FRAME_NEW_GAME;
    }

    while (l_run_frame.state != RUN_FRAME_DONE) {
        if (AC_TakeRebootRequest()) {
            CALL_EVENT(GameReset);
            if (l_run_frame.game != NULL) {
                game_dt(l_run_frame.game);
                ac_free(l_run_frame.game);
                l_run_frame.game = NULL;
                game_class_p = NULL;
            }

            __osInitialize_common();
            ResetStatus = IRQ_RESET_NORMAL;
            ResetTime = 0;
            SoftResetEnable = FALSE;
            l_run_frame.dlftbl = &game_dlftbls[0];
            l_run_frame.state = RUN_FRAME_NEW_GAME;
            OSReport("AC_RunFrame: reboot\n");
        }

        if (l_run_frame.state == RUN_FRAME_NEW_GAME) {
            size_t size;
            GAME* game;

            if (l_run_frame.dlftbl == NULL) {
                graph_dt(__graph);
                l_run_frame.state = RUN_FRAME_DONE;
                break;
            }

            size = l_run_frame.dlftbl->alloc_size;
            game = (GAME*)ac_malloc(size);
            if (game == NULL) {
                _dbg_hungup(__FILE__, __LINE__);
            }
            game_class_p = game;
            bzero(game, size);
            GRAPH_SET_DOING_POINT(__graph, GAME_CT);
            game_ct(game, l_run_frame.dlftbl->init, __graph);
            gfx_texture_cache_clear();
            GRAPH_SET_DOING_POINT(__graph, GAME_CT_FINISHED);
            l_run_frame.game = game;
            l_run_frame.state = RUN_FRAME_RUN;
            OSReport("AC_RunFrame: game %d constructed\n", (int)(l_run_frame.dlftbl - game_dlftbls));
            {
                static int prev_state = BW_GAME_STATE_NONE;
                int now_state = (int)(l_run_frame.dlftbl - game_dlftbls);

                CALL_EVENT(GameStateChanged, prev_state, now_state);
                prev_state = now_state;
            }
            FrameInterpolation_ShouldInterpolateFrame(FALSE);
            FrameInterpolation_DontInterpolateCamera();
        }

        if (game_is_doing(l_run_frame.game)) {
            AC_ResetTick();
            padmgr_PortUpdate();
            CALL_EVENT(GameFrameUpdate);
            gAC_PendingDL = NULL;
            FrameInterpolation_StartRecord();
            graph_main(__graph, l_run_frame.game);
            FrameInterpolation_StopRecord();
            if (gAC_PendingDL != NULL) {
                AC_SubmitFrame(gAC_PendingDL);
            }
            return TRUE;
        }

        if (osShutdown != FALSE && game_get_next_game_init(l_run_frame.game) == second_game_init) {
            osShutdownStart(OS_RESET_RESTART);
        }

        l_run_frame.dlftbl = game_get_next_game_dlftbl(l_run_frame.game);
        GRAPH_SET_DOING_POINT(__graph, GAME_18);
        GRAPH_SET_DOING_POINT(__graph, GAME_DT);
        game_dt(l_run_frame.game);
        GRAPH_SET_DOING_POINT(__graph, GAME_DT_FINISHED);
        ac_free(l_run_frame.game);
        l_run_frame.game = NULL;
        game_class_p = NULL;
        l_run_frame.state = RUN_FRAME_NEW_GAME;
    }

    return FALSE;
}
