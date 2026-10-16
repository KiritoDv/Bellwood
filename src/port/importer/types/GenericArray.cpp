#include "GenericArray.h"

namespace AC {
uint8_t* GenericArray::GetPointer() {
    return mData.data();
}

size_t GenericArray::GetPointerSize() {
    return mData.size();
}
} // namespace AC