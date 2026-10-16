#include "port/gba/DesignFiles.h"

#include <ship/Context.h>
#include <spdlog/spdlog.h>
#include <stb_image.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>

#define this this_
extern "C" {
#include "m_needlework.h"
#include "m_private.h"
}
#undef this

namespace fs = std::filesystem;

namespace DesignFiles {
namespace {

constexpr const char* kMetaFile = "designs.json";

uint32_t Crc32(const uint8_t* data, size_t size, uint32_t crc = 0) {
    crc = ~crc;
    for (size_t i = 0; i < size; i++) {
        crc ^= data[i];
        for (int k = 0; k < 8; k++) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1)));
        }
    }
    return ~crc;
}

void Put32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(v >> 24);
    out.push_back(v >> 16);
    out.push_back(v >> 8);
    out.push_back(v);
}

void Chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    Put32(out, (uint32_t)data.size());
    size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    Put32(out, Crc32(out.data() + start, out.size() - start));
}

void PaletteRgb(int palette, int index, uint8_t* rgb) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(mNW_PaletteIdx2Palette(palette)) + index * 2;
    uint16_t c = (uint16_t)((p[0] << 8) | p[1]);
    int r = (c >> 11) & 31;
    int g = (c >> 6) & 31;
    int b = (c >> 1) & 31;
    rgb[0] = (uint8_t)((r << 3) | (r >> 2));
    rgb[1] = (uint8_t)((g << 3) | (g >> 2));
    rgb[2] = (uint8_t)((b << 3) | (b >> 2));
}

int Nearest(int palette, const uint8_t* rgb, int* error) {
    int best = 0;
    int bestError = std::numeric_limits<int>::max();
    for (int i = 0; i < 16; i++) {
        uint8_t c[3];
        PaletteRgb(palette, i, c);
        int dr = rgb[0] - c[0];
        int dg = rgb[1] - c[1];
        int db = rgb[2] - c[2];
        int e = dr * dr + dg * dg + db * db;
        if (e < bestError) {
            best = i;
            bestError = e;
        }
    }
    if (error != nullptr) {
        *error = bestError;
    }
    return best;
}

int BestPalette(const std::vector<uint8_t>& rgba) {
    int best = 0;
    int64_t bestError = std::numeric_limits<int64_t>::max();
    for (int palette = 0; palette < mNW_PALETTE_COUNT; palette++) {
        int64_t total = 0;
        for (int i = 0; i < kWidth * kHeight; i++) {
            int e;
            Nearest(palette, &rgba[i * 4], &e);
            total += e;
        }
        if (total < bestError) {
            best = palette;
            bestError = total;
        }
    }
    return best;
}

std::string FileName(int slot) {
    return "design_" + std::to_string(slot + 1) + ".png";
}

std::string NameText(const uint8_t* name) {
    std::string s;
    for (int i = 0; i < mNW_ORIGINAL_DESIGN_NAME_LEN; i++) {
        s += (name[i] >= 0x20 && name[i] < 0x7F) ? (char)name[i] : '?';
    }
    while (!s.empty() && s.back() == ' ') {
        s.pop_back();
    }
    return s;
}

std::string Hex(const uint8_t* data, size_t size) {
    static const char digits[] = "0123456789abcdef";
    std::string s;
    for (size_t i = 0; i < size; i++) {
        s += digits[data[i] >> 4];
        s += digits[data[i] & 15];
    }
    return s;
}

bool Unhex(const std::string& s, uint8_t* data, size_t size) {
    if (s.size() != size * 2) {
        return false;
    }
    for (size_t i = 0; i < size; i++) {
        unsigned v;
        if (sscanf(s.c_str() + i * 2, "%2x", &v) != 1) {
            return false;
        }
        data[i] = (uint8_t)v;
    }
    return true;
}

bool ReadFile(const fs::path& path, std::vector<uint8_t>& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return false;
    }
    out.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    return true;
}

} // namespace

std::string Directory() {
    return Ship::Context::GetPathRelativeToAppDirectory("designs");
}

std::vector<uint8_t> EncodePng(const uint8_t* rgba, int width, int height) {
    std::vector<uint8_t> out = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    std::vector<uint8_t> ihdr;
    Put32(ihdr, (uint32_t)width);
    Put32(ihdr, (uint32_t)height);
    ihdr.insert(ihdr.end(), { 8, 6, 0, 0, 0 });
    Chunk(out, "IHDR", ihdr);

    std::vector<uint8_t> raw;
    for (int y = 0; y < height; y++) {
        raw.push_back(0);
        raw.insert(raw.end(), rgba + y * width * 4, rgba + (y + 1) * width * 4);
    }
    std::vector<uint8_t> zlib = { 0x78, 0x01 };
    size_t pos = 0;
    do {
        size_t n = std::min<size_t>(raw.size() - pos, 0xFFFF);
        zlib.push_back(pos + n == raw.size() ? 1 : 0);
        zlib.push_back(n & 0xFF);
        zlib.push_back(n >> 8);
        zlib.push_back(~n & 0xFF);
        zlib.push_back((~n >> 8) & 0xFF);
        zlib.insert(zlib.end(), raw.begin() + pos, raw.begin() + pos + n);
        pos += n;
    } while (pos < raw.size());
    uint32_t a = 1;
    uint32_t b = 0;
    for (uint8_t v : raw) {
        a = (a + v) % 65521;
        b = (b + a) % 65521;
    }
    Put32(zlib, (b << 16) | a);
    Chunk(out, "IDAT", zlib);
    Chunk(out, "IEND", {});
    return out;
}

bool DecodePng(const std::vector<uint8_t>& png, std::vector<uint8_t>& rgba, int* width, int* height) {
    int channels;
    stbi_uc* pixels = stbi_load_from_memory(png.data(), (int)png.size(), width, height, &channels, 4);
    if (pixels == nullptr) {
        return false;
    }
    rgba.assign(pixels, pixels + (size_t)*width * *height * 4);
    stbi_image_free(pixels);
    return true;
}

bool Export(const uint8_t* designs, int count, std::string* error) {
    std::error_code ec;
    fs::path dir = Directory();
    fs::create_directories(dir, ec);
    nlohmann::ordered_json meta = nlohmann::ordered_json::array();
    for (int slot = 0; slot < count; slot++) {
        const mNW_original_design_c* design =
            reinterpret_cast<const mNW_original_design_c*>(designs + slot * sizeof(mNW_original_design_c));
        std::vector<uint8_t> rgba(kWidth * kHeight * 4);
        for (int y = 0; y < kHeight; y++) {
            for (int x = 0; x < kWidth; x++) {
                uint8_t byte = design->design.data[(y * kWidth + x) >> 1];
                int index = (x & 1) ? (byte >> 4) : (byte & 15);
                uint8_t* px = &rgba[(y * kWidth + x) * 4];
                PaletteRgb(design->palette, index, px);
                px[3] = 0xFF;
            }
        }
        std::vector<uint8_t> png = EncodePng(rgba.data(), kWidth, kHeight);
        std::ofstream out(dir / FileName(slot), std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(png.data()), (std::streamsize)png.size());
        if (!out) {
            *error = "Cannot write " + (dir / FileName(slot)).string();
            return false;
        }
        nlohmann::ordered_json entry;
        entry["file"] = FileName(slot);
        entry["text"] = NameText(design->name);
        entry["name"] = Hex(design->name, mNW_ORIGINAL_DESIGN_NAME_LEN);
        entry["palette"] = design->palette;
        meta.push_back(entry);
    }
    std::ofstream out(dir / kMetaFile, std::ios::trunc);
    out << meta.dump(2);
    SPDLOG_INFO("designs: exported {} designs to {}", count, dir.string());
    return true;
}

bool HasImport() {
    std::error_code ec;
    fs::path dir = Directory();
    for (int slot = 0; slot < mPr_ORIGINAL_DESIGN_COUNT; slot++) {
        if (fs::exists(dir / FileName(slot), ec)) {
            return true;
        }
    }
    return false;
}

bool Import(uint8_t* designs, int count, std::string* error) {
    fs::path dir = Directory();
    nlohmann::json meta;
    std::vector<uint8_t> metaText;
    if (ReadFile(dir / kMetaFile, metaText)) {
        meta = nlohmann::json::parse(metaText.begin(), metaText.end(), nullptr, false);
    }
    int imported = 0;
    for (int slot = 0; slot < count; slot++) {
        mNW_original_design_c* design =
            reinterpret_cast<mNW_original_design_c*>(designs + slot * sizeof(mNW_original_design_c));
        std::vector<uint8_t> png;
        std::vector<uint8_t> rgba;
        int w;
        int h;
        if (!ReadFile(dir / FileName(slot), png)) {
            continue;
        }
        if (!DecodePng(png, rgba, &w, &h) || w != kWidth || h != kHeight) {
            SPDLOG_WARN("designs: {} is not a 32x32 image", FileName(slot));
            continue;
        }

        const nlohmann::json* entry = nullptr;
        if (meta.is_array()) {
            for (const auto& e : meta) {
                if (e.is_object() && e.value("file", std::string()) == FileName(slot)) {
                    entry = &e;
                }
            }
        }
        int palette = entry != nullptr && (*entry)["palette"].is_number_integer()
                          ? std::clamp((*entry)["palette"].get<int>(), 0, mNW_PALETTE_COUNT - 1)
                          : BestPalette(rgba);
        memset(design->name, ' ', sizeof(design->name));
        if (entry == nullptr || !Unhex(entry->value("name", std::string()), design->name, sizeof(design->name))) {
            std::string label = "Design " + std::to_string(slot + 1);
            memcpy(design->name, label.data(), std::min(label.size(), sizeof(design->name)));
        }
        design->palette = (u8)palette;
        memset(design->design.data, 0, sizeof(design->design.data));
        for (int y = 0; y < kHeight; y++) {
            for (int x = 0; x < kWidth; x++) {
                int index = Nearest(palette, &rgba[(y * kWidth + x) * 4], nullptr);
                design->design.data[(y * kWidth + x) >> 1] |= (uint8_t)(index << ((x & 1) ? 4 : 0));
            }
        }
        imported++;
    }
    if (imported == 0) {
        *error = "No design images in " + dir.string();
        return false;
    }
    SPDLOG_INFO("designs: imported {} designs from {}", imported, dir.string());
    return true;
}

} // namespace DesignFiles
