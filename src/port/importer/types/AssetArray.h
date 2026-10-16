#pragma once

#include <ship/resource/Resource.h>

namespace AC {
    class AssetArray : public Ship::Resource<uintptr_t> {
    public:
        using Resource::Resource;

        AssetArray() : Resource(std::shared_ptr<Ship::ResourceInitData>()) {}

        uintptr_t* GetPointer();
        size_t GetPointerSize();

        std::vector<uintptr_t> mPtrs;
        std::vector<std::string> mPaths;
    };
}