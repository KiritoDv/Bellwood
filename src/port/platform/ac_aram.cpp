#include <libultraship.h>
#include <ship/resource/ResourceManager.h>
#include <fast/resource/type/Texture.h>
#include "port/ShipCompat.h"
#include "port/importer/types/AssetArray.h"

#include <array>
#include <cstring>
#include <mutex>
#include <vector>

extern "C" {
#include "jsyswrap.h"
#include "assets/arc.h"
}

namespace {

constexpr u32 kAramShift = 24;
constexpr u32 kAramOffsetMask = (1u << kAramShift) - 1;

struct AramFile {
    bool built = false;
    bool missing = false;
    std::vector<uint8_t> data;
};

std::array<AramFile, RESOURCE_NUM> sFiles;
std::mutex sMutex;

bool LoadInto(std::vector<uint8_t>& out, const char* path) {
    void* p = ResourceGetDataByName(path);
    size_t n = ResourceGetSizeByName(path);
    if (p == nullptr) {
        SPDLOG_ERROR("ARAM: missing resource {}", path);
        return false;
    }
    out.assign((uint8_t*)p, (uint8_t*)p + n);
    return true;
}

std::shared_ptr<AC::AssetArray> LoadIndex(const char* path) {
    auto res = ShipCompat::GetResourceManager()->LoadResource(path);
    auto arr = std::dynamic_pointer_cast<AC::AssetArray>(res);
    if (arr == nullptr) {
        SPDLOG_ERROR("ARAM: missing index table {}", path);
    }
    return arr;
}

uint16_t Rgba32ToRgba5551(const uint8_t* c) {
    uint16_t r = c[0] >> 3;
    uint16_t g = c[1] >> 3;
    uint16_t b = c[2] >> 3;
    uint16_t a = c[3] >= 0x80 ? 1 : 0;
    return (uint16_t)((r << 11) | (g << 6) | (b << 1) | a);
}

bool CopyTexture(const std::shared_ptr<AC::AssetArray>& index, size_t entry, uint8_t* dst, size_t size, bool tlut) {
    if (index == nullptr || entry >= index->mPtrs.size() || index->mPtrs[entry] == 0) {
        return false;
    }
    std::string path = (const char*)index->mPtrs[entry];
    if (path.starts_with("__OTR__")) {
        path = path.substr(7);
    }
    auto res = ShipCompat::GetResourceManager()->LoadResource(path, true);
    auto tex = std::dynamic_pointer_cast<Fast::Texture>(res);
    if (tex == nullptr) {
        SPDLOG_ERROR("ARAM: {} is not a texture", path);
        return false;
    }
    if (tlut && tex->Type == Fast::TextureType::RGBA32bpp) {
        size_t colors = std::min<size_t>(size / 2, tex->ImageDataSize / 4);
        for (size_t i = 0; i < colors; i++) {
            uint16_t c = Rgba32ToRgba5551(tex->ImageData + i * 4);
            dst[i * 2 + 0] = (uint8_t)(c >> 8);
            dst[i * 2 + 1] = (uint8_t)c;
        }
        return true;
    }
    memcpy(dst, tex->ImageData, std::min<size_t>(size, tex->ImageDataSize));
    return true;
}

struct RecordLayout {
    const char* index;
    size_t records;
    size_t stride;
    size_t entries_per_record;
    const size_t* entry_offsets;
    const size_t* entry_sizes;
    const bool* entry_is_tlut;
};

bool BuildRecords(std::vector<uint8_t>& out, size_t base, const RecordLayout& l) {
    auto index = LoadIndex(l.index);
    if (index == nullptr) {
        return false;
    }
    if (out.size() < base + l.records * l.stride) {
        out.resize(base + l.records * l.stride, 0);
    }
    for (size_t r = 0; r < l.records; r++) {
        for (size_t e = 0; e < l.entries_per_record; e++) {
            CopyTexture(index, r * l.entries_per_record + e, out.data() + base + r * l.stride + l.entry_offsets[e],
                        l.entry_sizes[e], l.entry_is_tlut[e]);
        }
    }
    return true;
}

bool BuildFaceBoy(std::vector<uint8_t>& out) {
    static const size_t kOffsets[15] = { 0x000, 0x100, 0x200, 0x300, 0x400, 0x500, 0x600, 0x700,
                                         0x800, 0x900, 0xA00, 0xB00, 0xC00, 0xD00, 0xE00 };
    static const size_t kSizes[15] = { 0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x100,
                                       0x100, 0x100, 0x100, 0x100, 0x100, 0x100, 0x20 };
    static const bool kTlut[15] = { false, false, false, false, false, false, false, false,
                                    false, false, false, false, false, false, true };
    static const size_t kSunOffsets[1] = { 0 };
    static const size_t kSunSizes[1] = { 0x20 };
    static const bool kSunTlut[1] = { true };

    bool ok = BuildRecords(out, 0, { arc_player_face_index, 64, 0xE20, 15, kOffsets, kSizes, kTlut });
    ok &= BuildRecords(out, 0x38800,
                       { arc_player_face_sunburn_pal_index, 256, 0x20, 1, kSunOffsets, kSunSizes, kSunTlut });
    return ok;
}

bool BuildSimple(std::vector<uint8_t>& out, const char* index, size_t records, size_t stride, bool tlut) {
    static const size_t kZero[1] = { 0 };
    size_t sizes[1] = { stride };
    bool tluts[1] = { tlut };
    return BuildRecords(out, 0, { index, records, stride, 1, kZero, sizes, tluts });
}

bool BuildRoom(std::vector<uint8_t>& out, const char* index, size_t textures) {
    size_t offsets[5];
    size_t sizes[5];
    bool tluts[5];
    offsets[0] = 0;
    sizes[0] = 0x20;
    tluts[0] = true;
    for (size_t i = 1; i <= textures; i++) {
        offsets[i] = 0x20 + (i - 1) * 0x800;
        sizes[i] = 0x800;
        tluts[i] = false;
    }
    return BuildRecords(out, 0, { index, 71, 0x20 + textures * 0x800, textures + 1, offsets, sizes, tluts });
}

const char* DirectPath(int res) {
    switch (res) {
        case RESOURCE_FGDATA:
            return arc_field_fgdata;
        case RESOURCE_FGNPCDATA:
            return arc_field_fgnpcdata;
        case RESOURCE_MAIL:
            return arc_text_mail_data;
        case RESOURCE_MAIL_TABLE:
            return arc_text_mail_data_table;
        case RESOURCE_MAILA:
            return arc_text_maila_data;
        case RESOURCE_MAILA_TABLE:
            return arc_text_maila_data_table;
        case RESOURCE_MAILB:
            return arc_text_mailb_data;
        case RESOURCE_MAILB_TABLE:
            return arc_text_mailb_data_table;
        case RESOURCE_MAILC:
            return arc_text_mailc_data;
        case RESOURCE_MAILC_TABLE:
            return arc_text_mailc_data_table;
        case RESOURCE_PS:
            return arc_text_ps_data;
        case RESOURCE_PS_TABLE:
            return arc_text_ps_data_table;
        case RESOURCE_PSZ:
            return arc_text_psz_data;
        case RESOURCE_PSZ_TABLE:
            return arc_text_psz_data_table;
        case RESOURCE_SELECT:
            return arc_text_select_data;
        case RESOURCE_SELECT_TABLE:
            return arc_text_select_data_table;
        case RESOURCE_STRING:
            return arc_text_string_data;
        case RESOURCE_STRING_TABLE:
            return arc_text_string_data_table;
        case RESOURCE_SUPERZ:
            return arc_text_superz_data;
        case RESOURCE_SUPERZ_TABLE:
            return arc_text_superz_data_table;
        case RESOURCE_SUPER:
            return arc_text_super_data;
        case RESOURCE_SUPER_TABLE:
            return arc_text_super_data_table;
        case RESOURCE_MESSAGE:
            return arc_text_message_data;
        case RESOURCE_MESSAGE_TABLE:
            return arc_text_message_data_table;
        case RESOURCE_NPC_NAME_STR_TABLE:
            return arc_text_npc_name_str_table;
        default:
            return nullptr;
    }
}

bool Build(int res, std::vector<uint8_t>& out) {
    const char* direct = DirectPath(res);
    if (direct != nullptr) {
        return LoadInto(out, direct);
    }
    switch (res) {
        case RESOURCE_TEX_BOY:
            return BuildSimple(out, arc_player_cloth_index, 256, 0x200, false);
        case RESOURCE_PALLET_BOY:
            return BuildSimple(out, arc_player_cloth_pal_index, 256, 0x20, true);
        case RESOURCE_FACE_BOY:
            return BuildFaceBoy(out);
        case RESOURCE_PLAYER_ROOM_FLOOR:
            return BuildRoom(out, arc_room_floor_index, 4);
        case RESOURCE_PLAYER_ROOM_WALL:
            return BuildRoom(out, arc_room_wall_index, 2);
        case RESOURCE_MY_ORIGINAL:
            return BuildSimple(out, arc_design_my_original_index, 4, 0x200, false);
        case RESOURCE_NEEDLEWORK_JOYBOOT:
            return BuildSimple(out, arc_design_needlework_index, 8, 0x200, false);
        default:
            return false;
    }
}

AramFile& Get(int res) {
    AramFile& f = sFiles[res];
    if (!f.built) {
        f.built = true;
        if (!Build(res, f.data)) {
            f.missing = true;
            SPDLOG_WARN("ARAM: resource {} has no port export; reads return zeros", res);
        }
    }
    return f;
}

bool ValidRes(int res) {
    return res >= 0 && res < RESOURCE_NUM;
}

} // namespace

extern "C" u32 JW_GetAramAddress(int res_no) {
    if (!ValidRes(res_no)) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(sMutex);
    Get(res_no);
    return (u32)(res_no + 1) << kAramShift;
}

extern "C" void AC_AramWriteRecord(int res_no, u32 offset, const void* data, u32 size) {
    if (!ValidRes(res_no) || data == nullptr || size == 0) {
        return;
    }
    std::lock_guard<std::mutex> lock(sMutex);
    AramFile& f = Get(res_no);
    if (f.data.size() < (size_t)offset + size) {
        f.data.resize((size_t)offset + size, 0);
    }
    memcpy(f.data.data() + offset, data, size);
}

extern "C" u32 JW_GetResSizeFileNo(int res_no) {
    if (!ValidRes(res_no)) {
        return 0;
    }
    std::lock_guard<std::mutex> lock(sMutex);
    return (u32)Get(res_no).data.size();
}

extern "C" u8* _JW_GetResourceAram(u32 aram_addr, u8* dst, u32 size) {
    if (dst == NULL || size == 0) {
        return dst;
    }
    int res = (int)(aram_addr >> kAramShift) - 1;
    u32 ofs = aram_addr & kAramOffsetMask;
    if (!ValidRes(res)) {
        SPDLOG_ERROR("ARAM: read of {} bytes from invalid address {:#x}", size, aram_addr);
        memset(dst, 0, size);
        return dst;
    }
    std::lock_guard<std::mutex> lock(sMutex);
    const AramFile& f = Get(res);
    u32 avail = ofs < f.data.size() ? (u32)(f.data.size() - ofs) : 0;
    u32 n = std::min(avail, size);
    if (n != 0) {
        memcpy(dst, f.data.data() + ofs, n);
    }
    if (n < size) {
        memset(dst + n, 0, size - n);
    }
    return dst;
}
