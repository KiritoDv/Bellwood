#ifndef M_BG_TEX_H
#define M_BG_TEX_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct bg_tex_obj_s {
  void* data;
  size_t size;
} mBGTex_obj_c;

#if __has_include("assets/bg_common_buffers.h")
#include "assets/bg_common_buffers.h"
#define sprashC_tex_dummy (gBgCommon.sprashC_tex_dummy)
#define sprashA_tex_dummy (gBgCommon.sprashA_tex_dummy)
#define wave3_tex_dummy (gBgCommon.wave3_tex_dummy)
#define wave2_tex_dummy (gBgCommon.wave2_tex_dummy)
#define wave1_tex_dummy (gBgCommon.wave1_tex_dummy)
#define sand_tex_dummy (gBgCommon.sand_tex_dummy)
#define beach_pal_dummy2 (gBgCommon.beach_pal_dummy2)
#define beach2_tex_dummy2 (gBgCommon.beach2_tex_dummy2)
#define beach1_tex_dummy2 (gBgCommon.beach1_tex_dummy2)
#define beach_tex_dummy (gBgCommon.beach_tex_dummy)
#define tunnel_tex_dummy (gBgCommon.tunnel_tex_dummy)
#define rail_pal_dummy (gBgCommon.rail_pal_dummy)
#define bush_pal_dummy (gBgCommon.bush_pal_dummy)
#define cliff_pal_dummy (gBgCommon.cliff_pal_dummy)
#define earth_pal_dummy (gBgCommon.earth_pal_dummy)
#define station_pal_dummy (gBgCommon.station_pal_dummy)
#define bridge_2_pal_dummy (gBgCommon.bridge_2_pal_dummy)
#define bridge_1_pal_dummy (gBgCommon.bridge_1_pal_dummy)
#define tekkyo_tex_dummy (gBgCommon.tekkyo_tex_dummy)
#define bridge_2_tex_dummy (gBgCommon.bridge_2_tex_dummy)
#define bridge_1_tex_dummy (gBgCommon.bridge_1_tex_dummy)
#define water_2_tex_dummy (gBgCommon.water_2_tex_dummy)
#define water_1_tex_dummy (gBgCommon.water_1_tex_dummy)
#define river_tex_dummy (gBgCommon.river_tex_dummy)
#define stone_tex_dummy (gBgCommon.stone_tex_dummy)
#define station_tex_dummy (gBgCommon.station_tex_dummy)
#define rail_tex_dummy (gBgCommon.rail_tex_dummy)
#define grass_tex_dummy (gBgCommon.grass_tex_dummy)
#define bush_b_tex_dummy (gBgCommon.bush_b_tex_dummy)
#define bush_a_tex_dummy (gBgCommon.bush_a_tex_dummy)
#define cliff_tex_dummy (gBgCommon.cliff_tex_dummy)
#define earth_tex_dummy (gBgCommon.earth_tex_dummy)
#else
extern u8 sprashC_tex_dummy[];
extern u8 sprashA_tex_dummy[];
extern u8 wave3_tex_dummy[];
extern u8 wave2_tex_dummy[];
extern u8 wave1_tex_dummy[];
extern u8 sand_tex_dummy[];
extern u8 beach_pal_dummy2[];
extern u8 beach2_tex_dummy2[];
extern u8 beach1_tex_dummy2[];
extern u8 beach_tex_dummy[];
extern u8 tunnel_tex_dummy[];
extern u8 rail_pal_dummy[];
extern u8 bush_pal_dummy[];
extern u8 cliff_pal_dummy[];
extern u8 earth_pal_dummy[];
extern u8 station_pal_dummy[];
extern u8 bridge_2_pal_dummy[];
extern u8 bridge_1_pal_dummy[];
extern u8 tekkyo_tex_dummy[];
extern u8 bridge_2_tex_dummy[];
extern u8 bridge_1_tex_dummy[];
extern u8 water_2_tex_dummy[];
extern u8 water_1_tex_dummy[];
extern u8 river_tex_dummy[];
extern u8 stone_tex_dummy[];
extern u8 station_tex_dummy[];
extern u8 rail_tex_dummy[];
extern u8 grass_tex_dummy[];
extern u8 bush_b_tex_dummy[];
extern u8 bush_a_tex_dummy[];
extern u8 cliff_tex_dummy[];
extern u8 earth_tex_dummy[];
#endif

#ifdef __cplusplus
}
#endif

#endif
