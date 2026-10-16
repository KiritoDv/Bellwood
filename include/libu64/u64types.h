#ifndef U64TYPES_H
#define U64TYPES_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct xy_s {
    f32 x, y;
} xy_t;

typedef struct xz_s {
    f32 x, z;
} xz_t;

typedef struct xyz_s {
    f32 x, y, z;
} xyz_t;

typedef struct s_xyz_s {
    s16 x, y, z;
} s_xyz;

typedef struct s_xz_s {
    s16 x, z;
} s_xz;

typedef struct {
  u32 r:8;
  u32 g:8;
  u32 b:8;
  u32 a:8;
} rgba8888_t;

typedef union {
  u32 rgba8888;
  rgba8888_t c;
} rgba8888;

static inline u32 rgba8888_get(const rgba8888* color) {
  return ((u32)color->c.r << 24) | ((u32)color->c.g << 16) | ((u32)color->c.b << 8) | (u32)color->c.a;
}

typedef union {
    struct {
        u8 r, g, b, a;
    };
    u32 rgba;
} Color_RGBA8_u32;

static inline void Color_RGBA8_u32_set(Color_RGBA8_u32* color, u32 rgba) {
    color->r = (u8)(rgba >> 24);
    color->g = (u8)(rgba >> 16);
    color->b = (u8)(rgba >> 8);
    color->a = (u8)rgba;
}

#ifdef __cplusplus
}
#endif

#endif
