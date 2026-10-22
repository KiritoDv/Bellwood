#ifdef ENABLE_SCRIPTING

#include "port/save/SaveJsonHelpers.h"
#include "port/save/SaveMods.h"

extern "C" {
#include "port/api/savedata.h"
}

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

namespace {

const SaveJson* Find(const char* key) {
    if (key == nullptr) {
        return nullptr;
    }
    const SaveJson& data = SaveMods::TownData();
    auto it = data.find(key);
    return it != data.end() ? &*it : nullptr;
}

void Set(const char* key, SaveJson value) {
    if (key != nullptr) {
        SaveMods::TownData()[key] = std::move(value);
    }
}

} // namespace

extern "C" int C_SaveDataHas(const char* key) {
    return Find(key) != nullptr;
}

extern "C" void C_SaveDataRemove(const char* key) {
    if (key != nullptr) {
        SaveMods::TownData().erase(key);
    }
}

extern "C" int32_t C_SaveDataGetInt(const char* key, int32_t defaultValue) {
    const SaveJson* value = Find(key);
    return value != nullptr && value->is_number() ? value->get<int32_t>() : defaultValue;
}

extern "C" void C_SaveDataSetInt(const char* key, int32_t value) {
    Set(key, value);
}

extern "C" float C_SaveDataGetFloat(const char* key, float defaultValue) {
    const SaveJson* value = Find(key);
    return value != nullptr && value->is_number() ? value->get<float>() : defaultValue;
}

extern "C" void C_SaveDataSetFloat(const char* key, float value) {
    Set(key, value);
}

extern "C" const char* C_SaveDataGetString(const char* key, const char* defaultValue) {
    const SaveJson* value = Find(key);
    return value != nullptr && value->is_string() ? value->get_ref<const std::string&>().c_str() : defaultValue;
}

extern "C" void C_SaveDataSetString(const char* key, const char* value) {
    Set(key, value != nullptr ? value : "");
}

extern "C" uint32_t C_SaveDataGetBytes(const char* key, void* out, uint32_t capacity) {
    const SaveJson* value = Find(key);
    if (value == nullptr || !value->is_string()) {
        return 0;
    }
    const std::string& hex = value->get_ref<const std::string&>();
    if (hex.size() % 2 != 0) {
        return 0;
    }
    const uint32_t size = (uint32_t)(hex.size() / 2);
    if (out != nullptr && capacity > 0) {
        std::vector<uint8_t> bytes(size);
        SaveJsonDecodeBytes(*value, bytes.data(), size);
        memcpy(out, bytes.data(), std::min(size, capacity));
    }
    return size;
}

extern "C" void C_SaveDataSetBytes(const char* key, const void* data, uint32_t size) {
    if (key == nullptr || (data == nullptr && size > 0)) {
        return;
    }
    SaveJsonPutBytes(SaveMods::TownData()[key], data, size);
}

#endif
