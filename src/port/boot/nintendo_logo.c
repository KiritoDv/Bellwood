#include "nintendo_logo.h"

#include "bootdata.h"
#include "libforest/gbi_extensions.h"
#include "assets/static/bootdata/logo_nin.h"

#define G_CC_LOGO 0, 0, 0, PRIMITIVE, TEXEL0, 0, PRIMITIVE, 0

static Vp viewport = { 1280, 960, 511, 0, 1280, 960, 511, 0 };

static Mtx lookat = {
    0x10000, 0x00000, 0x00001, 0x00000, 0x00000, 0x10000, 0x00000, 0x00001,
    0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000,
};

static Mtx default_model_scale = {
    0x20000, 0x00000, 0x00002, 0x00000, 0x00000, 0x10000, 0x00000, 0x00001,
    0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000,
};

static Mtx logo_projection = {
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, (s32)0xFFFF0000, 0x00000000, 0x00000001,
    0x00CC0000, 0x00000000, 0x000000F9, 0x00000000, 0x00000000, 0x00000000,      0x0000007C, 0x00000000,
};

static Mtx logo_model_scale = {
    0x10000, 0x00000, 0x00001, 0x00000, 0x00000, 0x10000, 0x00000, 0x00001,
    0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000, 0x00000,
};

static Gfx logo_moji_setup[] = {
    gsSPTexture(0, 0, 0, 0, G_ON),
    gsDPSetCombineMode(G_CC_BLENDPRIMDECALA, G_CC_BLENDPRIMDECALA),
    gsDPSetRenderMode(G_RM_XLU_SURF, G_RM_XLU_SURF2),
    gsSPLoadGeometryMode(G_SHADE | G_SHADING_SMOOTH),
    gsSPEndDisplayList(),
};

static Gfx logo_initial_dl[] = {
    gsDPSetScissor(G_SC_NON_INTERLACE, 0, 0, 640, 480),
    gsSPClipRatio(FRUSTRATIO_2),
    gsSPViewport(&viewport),
    gsSPMatrix(&logo_projection, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_PROJECTION),
    gsSPMatrix(&lookat, G_MTX_NOPUSH | G_MTX_MUL | G_MTX_PROJECTION),
    gsSPMatrix(&default_model_scale, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW),
    gsDPSetOtherMode(G_AD_DISABLE | G_CD_DISABLE | G_CK_NONE | G_TC_FILT | G_TF_BILERP | G_TT_NONE | G_TL_TILE |
                         G_TD_CLAMP | G_TP_PERSP | G_CYC_1CYCLE | G_PM_NPRIMITIVE,
                     G_AC_NONE | G_ZS_PIXEL | G_RM_NOOP | G_RM_NOOP2),
    gsSPEndDisplayList(),
};

void make_dl_nintendo_logo(Gfx** gpp, u32 alpha) {
    Gfx* g = *gpp;

    gSPSegment(g++, G_MWO_SEGMENT_8, logo_initial_dl);
    gSPSegment(g++, G_MWO_SEGMENT_9, logo_moji_setup);
    gSPDisplayList(g++, SEGMENT_ADDR(8, 0));
    gSPDisplayList(g++, SEGMENT_ADDR(9, 0));
    gSPMatrix(g++, &logo_model_scale, G_MTX_PUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
    gDPSetCombineMode(g++, G_CC_LOGO, G_CC_LOGO);
    gDPSetPrimColor(g++, 0, 255, LOGO_COLOR_R, LOGO_COLOR_G, LOGO_COLOR_B, alpha);
    gSPDisplayList(g++, logo_ninT_model);
    gSPPopMatrix(g++, G_MTX_MODELVIEW);

    *gpp = g;
}
