#include "port/save/SaveJson.h"
#include "port/save/SaveJsonHelpers.h"

#include <cstring>
#include <string>

namespace {

const char kHex[] = "0123456789abcdef";

int HexValue(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

} // namespace

void SaveJsonExpectObject(const SaveJson& j) {
    if (!j.is_object()) {
        throw SaveJson::type_error::create(302, "expected an object", &j);
    }
}

void SaveJsonPutBytes(SaveJson& j, const void* data, size_t size) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    std::string s(size * 2, '0');
    for (size_t i = 0; i < size; i++) {
        s[i * 2] = kHex[p[i] >> 4];
        s[i * 2 + 1] = kHex[p[i] & 0xF];
    }
    j = std::move(s);
}

void SaveJsonDecodeBytes(const SaveJson& j, void* data, size_t size) {
    uint8_t* p = static_cast<uint8_t*>(data);
    if (j.is_array()) {
        size_t n = std::min<size_t>(j.size(), size);
        for (size_t i = 0; i < n; i++) {
            p[i] = static_cast<uint8_t>(j[i].get<int>());
        }
        return;
    }
    const std::string& s = j.get_ref<const std::string&>();
    if (s.size() % 2 != 0) {
        throw SaveJson::type_error::create(302, "odd-length hex string", &j);
    }
    size_t n = std::min<size_t>(s.size() / 2, size);
    for (size_t i = 0; i < n; i++) {
        int hi = HexValue(s[i * 2]);
        int lo = HexValue(s[i * 2 + 1]);
        if (hi < 0 || lo < 0) {
            throw SaveJson::type_error::create(302, "bad hex digit", &j);
        }
        p[i] = static_cast<uint8_t>((hi << 4) | lo);
    }
}

void SaveJsonGetBytes(const SaveJson& j, const char* key, void* data, size_t size) {
    auto it = j.find(key);
    if (it != j.end() && !it->is_null()) {
        SaveJsonDecodeBytes(*it, data, size);
    }
}

void SaveJson_ClearPadding(SaveJsonRoot root, void* data) {
    size_t count = 0;
    const SaveJsonPadding* pad = SaveJson_Padding(root, &count);
    uint8_t* p = static_cast<uint8_t*>(data);
    for (size_t i = 0; i < count; i++) {
        for (uint32_t k = 0; k < pad[i].length; k++) {
            p[pad[i].offset + k] &= static_cast<uint8_t>(~pad[i].mask);
        }
    }
}
