#include "NpcAnimation.h"

namespace AC {

NpcAnimationData* NpcAnimation::GetPointer() {
    return &mData;
}

size_t NpcAnimation::GetPointerSize() {
    return sizeof(mData);
}

} // namespace AC
