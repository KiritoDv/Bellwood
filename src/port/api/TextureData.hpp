#pragma once

#include "port/ShipCompat.h"

#include <fast/resource/type/Texture.h>
#include <ship/resource/ResourceManager.h>
#include <spdlog/spdlog.h>

#include <cstdint>
#include <cstring>
#include <memory>
#include <string>

namespace TextureData {

constexpr size_t kPaletteSize = 0x20;

inline bool IsAsset(const void* data) {
    return strncmp((const char*)data, "__OTR__", 7) == 0;
}

inline std::shared_ptr<Fast::Texture> Load(const void* asset) {
    const std::string path = std::string((const char*)asset).substr(7);
    auto texture = std::dynamic_pointer_cast<Fast::Texture>(ShipCompat::GetResourceManager()->LoadResource(path, true));
    if (texture == nullptr) {
        SPDLOG_ERROR("{} is not a texture", path);
    }
    return texture;
}

// Reads a 16 colour palette as big-endian RGBA5551, from an asset path or raw bytes.
inline bool ReadPalette16(const void* palette, uint8_t* out) {
    if (!IsAsset(palette)) {
        memcpy(out, palette, kPaletteSize);
        return true;
    }
    auto texture = Load(palette);
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
        SPDLOG_ERROR("A palette must have 16 RGBA16 colours");
        return false;
    }
    memcpy(out, texture->ImageData, kPaletteSize);
    return true;
}

// Reads a CI4 texture of exactly width x height, from an asset path or raw bytes.
inline bool ReadCI4(const void* image, uint32_t width, uint32_t height, uint8_t* out) {
    const size_t size = width * height / 2;
    if (!IsAsset(image)) {
        memcpy(out, image, size);
        return true;
    }
    auto texture = Load(image);
    if (texture == nullptr) {
        return false;
    }
    if (texture->Type != Fast::TextureType::Palette4bpp || texture->Width != width || texture->Height != height) {
        SPDLOG_ERROR("A texture must be CI4, {}x{}", width, height);
        return false;
    }
    memcpy(out, texture->ImageData, size);
    return true;
}

} // namespace TextureData
