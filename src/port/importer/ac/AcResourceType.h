#pragma once

#include <cstdint>

namespace AC {

enum class AcResourceType : uint32_t {
    TextureAnimation = 0x4F54414E, // OTAN
    NpcAnimation = 0x4F4E5041,
};

} // namespace AC
