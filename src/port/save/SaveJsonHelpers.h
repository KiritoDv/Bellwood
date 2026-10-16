#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include "port/save/SaveJson.h"

void SaveJsonExpectObject(const SaveJson& j);
void SaveJsonPutBytes(SaveJson& j, const void* data, size_t size);
void SaveJsonGetBytes(const SaveJson& j, const char* key, void* data, size_t size);
void SaveJsonDecodeBytes(const SaveJson& j, void* data, size_t size);

template <typename T> void SaveJsonPut(SaveJson& j, const T& v) {
    if constexpr (std::is_array_v<T>) {
        using E = std::remove_extent_t<T>;
        if constexpr (std::is_arithmetic_v<E> && sizeof(E) == 1) {
            SaveJsonPutBytes(j, v, sizeof(T));
        } else {
            j = SaveJson::array();
            for (size_t i = 0; i < std::extent_v<T>; i++) {
                j.push_back(SaveJson());
                SaveJsonPut(j.back(), v[i]);
            }
        }
    } else if constexpr (std::is_enum_v<T>) {
        j = static_cast<int64_t>(v);
    } else if constexpr (std::is_arithmetic_v<T>) {
        if constexpr (sizeof(T) == 1 && std::is_signed_v<T> && !std::is_same_v<T, char>) {
            j = static_cast<int>(v);
        } else if constexpr (sizeof(T) == 1) {
            j = static_cast<unsigned>(static_cast<uint8_t>(v));
        } else {
            j = v;
        }
    } else {
        to_json(j, v);
    }
}

template <typename T> void SaveJsonGetValue(const SaveJson& j, T& v) {
    if constexpr (std::is_array_v<T>) {
        using E = std::remove_extent_t<T>;
        if constexpr (std::is_arithmetic_v<E> && sizeof(E) == 1) {
            if (j.is_string()) {
                SaveJsonDecodeBytes(j, v, sizeof(T));
                return;
            }
        }
        if (!j.is_array()) {
            throw SaveJson::type_error::create(302, "expected an array", &j);
        }
        size_t n = std::min<size_t>(j.size(), std::extent_v<T>);
        for (size_t i = 0; i < n; i++) {
            SaveJsonGetValue(j[i], v[i]);
        }
    } else if constexpr (std::is_enum_v<T>) {
        v = static_cast<T>(j.get<int64_t>());
    } else if constexpr (std::is_arithmetic_v<T>) {
        if constexpr (sizeof(T) == 1) {
            v = static_cast<T>(j.get<int>());
        } else {
            v = j.get<T>();
        }
    } else {
        from_json(j, v);
    }
}

template <typename T> void SaveJsonGet(const SaveJson& j, const char* key, T& v) {
    auto it = j.find(key);
    if (it != j.end() && !it->is_null()) {
        SaveJsonGetValue(*it, v);
    }
}
