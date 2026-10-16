#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "port/save/SaveGame.h"

inline void SaveGcCopy(const uint8_t* p, void* dst, size_t size) {
    memcpy(dst, p, size);
}

inline uint64_t SaveGcLoad(const uint8_t* p, size_t size) {
    uint64_t value = 0;
    for (size_t i = 0; i < size; i++) {
        value = (value << 8) | p[i];
    }
    return value;
}

inline uint16_t SaveGcU16(const uint8_t* p) {
    return (uint16_t)SaveGcLoad(p, 2);
}

inline uint32_t SaveGcU32(const uint8_t* p) {
    return (uint32_t)SaveGcLoad(p, 4);
}

template <typename T>
std::enable_if_t<std::is_arithmetic_v<T> || std::is_enum_v<T>> SaveGcRead(const uint8_t* p, T& v) {
    if constexpr (sizeof(T) == 1) {
        memcpy(&v, p, 1);
    } else {
        using U = std::conditional_t<sizeof(T) == 2, uint16_t, std::conditional_t<sizeof(T) == 4, uint32_t, uint64_t>>;
        U u = (U)SaveGcLoad(p, sizeof(T));
        memcpy(&v, &u, sizeof(T));
    }
}

template <typename T, size_t N> void SaveGcRead(const uint8_t* p, T (&v)[N]) {
    if constexpr (sizeof(T) == 1 && std::is_arithmetic_v<T>) {
        memcpy(v, p, N);
    } else {
        for (size_t i = 0; i < N; i++) {
            SaveGcRead(p + i * sizeof(T), v[i]);
        }
    }
}

template <typename T> T SaveGcBits(const uint8_t* p, uint32_t start, uint32_t width) {
    uint64_t value = 0;
    for (uint32_t i = 0; i < width; i++) {
        uint32_t bit = start + i;
        value = (value << 1) | ((p[bit >> 3] >> (7 - (bit & 7))) & 1);
    }
    if constexpr (std::is_signed_v<T>) {
        if (width < 64 && ((value >> (width - 1)) & 1) != 0) {
            value |= ~0ull << width;
        }
    }
    return (T)(int64_t)value;
}

void SaveGcRead(const uint8_t* p, mNW_original_tex_c& v);

#include "port/save/SaveGc.gen.h"
