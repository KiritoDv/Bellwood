#include "port/save/SaveGci.h"

#include <ship/Context.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

extern "C" {
#include "port/famicom/famicomPriv.h"
}

namespace fs = std::filesystem;

namespace {

constexpr const char* kGamePrefix = "DobutsunomoriP_F_";
constexpr size_t kHeaderSize = 0x20;
constexpr size_t kTitleOffset = 2;
constexpr size_t kMaxRomSize = 0x100000;

struct CardGame {
    SaveGci::CardFile file;
    size_t dataOffset;
};

std::vector<uint8_t> sRom;
std::vector<uint8_t> sTags;
std::vector<uint8_t> sSaveBuffer;

uint16_t Be16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

uint32_t Be32(const uint8_t* p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

bool Yaz0(const uint8_t* src, size_t size, std::vector<uint8_t>& out) {
    if (size < 16) {
        return false;
    }
    uint32_t outSize = Be32(src + 4);
    if (outSize > kMaxRomSize) {
        return false;
    }
    out.clear();
    out.reserve(outSize);
    size_t pos = 16;
    while (out.size() < outSize) {
        if (pos >= size) {
            return false;
        }
        uint8_t code = src[pos++];
        for (int bit = 7; bit >= 0 && out.size() < outSize; bit--) {
            if (code & (1 << bit)) {
                if (pos >= size) {
                    return false;
                }
                out.push_back(src[pos++]);
                continue;
            }
            if (pos + 2 > size) {
                return false;
            }
            uint8_t b1 = src[pos++];
            uint8_t b2 = src[pos++];
            size_t dist = (((b1 & 0x0F) << 8) | b2) + 1;
            size_t n = b1 >> 4;
            if (n == 0) {
                if (pos >= size) {
                    return false;
                }
                n = src[pos++] + 0x12;
            } else {
                n += 2;
            }
            if (dist > out.size()) {
                return false;
            }
            for (size_t i = 0; i < n && out.size() < outSize; i++) {
                out.push_back(out[out.size() - dist]);
            }
        }
    }
    return true;
}

bool Yay0(const uint8_t* src, size_t size, std::vector<uint8_t>& out) {
    if (size < 16) {
        return false;
    }
    uint32_t outSize = Be32(src + 4);
    size_t link = Be32(src + 8);
    size_t chunk = Be32(src + 12);
    if (outSize > kMaxRomSize || link > size || chunk > size) {
        return false;
    }
    out.clear();
    out.reserve(outSize);
    size_t maskPos = 16;
    uint32_t mask = 0;
    int bits = 0;
    while (out.size() < outSize) {
        if (bits == 0) {
            if (maskPos + 4 > size) {
                return false;
            }
            mask = Be32(src + maskPos);
            maskPos += 4;
            bits = 32;
        }
        bool literal = (mask & 0x80000000u) != 0;
        mask <<= 1;
        bits--;
        if (literal) {
            if (chunk >= size) {
                return false;
            }
            out.push_back(src[chunk++]);
            continue;
        }
        if (link + 2 > size) {
            return false;
        }
        uint16_t v = Be16(src + link);
        link += 2;
        size_t dist = (v & 0x0FFF) + 1;
        size_t n = v >> 12;
        if (n == 0) {
            if (chunk >= size) {
                return false;
            }
            n = src[chunk++] + 0x12;
        } else {
            n += 2;
        }
        if (dist > out.size()) {
            return false;
        }
        for (size_t i = 0; i < n && out.size() < outSize; i++) {
            out.push_back(out[out.size() - dist]);
        }
    }
    return true;
}

bool ValidGame(const SaveGci::CardFile& f, size_t* dataOffset) {
    if (f.gameCode != SaveGci::kGameCode || f.maker != "01" || f.name.rfind(kGamePrefix, 0) != 0 ||
        f.name == SaveGci::kFamicomSaveFileName) {
        return false;
    }
    uint8_t sum = 0;
    for (uint8_t b : f.data) {
        sum += b;
    }
    size_t offset = SaveGci::DataOffset(f);
    if (sum != 0 || offset == 0 || offset + kHeaderSize > f.data.size()) {
        return false;
    }
    *dataOffset = offset;
    return true;
}

fs::path GameDirectory() {
    return Ship::Context::GetPathRelativeToAppDirectory("famicom");
}

std::vector<CardGame> ListGames() {
    std::vector<CardGame> games;
    std::error_code ec;
    fs::path dir = GameDirectory();
    fs::create_directories(dir, ec);
    std::vector<fs::path> paths;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (e.is_regular_file(ec)) {
            paths.push_back(e.path());
        }
    }
    std::sort(paths.begin(), paths.end());
    for (const auto& path : paths) {
        std::vector<SaveGci::CardFile> files;
        std::string error;
        if (!SaveGci::ReadContainer(path.string(), files, &error)) {
            continue;
        }
        for (auto& f : files) {
            size_t offset;
            if (ValidGame(f, &offset)) {
                games.push_back({ std::move(f), offset });
            }
        }
    }
    return games;
}

fs::path SavePath(const char* name) {
    return fs::path(Ship::Context::GetPathRelativeToAppDirectory("save")) / "famicom" / (std::string(name) + ".bin");
}

} // namespace

extern "C" {

int FamicomCard_Count(void) {
    return (int)ListGames().size();
}

int FamicomCard_Titles(u8* names, int max_games) {
    auto games = ListGames();
    int count = std::min((int)games.size(), max_games);
    for (int i = 0; i < count; i++) {
        const uint8_t* header = games[i].file.data.data() + games[i].dataOffset;
        memcpy(names + i * FAMICOM_MORI_NAME_LEN, header + kTitleOffset, FAMICOM_MORI_NAME_LEN);
    }
    return count;
}

int FamicomCard_Load(int index, FamicomCardGame* game) {
    auto games = ListGames();
    if (index < 0 || index >= (int)games.size()) {
        return FALSE;
    }
    const auto& f = games[index].file;
    const uint8_t* end = f.data.data() + f.data.size();
    const uint8_t* header = f.data.data() + games[index].dataOffset;
    const uint8_t* p = header + kHeaderSize;
    size_t romUnits = Be16(header + 0x12);
    size_t tagsSize = Be16(header + 0x14);
    size_t commentSize = Be16(header + 0x1A);
    bool hasComment = (header[0x1C] & 0x80) != 0;

    if (p + tagsSize > end) {
        return FALSE;
    }
    sTags.assign(p, p + tagsSize);
    p += (tagsSize + 15) & ~(size_t)15;
    if (hasComment && commentSize != 0) {
        p += (commentSize + 15) & ~(size_t)15;
    }
    if (romUnits == 0 || p >= end) {
        return FALSE;
    }
    size_t avail = end - p;
    if (avail >= 4 && memcmp(p, "Yaz0", 4) == 0) {
        if (!Yaz0(p, avail, sRom)) {
            return FALSE;
        }
    } else if (avail >= 4 && memcmp(p, "Yay0", 4) == 0) {
        if (!Yay0(p, avail, sRom)) {
            return FALSE;
        }
    } else {
        size_t size = romUnits << 4;
        if (size > avail) {
            return FALSE;
        }
        sRom.assign(p, p + size);
    }

    game->rom = sRom.data();
    game->rom_size = sRom.size();
    game->tags = sTags.empty() ? NULL : sTags.data();
    game->tags_size = sTags.size();
    memset(game->save_name, 0, sizeof(game->save_name));
    snprintf(game->save_name, sizeof(game->save_name), "%s", f.name.c_str());
    game->save_name[15] = 'S';
    SPDLOG_INFO("Famicom: loaded memory card game {} ({} bytes)", f.name, sRom.size());
    return TRUE;
}

void FamicomCard_Release(void) {
    sRom.clear();
    sRom.shrink_to_fit();
    sTags.clear();
    sTags.shrink_to_fit();
}

u8* FamicomCard_SaveBuffer(size_t size) {
    if (sSaveBuffer.size() < size) {
        sSaveBuffer.resize(size);
    }
    return sSaveBuffer.data();
}

int FamicomCard_ReadSave(const char* name, void* data, size_t size) {
    std::ifstream in(SavePath(name), std::ios::binary);
    if (!in) {
        return 0;
    }
    in.read(static_cast<char*>(data), (std::streamsize)size);
    return (int)in.gcount();
}

int FamicomCard_WriteSave(const char* name, const void* data, size_t size) {
    std::error_code ec;
    fs::path path = SavePath(name);
    fs::create_directories(path.parent_path(), ec);
    fs::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(static_cast<const char*>(data), (std::streamsize)size);
        if (!out) {
            return FALSE;
        }
    }
    fs::rename(tmp, path, ec);
    if (ec) {
        SPDLOG_ERROR("Famicom: could not write {}: {}", path.string(), ec.message());
        return FALSE;
    }
    return TRUE;
}

} // extern "C"
