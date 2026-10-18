#ifdef ENABLE_SCRIPTING

#include "port/ShipCompat.h"
#include "port/ShipInit.hpp"
#include "port/api/GameText.hpp"
#include "port/api/StableIds.hpp"
#include "port/events/Events.h"

#include <fast/resource/type/Texture.h>
#include <ship/resource/ResourceManager.h>
#include <spdlog/spdlog.h>

#define this this_
extern "C" {
#include "port/api/interior.h"
}
#undef this

#include <array>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr size_t kPaletteSize = 0x20;
constexpr size_t kTileSize = 0x800;
constexpr int kTileWidth = 64;
constexpr int kCarpetTiles = 4;
constexpr int kWallTiles = 2;

struct CustomInterior {
    bool used = false;
    std::string id;
    std::string name;
    C_InteriorDef def{};
    std::vector<uint8_t> data;
};

std::array<CustomInterior, C_INTERIOR_LAST + 1 - C_CARPET_FIRST> sCarpets;
std::array<CustomInterior, C_INTERIOR_LAST + 1 - C_WALLPAPER_FIRST> sWallpapers;
StableIds sCarpetIds("custom_carpets.json", C_CARPET_FIRST, C_INTERIOR_LAST);
StableIds sWallpaperIds("custom_wallpapers.json", C_WALLPAPER_FIRST, C_INTERIOR_LAST);

CustomInterior* Find(int index, bool wall) {
    const int slot = index - (wall ? C_WALLPAPER_FIRST : C_CARPET_FIRST);
    const size_t count = wall ? sWallpapers.size() : sCarpets.size();
    if (slot < 0 || (size_t)slot >= count) {
        return nullptr;
    }
    CustomInterior& entry = wall ? sWallpapers[slot] : sCarpets[slot];
    return entry.used ? &entry : nullptr;
}

std::shared_ptr<Fast::Texture> LoadTexture(const void* asset) {
    std::string path = (const char*)asset;
    path = path.substr(7);
    auto texture = std::dynamic_pointer_cast<Fast::Texture>(ShipCompat::GetResourceManager()->LoadResource(path, true));
    if (texture == nullptr) {
        SPDLOG_ERROR("Interior: {} is not a texture", path);
    }
    return texture;
}

bool IsAsset(const void* data) {
    return strncmp((const char*)data, "__OTR__", 7) == 0;
}

bool ReadPalette(const void* palette, uint8_t* out) {
    if (!IsAsset(palette)) {
        memcpy(out, palette, kPaletteSize);
        return true;
    }
    auto texture = LoadTexture(palette);
    if (texture == nullptr) {
        return false;
    }
    if (texture->Type == Fast::TextureType::RGBA32bpp) {
        for (size_t i = 0; i < 16 && i * 4 + 3 < texture->ImageDataSize; i++) {
            const uint8_t* c = texture->ImageData + i * 4;
            const uint16_t color = ((c[0] >> 3) << 11) | ((c[1] >> 3) << 6) | ((c[2] >> 3) << 1) | (c[3] >= 0x80);
            out[i * 2] = (uint8_t)(color >> 8);
            out[i * 2 + 1] = (uint8_t)color;
        }
        return true;
    }
    if (texture->Type != Fast::TextureType::RGBA16bpp || texture->ImageDataSize < kPaletteSize) {
        SPDLOG_ERROR("Interior: the palette must have 16 RGBA16 colours");
        return false;
    }
    memcpy(out, texture->ImageData, kPaletteSize);
    return true;
}

bool ReadTiles(const void* image, int columns, int rows, uint8_t* out) {
    const int tiles = columns * rows;
    const uint8_t* pixels = (const uint8_t*)image;
    std::shared_ptr<Fast::Texture> texture;
    int width = kTileWidth * columns;
    int height = kTileWidth * rows;

    if (IsAsset(image)) {
        texture = LoadTexture(image);
        if (texture == nullptr) {
            return false;
        }
        if (texture->Type != Fast::TextureType::Palette4bpp ||
            !((texture->Width == (uint32_t)width && texture->Height == (uint32_t)height) ||
              (texture->Width == kTileWidth && texture->Height == kTileWidth))) {
            SPDLOG_ERROR("Interior: the texture must be CI4, {}x{} or {}x{}", width, height, kTileWidth, kTileWidth);
            return false;
        }
        width = texture->Width;
        height = texture->Height;
        pixels = texture->ImageData;
    }

    if (width == kTileWidth && height == kTileWidth) {
        for (int i = 0; i < tiles; i++) {
            memcpy(out + i * kTileSize, pixels, kTileSize);
        }
        return true;
    }

    const int stride = width / 2;
    for (int i = 0; i < tiles; i++) {
        const int tx = i % columns;
        const int ty = i / columns;
        for (int y = 0; y < kTileWidth; y++) {
            memcpy(out + i * kTileSize + y * (kTileWidth / 2),
                   pixels + (ty * kTileWidth + y) * stride + tx * (kTileWidth / 2), kTileWidth / 2);
        }
    }
    return true;
}

mActor_name_t Register(const C_InteriorDef* def, bool wall) {
    if (def == nullptr || def->id == nullptr || def->id[0] == '\0' || def->texture == nullptr ||
        def->palette == nullptr) {
        return EMPTY_NO;
    }
    const int columns = wall ? 1 : 2;
    const int rows = 2;
    std::vector<uint8_t> data(kPaletteSize + kTileSize * (wall ? kWallTiles : kCarpetTiles));
    if (!ReadPalette(def->palette, data.data()) ||
        !ReadTiles(def->texture, columns, rows, data.data() + kPaletteSize)) {
        SPDLOG_ERROR("Interior: could not load {}", def->id);
        return EMPTY_NO;
    }
    const int index = (wall ? sWallpaperIds : sCarpetIds).Get(def->id);
    if (index < 0) {
        return EMPTY_NO;
    }
    CustomInterior& entry = wall ? sWallpapers[index - C_WALLPAPER_FIRST] : sCarpets[index - C_CARPET_FIRST];
    entry.used = true;
    entry.id = def->id;
    entry.name = GameText_Encode(def->name != nullptr ? def->name : "", false);
    entry.name.resize(C_INTERIOR_NAME_LEN, ' ');
    entry.def = *def;
    entry.def.id = entry.id.c_str();
    entry.def.name = entry.name.c_str();
    entry.data = std::move(data);
    return (mActor_name_t)((wall ? ITM_WALL_START : ITM_CARPET_START) + index);
}

void Unregister(mActor_name_t item, bool wall) {
    const int category = wall ? ITEM1_CAT_WALL : ITEM1_CAT_CARPET;
    if (ITEM_NAME_GET_TYPE(item) != NAME_TYPE_ITEM1 || ITEM_NAME_GET_CAT(item) != category) {
        return;
    }
    if (CustomInterior* entry = Find(ITEM_NAME_GET_INDEX(item), wall)) {
        entry->used = false;
    }
}

void InitInteriorApi() {
    REGISTER_LISTENER(InteriorLookup, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        auto* lookup = reinterpret_cast<InteriorLookup*>(event);
        if (CustomInterior* entry = Find(lookup->index, lookup->wall != 0)) {
            lookup->definition = &entry->def;
            lookup->data = entry->data.data();
        }
    });
}

RegisterShipInitFunc sInitInteriorApi(InitInteriorApi);

} // namespace

extern "C" mActor_name_t C_RegisterCarpet(const C_InteriorDef* def) {
    return Register(def, false);
}

extern "C" mActor_name_t C_RegisterWallpaper(const C_InteriorDef* def) {
    return Register(def, true);
}

extern "C" void C_UnregisterCarpet(mActor_name_t item) {
    Unregister(item, false);
}

extern "C" void C_UnregisterWallpaper(mActor_name_t item) {
    Unregister(item, true);
}

#endif
