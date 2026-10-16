#include "EvwAnime.h"

namespace AC {

EvwAnimeData* EvwAnime::GetPointer() {
    return mEntries.data();
}

size_t EvwAnime::GetPointerSize() {
    return mEntries.size() * sizeof(EvwAnimeData);
}

} // namespace AC
