#ifdef ENABLE_SCRIPTING
#include "port/api/visit.h"
#include "port/save/SaveManager.h"

#include <cstring>
#include <vector>

static std::vector<uint8_t> sExported;

extern "C" uint32_t C_ExportTown(void* buffer, uint32_t capacity) {
    // The size query and the copy that follows it share one export.
    if (sExported.empty()) {
        sExported = SaveManager::ExportTown();
    }
    const uint32_t size = static_cast<uint32_t>(sExported.size());
    if (buffer != nullptr && capacity >= size) {
        memcpy(buffer, sExported.data(), size);
        sExported.clear();
    }
    return size;
}

extern "C" int C_VisitTown(const void* data, uint32_t size) {
    if (data == nullptr || size == 0) {
        return 0;
    }
    const auto* bytes = static_cast<const uint8_t*>(data);
    return SaveManager::VisitTown(std::vector<uint8_t>(bytes, bytes + size));
}

extern "C" int C_EndVisit(void) {
    return SaveManager::EndVisit();
}

extern "C" int C_IsVisiting(void) {
    return SaveManager::IsVisitor();
}
#endif
