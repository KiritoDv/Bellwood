#pragma once

namespace AC {
enum class ResourceType {
    GenericArray = 0x47415252, // GARR
    AssetArray = 0x41415252,   // AARR
    RawTexture = 0x52544558,   // RTEX
    KeyFrameSkel = 0x4F4B4653, // OKFS
    KeyFrameAnim = 0x4F4B4641, // OKFA
};
} // namespace AC
