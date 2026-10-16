#ifndef __GBI_EXTENSIONS_H__
#define __GBI_EXTENSIONS_H__

#ifdef __cplusplus
extern "C" {
#endif

// clang-format off

#include "types.h"
#include <PR/mbi.h>

#define SOFTSPRITE_MTX_SEG 0x07
#define ANIME_1_TXT_SEG 0x08
#define ANIME_2_TXT_SEG 0x09
#define ANIME_3_TXT_SEG 0x0A
#define ANIME_4_TXT_SEG 0x0B
#define ANIME_5_TXT_SEG 0x0C
#define ANIME_6_TXT_SEG 0x0D

#define ANIME_4_TXT_SIZE 0x800
#define ANIME_6_MODEL_SIZE 0x408

#define SEG_EQUALS(seg_addr, seg) (seg_addr == SEGMENT_ADDR(seg, 0))

#define softsprite_mtx SEGMENT_ADDR(SOFTSPRITE_MTX_SEG, 0)
#define anime_1_txt SEGMENT_ADDR(ANIME_1_TXT_SEG, 0)
#define anime_1_model SEGMENT_ADDR(ANIME_1_TXT_SEG, 0)
#define anime_2_txt SEGMENT_ADDR(ANIME_2_TXT_SEG, 0)
#define anime_2_model SEGMENT_ADDR(ANIME_2_TXT_SEG, 0)
#define anime_3_txt SEGMENT_ADDR(ANIME_3_TXT_SEG, 0)
#define anime_3_model SEGMENT_ADDR(ANIME_3_TXT_SEG, 0)
#define anime_4_txt SEGMENT_ADDR(ANIME_4_TXT_SEG, 0)
#define anime_4_model SEGMENT_ADDR(ANIME_4_TXT_SEG, 0)
#define anime_5_txt SEGMENT_ADDR(ANIME_5_TXT_SEG, 0)
#define anime_5_model SEGMENT_ADDR(ANIME_5_TXT_SEG, 0)
#define anime_6_txt SEGMENT_ADDR(ANIME_6_TXT_SEG, 0)
#define anime_6_model SEGMENT_ADDR(ANIME_6_TXT_SEG, 0)
#define anime_6_mdl ((Mtx*)(uintptr_t)anime_6_model)

#define G_TRIN 0x09
#define G_TRIN_INDEPEND 0x0A
#define G_SETTEXEDGEALPHA 0xCE

#define G_CC_BLENDPRIMDECALA 0, 0, 0, PRIMITIVE, 0, 0, 0, TEXEL0

/* Triangle/Quad vertex bit size */
#define POLY_5b 0 /* 5 bits per vertex index (0 - 31) */
#define POLY_7b 1 /* 7 bits per vertex index (0 - 127) */
#define POLY_BITMASK 1

/* Extra Geometry Mode Flags */
#ifndef G_LIGHTING_POSITIONAL
#define G_LIGHTING_POSITIONAL 0x400000
#endif
#define G_DECAL_LEQUAL 0x00
#define G_DECAL_GEQUAL 0x10
#define G_DECAL_EQUAL 0x20
#define G_DECAL_ALWAYS 0x30
#define G_DECAL_SPECIAL 0x40
#define G_DECAL_ALL (G_DECAL_ALWAYS | G_DECAL_SPECIAL)

/* Indicies for G_SPECIAL_1 */
#define G_SPECIAL_NONE 0
#define G_SPECIAL_UNKNOWN 1
#define G_SPECIAL_TA_MODE 2

/* Texture Adjust Modes (used in G_SPECIAL1) */
#define G_TA_N64 0
#define G_TA_HALFTEXEL 1

#define G_TEX_EDGE_ALPHA_DEFAULT 144

typedef struct {
    unsigned char col[3];
    unsigned char kc;
    unsigned char colc[3];
    unsigned char kl;
    signed short pos[3]; /* position of light */
    unsigned char kq;
} Light_pos_t;

typedef union {
    Light_t l;
    Light_pos_t p;
    long long int force_align[2];
} Light_new;

#define G_DOLPHIN_GBI_REMOVED(name) sizeof(struct { _Static_assert(0, #name " is a Dolphin GBI command; use the N64 form"); int unused; })

#define gDPLoadTLUT_Dolphin(pkt, name, count, unk, addr) do { (void)G_DOLPHIN_GBI_REMOVED(gDPLoadTLUT_Dolphin); } while (0)
#define gsDPLoadTLUT_Dolphin(name, count, unk, addr) {{ G_DOLPHIN_GBI_REMOVED(gsDPLoadTLUT_Dolphin), 0 }}
#define gDPSetTextureImage_Dolphin(pkt, fmt, siz, h, w, img) do { (void)G_DOLPHIN_GBI_REMOVED(gDPSetTextureImage_Dolphin); } while (0)
#define gsDPSetTextureImage_Dolphin(fmt, siz, w, h, img) {{ G_DOLPHIN_GBI_REMOVED(gsDPSetTextureImage_Dolphin), 0 }}
#define gDPSetTile_Dolphin(pkt, d_fmt, tile, tlut_name, wrap_s, wrap_t, shift_s, shift_t) do { (void)G_DOLPHIN_GBI_REMOVED(gDPSetTile_Dolphin); } while (0)
#define gsDPSetTile_Dolphin(d_fmt, tile, tlut_name, wrap_s, wrap_t, shift_s, shift_t) {{ G_DOLPHIN_GBI_REMOVED(gsDPSetTile_Dolphin), 0 }}
#define gDPLoadTextureBlock_4b_Dolphin(pkt, timg, fmt, w, h, pal, ws, wt, ss, st) do { (void)G_DOLPHIN_GBI_REMOVED(gDPLoadTextureBlock_4b_Dolphin); } while (0)
#define gsDPLoadTextureBlock_4b_Dolphin(timg, fmt, w, h, pal, ws, wt, ss, st) {{ G_DOLPHIN_GBI_REMOVED(gsDPLoadTextureBlock_4b_Dolphin), 0 }}
#define gDPLoadTextureBlock_8b_Dolphin(pkt, timg, fmt, w, h, pal, ws, wt, ss, st) do { (void)G_DOLPHIN_GBI_REMOVED(gDPLoadTextureBlock_8b_Dolphin); } while (0)
#define gsDPLoadTextureBlock_8b_Dolphin(timg, fmt, w, h, pal, ws, wt, ss, st) {{ G_DOLPHIN_GBI_REMOVED(gsDPLoadTextureBlock_8b_Dolphin), 0 }}
#define gDPLoadTextureTile_4b_Dolphin(pkt, timg, fmt, w, h) do { (void)G_DOLPHIN_GBI_REMOVED(gDPLoadTextureTile_4b_Dolphin); } while (0)
#define gDPLoadMultiBlock_4b_Dolphin(pkt, timg, tile, fmt, w, h, pal, ws, wt, ss, st) do { (void)G_DOLPHIN_GBI_REMOVED(gDPLoadMultiBlock_4b_Dolphin); } while (0)
#define gsDPLoadMultiBlock_4b_Dolphin(timg, tile, fmt, w, h, pal, ws, wt, ss, st) {{ G_DOLPHIN_GBI_REMOVED(gsDPLoadMultiBlock_4b_Dolphin), 0 }}
#define gDPLoadMultiBlock_8b_Dolphin(pkt, timg, tile, fmt, w, h, pal, ws, wt, ss, st) do { (void)G_DOLPHIN_GBI_REMOVED(gDPLoadMultiBlock_8b_Dolphin); } while (0)
#define gsDPLoadMultiBlock_8b_Dolphin(timg, tile, fmt, w, h, pal, ws, wt, ss, st) {{ G_DOLPHIN_GBI_REMOVED(gsDPLoadMultiBlock_8b_Dolphin), 0 }}
#define gGXCallDisplayList(pkt, dl, nbytes) do { (void)G_DOLPHIN_GBI_REMOVED(gGXCallDisplayList); } while (0)
#define gsGXCallDisplayList(dl, nbytes) {{ G_DOLPHIN_GBI_REMOVED(gsGXCallDisplayList), 0 }}

#define gDPSetTileSizeHiRes(pkt, tile, s, t, width, height) \
do { \
    Gfx* _gfx = (Gfx*)(pkt); \
    _gfx->words.w0 = _SHIFTL(G_SETTILESIZE, 24, 8) | _SHIFTL(s, 10, 14) | _SHIFTL((width) - 1, 0, 10); \
    _gfx->words.w1 = _SHIFTL(1, 31, 1) | _SHIFTL(tile, 24, 3) | _SHIFTL(t, 10, 14) | _SHIFTL((height) - 1, 0, 10); \
} while (0)

#define gsDPSetTileSizeHiRes(tile, s, t, width, height) \
{{ \
    _SHIFTL(G_SETTILESIZE, 24, 8) | _SHIFTL(s, 10, 14) | _SHIFTL((width) - 1, 0, 10), \
    _SHIFTL(1, 31, 1) | _SHIFTL(tile, 24, 3) | _SHIFTL(t, 10, 14) | _SHIFTL((height) - 1, 0, 10) \
}}

#define gDPLoadTextureBlockWide_4b(pkt, timg, fmt, width, height, pal, cms, cmt, masks, maskt, shifts, shiftt) \
do { \
    gDPSetTextureImage(pkt, fmt, G_IM_SIZ_16b, 1, timg); \
    gDPSetTile(pkt, fmt, G_IM_SIZ_16b, 0, 0, G_TX_LOADTILE, 0, cmt, maskt, shiftt, cms, masks, shifts); \
    gDPLoadSync(pkt); \
    { \
        Gfx* _g0 = (Gfx*)(pkt); \
        Gfx* _g1 = (Gfx*)(pkt); \
        _g0->words.w0 = _SHIFTL(G_LOADBLOCK_WIDE, 24, 8) | _SHIFTL(G_TX_LOADTILE, 0, 3); \
        _g0->words.w1 = (uint32_t)((((width) * (height) + 3) >> 2) - 1); \
        _g1->words.w0 = 0; \
        _g1->words.w1 = _SHIFTL(CALC_DXT_4b(width), 0, 12); \
    } \
    gDPPipeSync(pkt); \
    gDPSetTile(pkt, fmt, G_IM_SIZ_4b, ((((width) >> 1) + 7) >> 3), 0, G_TX_RENDERTILE, pal, cmt, maskt, shiftt, cms, masks, shifts); \
    gDPSetTileSize(pkt, G_TX_RENDERTILE, 0, 0, ((width) - 1) << G_TEXTURE_IMAGE_FRAC, ((height) - 1) << G_TEXTURE_IMAGE_FRAC); \
} while (0)

/* 5 bits per vertex id (32) */
#define gsSPNTriangleData1(v0, v1, v2, flag) (_SHIFTL(v2, 10, 5) | _SHIFTL(v1, 5, 5) | _SHIFTL(v0, 0, 5))

#define G_VTX_MODE_5bit 0
#define G_VTX_MODE_7bit 1

#define gsSPNTriangles_Independ(n) \
{{ \
    _SHIFTL(G_TRIN_INDEPEND, 24, 8) | _SHIFTL((n) - 1, 17, 7), 0 \
}}

#define gSPNTriangles_5b(pkt, v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11) \
do { \
    Gfx* _g = (Gfx*)(pkt); \
    _g->words.w0 = _SHIFTL(gsSPNTriangleData1(v9, v10, v11, 0), 17, 15) | _SHIFTL(gsSPNTriangleData1(v6, v7, v8, 0), 2, 15) | _SHIFTL(_SHIFTR(gsSPNTriangleData1(v3, v4, v5, 0), 13, 2), 0, 2); \
    _g->words.w1 = _SHIFTL(gsSPNTriangleData1(v3, v4, v5, 0), 19, 13) | _SHIFTL(gsSPNTriangleData1(v0, v1, v2, 0), 4, 15) | _SHIFTL(G_VTX_MODE_5bit, 0, 1); \
} while (0)

#define gSPNTrianglesInit_5b(pkt, n, v0, v1, v2, v3, v4, v5, v6, v7, v8) \
do { \
    Gfx* _g = (Gfx*)(pkt); \
    _g->words.w0 = _SHIFTL(G_TRIN_INDEPEND, 24, 8) | _SHIFTL((n) - 1, 17, 7) | _SHIFTL(gsSPNTriangleData1(v6, v7, v8, 0), 2, 15) | _SHIFTL(_SHIFTR(gsSPNTriangleData1(v3, v4, v5, 0), 13, 2), 0, 2); \
    _g->words.w1 = _SHIFTL(gsSPNTriangleData1(v3, v4, v5, 0), 19, 13) | _SHIFTL(gsSPNTriangleData1(v0, v1, v2, 0), 4, 15) | _SHIFTL(G_VTX_MODE_5bit, 0, 1); \
} while (0)

#define gsSPNTriangles_5b(v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11) \
{{ \
    _SHIFTL(gsSPNTriangleData1(v9, v10, v11, 0), 17, 15) | _SHIFTL(gsSPNTriangleData1(v6, v7, v8, 0), 2, 15) | _SHIFTL(_SHIFTR(gsSPNTriangleData1(v3, v4, v5, 0), 13, 2), 0, 2), \
    _SHIFTL(gsSPNTriangleData1(v3, v4, v5, 0), 19, 13) | _SHIFTL(gsSPNTriangleData1(v0, v1, v2, 0), 4, 15) | _SHIFTL(G_VTX_MODE_5bit, 0, 1) \
}}

#define gsSPNTrianglesInit_5b(n, v0, v1, v2, v3, v4, v5, v6, v7, v8) \
{{ \
    _SHIFTL(G_TRIN_INDEPEND, 24, 8) | _SHIFTL((n) - 1, 17, 7) | _SHIFTL(gsSPNTriangleData1(v6, v7, v8, 0), 2, 15) | _SHIFTL(_SHIFTR(gsSPNTriangleData1(v3, v4, v5, 0), 13, 2), 0, 2), \
    _SHIFTL(gsSPNTriangleData1(v3, v4, v5, 0), 19, 13) | _SHIFTL(gsSPNTriangleData1(v0, v1, v2, 0), 4, 15) | _SHIFTL(G_VTX_MODE_5bit, 0, 1) \
}}

#define gsSPNTriangles(n, v0, v1, v2, v3, v4, v5, v6, v7, v8) \
{{ \
    _SHIFTL(G_TRIN, 24, 8) | _SHIFTL((n) - 1, 17, 7) | _SHIFTL(gsSPNTriangleData1(v6, v7, v8, 0), 2, 15) | _SHIFTL(_SHIFTR(gsSPNTriangleData1(v3, v4, v5, 0), 13, 2), 0, 2), \
    _SHIFTL(gsSPNTriangleData1(v3, v4, v5, 0), 19, 13) | _SHIFTL(gsSPNTriangleData1(v0, v1, v2, 0), 4, 15) | _SHIFTL(G_VTX_MODE_5bit, 0, 1) \
}}

#define gDPSetTexEdgeAlpha(pkt, alpha) \
do { \
    Gfx* _g = (Gfx*)(pkt); \
    _g->words.w0 = _SHIFTL(G_SETTEXEDGEALPHA, 24, 8); \
    _g->words.w1 = _SHIFTL(alpha, 0, 8); \
} while (0)

#define gsDPSetTexEdgeAlpha(alpha) \
{{ \
    _SHIFTL(G_SETTEXEDGEALPHA, 24, 8), \
    _SHIFTL(alpha, 0, 8) \
}}

#define gDPSetTextureAdjustMode(pkt, mode) \
do { \
    Gfx* _g = (Gfx*)(pkt); \
    _g->words.w0 = _SHIFTL(G_SPECIAL_1, 24, 8) | _SHIFTL(G_SPECIAL_TA_MODE, 16, 8) | _SHIFTL(mode, 0, 16); \
    _g->words.w1 = 0; \
} while (0)

#define gsDPSetTextureAdjustMode(mode) \
{{ \
    _SHIFTL(G_SPECIAL_1, 24, 8) | _SHIFTL(G_SPECIAL_TA_MODE, 16, 8) | _SHIFTL(mode, 0, 16), \
    0 \
}}

//Helpful macro for defining values of a Matrix

#ifndef gdSPDefMtx
#define qs1616(e) ((s32)((e) * 0x00010000))

#define IPART(x) ((qs1616(x) >> 16) & 0xFFFF)
#define FPART(x) (qs1616(x) & 0xFFFF)

#define gdSPDefMtx(     \
        xx, yx, zx, wx, \
        xy, yy, zy, wy, \
        xz, yz, zz, wz, \
        xw, yw, zw, ww) \
    {{                                  \
        (IPART(xx) << 0x10) | IPART(xy),  \
        (IPART(xz) << 0x10) | IPART(xw),  \
        (IPART(yx) << 0x10) | IPART(yy),  \
        (IPART(yz) << 0x10) | IPART(yw),  \
        (IPART(zx) << 0x10) | IPART(zy),  \
        (IPART(zz) << 0x10) | IPART(zw),  \
        (IPART(wx) << 0x10) | IPART(wy),  \
        (IPART(wz) << 0x10) | IPART(ww),  \
        (FPART(xx) << 0x10) | FPART(xy),  \
        (FPART(xz) << 0x10) | FPART(xw),  \
        (FPART(yx) << 0x10) | FPART(yy),  \
        (FPART(yz) << 0x10) | FPART(yw),  \
        (FPART(zx) << 0x10) | FPART(zy),  \
        (FPART(zz) << 0x10) | FPART(zw),  \
        (FPART(wx) << 0x10) | FPART(wy),  \
        (FPART(wz) << 0x10) | FPART(ww),  \
    }}
#endif

// clang-format on

#ifdef __cplusplus
}
#endif

#endif /* __GBI_EXTENSIONS_H__ */
