#pragma once

#include <ship/resource/Resource.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace AC {

typedef struct {
    int8_t x;
    int8_t y;
    uint8_t width;
    uint8_t height;
} EvwAnimeScroll;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
    uint8_t l;
} EvwAnimeColPrim;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
} EvwAnimeColEnv;

typedef struct {
    uint16_t frame_count;
    uint16_t key_count;
    EvwAnimeColPrim* prim_colors;
    EvwAnimeColEnv* env_colors;
    uint16_t* keyframes;
} EvwAnimeColReg;

typedef struct {
    uint16_t frame_count;
    uint16_t key_count;
    void** texture_tbl;
    uint8_t* animation_pattern;
    uint16_t* keyframes;
} EvwAnimeTexAnime;

typedef struct {
    int8_t segment;
    int16_t type;
    void* data_p;
} EvwAnimeData;

enum EvwAnimeType : int16_t {
    EVW_ANIME_SCROLL1,
    EVW_ANIME_SCROLL2,
    EVW_ANIME_COLREG_MANUAL,
    EVW_ANIME_COLREG_LINEAR,
    EVW_ANIME_COLREG_NONLINEAR,
    EVW_ANIME_TEXANIME,
};

struct EvwAnimeStorage {
    EvwAnimeScroll scroll[2]{};
    EvwAnimeColReg colreg{};
    EvwAnimeTexAnime texanime{};
    std::vector<EvwAnimeColPrim> prim;
    std::vector<EvwAnimeColEnv> env;
    std::vector<uint16_t> keyframes;
    std::vector<std::string> textures;
    std::vector<void*> textureTable;
    std::vector<uint8_t> pattern;
};

class EvwAnime : public Ship::Resource<EvwAnimeData> {
  public:
    using Resource::Resource;

    EvwAnime() : Resource(std::shared_ptr<Ship::ResourceInitData>()) {
    }

    EvwAnimeData* GetPointer() override;
    size_t GetPointerSize() override;

    std::vector<EvwAnimeData> mEntries;
    std::vector<std::unique_ptr<EvwAnimeStorage>> mStorage;
};

} // namespace AC
