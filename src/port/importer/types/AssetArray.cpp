#include "AssetArray.h"

namespace AC {
uintptr_t* AssetArray::GetPointer() {
    return mPtrs.data();
}

size_t AssetArray::GetPointerSize() {
    return mPtrs.size() * sizeof(uintptr_t);
}
} // namespace AC