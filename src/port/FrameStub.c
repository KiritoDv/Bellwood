#include <libultraship.h>
#include "Engine.h"
#include "FrameStub.h"

#define FRAME_STUB_WIDTH 640
#define FRAME_STUB_HEIGHT 480
#define FRAME_STUB_GFX_COUNT 32

static u16 sColorImage[FRAME_STUB_WIDTH * FRAME_STUB_HEIGHT];
static u16 sDepthImage[FRAME_STUB_WIDTH * FRAME_STUB_HEIGHT];
static Gfx sFrameGfx[2][FRAME_STUB_GFX_COUNT];
static u32 sFrameCount;

void FrameStub_Init(void) {
    sFrameCount = 0;
    GameEngine_SetFrameDivisor(1);
}

void FrameStub_Run(void) {
    Gfx* head = sFrameGfx[sFrameCount & 1];
    Gfx* gfx = head;
    u16 fill = GameEngine_IsGameArchiveLoaded() ? GPACK_RGBA5551(64, 144, 80, 1) : GPACK_RGBA5551(48, 80, 144, 1);

    gDPPipeSync(gfx++);
    gDPSetDepthImage(gfx++, sDepthImage);
    gDPSetColorImage(gfx++, G_IM_FMT_RGBA, G_IM_SIZ_16b, FRAME_STUB_WIDTH, sColorImage);
    gDPSetScissor(gfx++, G_SC_NON_INTERLACE, 0, 0, FRAME_STUB_WIDTH, FRAME_STUB_HEIGHT);
    gDPSetCycleType(gfx++, G_CYC_FILL);
    gDPSetRenderMode(gfx++, G_RM_NOOP, G_RM_NOOP2);
    gDPSetFillColor(gfx++, (fill << 16) | fill);
    gDPFillRectangle(gfx++, 0, 0, FRAME_STUB_WIDTH - 1, FRAME_STUB_HEIGHT - 1);
    gDPPipeSync(gfx++);
    gSPEndDisplayList(gfx++);

    GameEngine_ProcessGfxCommands(head);
    sFrameCount++;
}
