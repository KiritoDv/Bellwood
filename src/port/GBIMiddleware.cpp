#include <libultraship.h>
#include "port/ShipCompat.h"

#include "GBIMiddleware.h"
#include "Engine.h"
#include <ship/resource/ResourceManager.h>
#include <fast/resource/ResourceType.h>
#include <fast/resource/type/DisplayList.h>
#include <fast/resource/type/Texture.h>

std::unordered_map<std::string, std::unordered_map<std::string, std::vector<GfxPatch>>> originalGfx;

// Attention! This is primarily for cosmetics & bug fixes. For things like mods and model replacement you should be
// using OTRs instead (When that is available). Index can be found using the commented out section below.
extern "C" void ResourceMgr_PatchGfxByName(const char* path, const char* patchName, GfxPatch* patches, int patchCount) {
    auto res = std::static_pointer_cast<Fast::DisplayList>(ShipCompat::GetResourceManager()->LoadResource(path));

    // Leaving this here for people attempting to find the correct Dlist index to patch
    /*SPDLOG_INFO("Patching DList: {}", path);
    for (int i = 0; i < res->Instructions.size(); i++) {
        Gfx* gfx = (Gfx*)&res->Instructions[i];
        // Log all commands
        SPDLOG_INFO("index:{} command:{}", i, gfx->words.w0 >> 24);
        // Log only SetPrimColors
        if (gfx->words.w0 >> 24 == G_MTX_OTR) {
            SPDLOG_INFO("index:{} r:{} g:{} b:{} a:{}", i, _SHIFTR(gfx->words.w1, 24, 8), _SHIFTR(gfx->words.w1, 16,
8), _SHIFTR(gfx->words.w1, 8, 8), _SHIFTR(gfx->words.w1, 0, 8));
        }
    }
    SPDLOG_INFO("End of DList: {}", path);*/

    // Index refers to individual gfx words, which are half the size on 32-bit
    // if (sizeof(uintptr_t) < 8) {
    // index /= 2;
    // }

    // Do not patch custom assets as they most likely do not have the same instructions as authentic assets
    if (res->GetInitData()->IsCustom) {
        return;
    }

    for (int i = 0; i < patchCount; i++) {
        GfxPatch patch = patches[i];

        // Store original gfx if not already stored
        if (originalGfx.find(path) == originalGfx.end() ||
            originalGfx[path].find(patchName) == originalGfx[path].end()) {
            originalGfx[path][patchName] = {};
            Gfx originalGfxCmd = res->Instructions[patch.index];
            GfxPatch originalPatch = { patch.index, originalGfxCmd };
            originalGfx[path][patchName].push_back(originalPatch);
        }
        // Apply patch
        res->Instructions[patch.index] = patch.instruction;
    }
}

static bool IsSegmentOrRawAddress(uintptr_t addr) {
    return addr == 0 || (addr & 1) != 0 || addr < 0x01000000;
}

static Gfx* LoadDisplayList(const char* path) {
    auto res = std::static_pointer_cast<Fast::DisplayList>(ShipCompat::GetResourceManager()->LoadResource(path));
    if (res == nullptr || res->Instructions.empty()) {
        SPDLOG_ERROR("Display list not found: {}", path);
        return nullptr;
    }
#ifdef USE_GBI_TRACE
    for (size_t i = 0; i < res->Instructions.size(); i++) {
        auto gfx = &res->Instructions[i];
        gfx->words.trace.file = path;
        gfx->words.trace.idx = i;
        gfx->words.trace.valid = 0xB00B;
    }
#endif
    return &res->Instructions[0];
}

extern "C" void gSPSegment(void* value, int segNum, uintptr_t target) {
    if (!IsSegmentOrRawAddress(target) && GameEngine_OTRSigCheck((const char*)target) == 1) {
        const char* path = (const char*)target;
        auto res = ShipCompat::GetResourceManager()->LoadResource(path);
        if (res == nullptr) {
            SPDLOG_ERROR("Segment {:#x}: resource not found: {}", segNum, path);
        } else {
            switch (res->GetInitData()->Type) {
                case (uint32_t)Fast::ResourceType::DisplayList:
                    target = (uintptr_t)&std::static_pointer_cast<Fast::DisplayList>(res)->Instructions[0];
                    break;
                case (uint32_t)Fast::ResourceType::Texture:
                    break;
                default:
                    target = (uintptr_t)res->GetRawPointer();
                    break;
            }
        }
    }

    __gSPSegment((Gfx*)value, segNum, target);
}

extern "C" void gSPSegmentLoadRes(void* value, int segNum, uintptr_t target) {
    if (!IsSegmentOrRawAddress(target) && GameEngine_OTRSigCheck((const char*)target) == 1) {
        const char* path = (const char*)target;
        auto res = ShipCompat::GetResourceManager()->LoadResource(path);
        if (res == nullptr) {
            SPDLOG_ERROR("Segment {:#x}: resource not found: {}", segNum, path);
        } else if (res->GetInitData()->Type == (uint32_t)Fast::ResourceType::DisplayList) {
            target = (uintptr_t)&std::static_pointer_cast<Fast::DisplayList>(res)->Instructions[0];
        } else if (res->GetInitData()->Type == (uint32_t)Fast::ResourceType::Texture) {
            target = (uintptr_t)std::static_pointer_cast<Fast::Texture>(res)->ImageData;
        } else {
            target = (uintptr_t)res->GetRawPointer();
        }
    }

    __gSPSegment((Gfx*)value, segNum, target);
}

extern "C" void gSPDisplayList(Gfx* pkt, Gfx* dl) {
    if (!IsSegmentOrRawAddress((uintptr_t)dl) && GameEngine_OTRSigCheck((const char*)dl) == 1) {
        dl = LoadDisplayList((const char*)dl);
    }

    __gSPDisplayList(pkt, dl);
}

extern "C" void gSPDisplayListOffset(Gfx* pkt, Gfx* dl, int offset) {
    if (!IsSegmentOrRawAddress((uintptr_t)dl) && GameEngine_OTRSigCheck((const char*)dl) == 1) {
        dl = LoadDisplayList((const char*)dl);
    }

    __gSPDisplayList(pkt, dl + offset);
}

extern "C" void gSPVertex(Gfx* pkt, uintptr_t v, int n, int v0) {
    if (!IsSegmentOrRawAddress(v) && GameEngine_OTRSigCheck((const char*)v) == 1) {
        v = (uintptr_t)ResourceGetDataByName((const char*)v);
    }

    __gSPVertex(pkt, v, n, v0);
}

extern "C" void gSPInvalidateTexCache(Gfx* pkt, uintptr_t texAddr) {
    if (!IsSegmentOrRawAddress(texAddr) && GameEngine_OTRSigCheck((const char*)texAddr) == 1) {
        auto res = ShipCompat::GetResourceManager()->LoadResource((const char*)texAddr);

        if (res == nullptr) {
            texAddr = 0;
        } else if (res->GetInitData()->Type == (uint32_t)Fast::ResourceType::DisplayList) {
            texAddr = (uintptr_t)&std::static_pointer_cast<Fast::DisplayList>(res)->Instructions[0];
        } else if (res->GetInitData()->Type == (uint32_t)Fast::ResourceType::Texture) {
            texAddr = (uintptr_t)std::static_pointer_cast<Fast::Texture>(res)->ImageData;
        } else {
            texAddr = (uintptr_t)res->GetRawPointer();
        }
    }
    __gSPInvalidateTexCache(pkt, texAddr);
}

extern "C" uint8_t ResourceMgr_FileExists(const char* filePath) {
    std::string path = filePath;
    if (path.substr(0, 7) == "__OTR__") {
        path = path.substr(7);
    }

    // return ExtensionCache.contains(path);
    return 1;
}