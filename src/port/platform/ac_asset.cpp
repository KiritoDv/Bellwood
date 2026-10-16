#include <libultraship.h>
#include <ship/resource/ResourceManager.h>
#include <fast/resource/type/Texture.h>
#include "port/ShipCompat.h"

#include <cstring>
#include <mutex>
#include <string>
#include <unordered_set>

extern "C" {
#include "ac_asset.h"
}

namespace {

std::mutex sMissingMutex;
std::unordered_set<std::string> sMissing;

void ReportMissing(const char* path) {
    std::lock_guard<std::mutex> lock(sMissingMutex);
    if (sMissing.insert(path).second) {
        SPDLOG_ERROR("Missing asset {}", path);
    }
}

uint16_t Rgba32To5551(const uint8_t* c) {
    return (uint16_t)(((c[0] >> 3) << 11) | ((c[1] >> 3) << 6) | ((c[2] >> 3) << 1) | (c[3] >= 0x80 ? 1 : 0));
}

} // namespace

extern "C" size_t AC_AssetCopy(void* dst, const void* asset, size_t offset, size_t size) {
    if (dst == nullptr || size == 0) {
        return 0;
    }
    if (asset == nullptr) {
        memset(dst, 0, size);
        return 0;
    }
    if (!GameEngine_OTRSigCheck((const char*)asset)) {
        memmove(dst, (const uint8_t*)asset + offset, size);
        return size;
    }

    auto res = ShipCompat::GetResourceManager()->LoadResource((const char*)asset);
    if (res == nullptr) {
        ReportMissing((const char*)asset);
        memset(dst, 0, size);
        return 0;
    }

    auto tex = std::dynamic_pointer_cast<Fast::Texture>(res);
    if (tex != nullptr && tex->Type == Fast::TextureType::RGBA32bpp && tex->Height == 1) {
        uint8_t* out = (uint8_t*)dst;
        size_t colors = tex->ImageDataSize / 4;
        size_t first = offset / 2;
        size_t taken = 0;
        for (size_t i = 0; i < size / 2; i++) {
            uint16_t c = 0;
            if (first + i < colors) {
                c = Rgba32To5551(tex->ImageData + (first + i) * 4);
                taken += 2;
            }
            out[i * 2 + 0] = (uint8_t)(c >> 8);
            out[i * 2 + 1] = (uint8_t)c;
        }
        if (size & 1) {
            out[size - 1] = 0;
        }
        return taken;
    }

    const uint8_t* data = (const uint8_t*)res->GetRawPointer();
    size_t total = res->GetPointerSize();
    size_t n = offset < total ? std::min(size, total - offset) : 0;
    if (n != 0) {
        memcpy(dst, data + offset, n);
    }
    if (n < size) {
        memset((uint8_t*)dst + n, 0, size - n);
    }
    return n;
}

extern "C" bool AC_AssetSame(const void* a, const void* b) {
    if (a == b) {
        return true;
    }
    if (a == nullptr || b == nullptr) {
        return false;
    }
    bool pa = GameEngine_OTRSigCheck((const char*)a);
    bool pb = GameEngine_OTRSigCheck((const char*)b);
    if (pa && pb) {
        return strcmp((const char*)a, (const char*)b) == 0;
    }
    if (pa) {
        return ResourceGetDataByName((const char*)a) == b;
    }
    if (pb) {
        return ResourceGetDataByName((const char*)b) == a;
    }
    return false;
}
