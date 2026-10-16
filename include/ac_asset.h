#ifndef AC_ASSET_H
#define AC_ASSET_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

bool GameEngine_OTRSigCheck(const char* data);
void* ResourceGetDataByName(const char* name);
size_t ResourceGetSizeByName(const char* name);

#define LOAD_ASSET(path)                                                                                    \
    ((path) == NULL ? NULL                                                                                  \
                    : (GameEngine_OTRSigCheck((const char*)(path)) ? ResourceGetDataByName((const char*)(path)) \
                                                                   : (void*)(path)))
#define LOAD_ASSET_RAW(path) ResourceGetDataByName((const char*)(path))
#define ASSET_SIZE(path) ResourceGetSizeByName((const char*)(path))

size_t AC_AssetCopy(void* dst, const void* asset, size_t offset, size_t size);

bool AC_AssetSame(const void* a, const void* b);

static inline unsigned short AC_Pal5551Get(const void* pal, int i) {
    const unsigned char* p = (const unsigned char*)pal + i * 2;
    return (unsigned short)((p[0] << 8) | p[1]);
}

static inline void AC_Pal5551Put(void* pal, int i, unsigned short c) {
    unsigned char* p = (unsigned char*)pal + i * 2;
    p[0] = (unsigned char)(c >> 8);
    p[1] = (unsigned char)c;
}

#if defined(__BYTE_ORDER__) && defined(__ORDER_BIG_ENDIAN__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define AC_TLUT_HOST16(c) ((unsigned short)(c))
#else
#define AC_TLUT_HOST16(c) ((unsigned short)((((c) & 0xFF) << 8) | (((c) >> 8) & 0xFF)))
#endif

static inline unsigned short AC_RGB5A3To5551(unsigned short c) {
    unsigned int r, g, b, a;
    if (c & 0x8000) {
        r = (c >> 10) & 0x1F;
        g = (c >> 5) & 0x1F;
        b = c & 0x1F;
        a = 1;
    } else {
        r = ((c >> 8) & 0xF) * 0x1F / 0xF;
        g = ((c >> 4) & 0xF) * 0x1F / 0xF;
        b = (c & 0xF) * 0x1F / 0xF;
        a = ((c >> 12) & 0x7) >= 4;
    }
    return (unsigned short)((r << 11) | (g << 6) | (b << 1) | a);
}

#ifdef __cplusplus
}
#endif

#endif
