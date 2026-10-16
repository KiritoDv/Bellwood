#include "ac_port.h"
#include "PR/gbi.h"

#include <stdint.h>

void GameEngine_ProcessGfxCommands(Gfx* commands);
void GameEngine_SetFrameDivisor(uint32_t divisor);

#define AC_FRAME_WIDTH 640
#define AC_FRAME_HEIGHT 480
#define AC_FRAME_PROLOGUE_SIZE 16

static u16 l_color_image[AC_FRAME_WIDTH * AC_FRAME_HEIGHT];
static u16 l_depth_image[AC_FRAME_WIDTH * AC_FRAME_HEIGHT];
static Gfx l_prologue[2][AC_FRAME_PROLOGUE_SIZE];
static u32 l_frame;
static u16 l_clear_color = GPACK_RGBA5551(0, 0, 0, 1);

int AC_SetClearColor(u8 r, u8 g, u8 b) {
    l_clear_color = GPACK_RGBA5551(r, g, b, 1);
    return FALSE;
}

void AC_SubmitFrame(void* dl) {
    Gfx* head = l_prologue[l_frame & 1];
    Gfx* gfx = head;

    gDPPipeSync(gfx++);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, AC_FRAME_WIDTH, l_color_image);
    gDPSetDepthImage(gfx++, l_depth_image);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, AC_FRAME_WIDTH, AC_FRAME_HEIGHT);
    gDPSetFillColor(gfx++, (l_clear_color << 16) | l_clear_color);
    gDPFillRectangle(gfx++, 0, 0, AC_FRAME_WIDTH - 1, AC_FRAME_HEIGHT - 1);
    gDPPipeSync(gfx++);
    gSPDisplayList(gfx++, (Gfx*)dl);
    gSPEndDisplayList(gfx++);

    GameEngine_ProcessGfxCommands(head);
    l_frame++;
}

void AC_SetFrameDivisor(int divisor) {
    GameEngine_SetFrameDivisor(divisor > 0 ? (uint32_t)divisor : 1);
}
