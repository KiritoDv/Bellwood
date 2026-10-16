#include "port/save/SaveGci.h"
#include "port/save/SaveGc.h"
#include "port/famicom/famicomPriv.h"

#define this this_
extern "C" {
#include "m_flashrom.h"
}
#undef this

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>

void SaveGcRead(const uint8_t* p, mNW_original_tex_c& v) {
    static_assert(sizeof(v.data) == mNW_ORIGINAL_DESIGN_WIDTH * mNW_ORIGINAL_DESIGN_HEIGHT / 2, "CI4 design");
    memset(v.data, 0, sizeof(v.data));
    for (int y = 0; y < mNW_ORIGINAL_DESIGN_HEIGHT; y++) {
        for (int x = 0; x < mNW_ORIGINAL_DESIGN_WIDTH; x++) {
            int tiled = (x & 7) + (y & 7) * 8 + (((x >> 3) & 3) + ((y >> 3) & 3) * 4) * 0x40;
            int linear = y * mNW_ORIGINAL_DESIGN_WIDTH + x;
            int shift = (x & 1) ? 0 : 4;
            u8 texel = (p[tiled >> 1] >> shift) & 0xF;
            v.data[linear >> 1] |= texel << shift;
        }
    }
}

namespace SaveGci {
namespace {

constexpr size_t kBlockSize = 0x2000;
constexpr size_t kEntrySize = 0x40;
constexpr size_t kDirEntryCount = 127;
constexpr size_t kFirstFileBlock = 5;
constexpr size_t kGcsHeaderSize = 0x110;
constexpr size_t kNameLen = 32;

constexpr u32 kHeaderSize = CARD_COMMENT_SIZE + 0xE00 + 0x600;
constexpr u32 kProtectOffset = kHeaderSize;
constexpr u32 kMailOffset = kHeaderSize + 32;
constexpr u32 kOriginalOffset = kMailOffset + mCD_KEEP_MAIL_SIZE;
constexpr u32 kDiaryOffset = kOriginalOffset + mCD_KEEP_ORIGINAL_SIZE;
constexpr u32 kLandOffset = sizeof(Save);
constexpr u32 kTownFileSize = kLandOffset + 2 * sizeof(Save);
constexpr u32 kPassportOffset = CARD_COMMENT_SIZE + 0xC00 + 0x200 + 0x400 * 8 + 0x200;

static_assert(mCD_ALIGN_SECTORSIZE(kDiaryOffset + mCD_KEEP_DIARY_SIZE) == kLandOffset, "town file layout");
static_assert(kTownFileSize == 0x72000, "town file size");
static_assert(mCD_ALIGN_SECTORSIZE(kPassportOffset + sizeof(mCD_foreigner_c)) == 0x6000, "passport file layout");

u16 Be16(const u8* p) {
    return (u16)((p[0] << 8) | p[1]);
}

u32 Be32(const u8* p) {
    return ((u32)p[0] << 24) | ((u32)p[1] << 16) | ((u32)p[2] << 8) | p[3];
}

u16 SumBe16(const u8* p, size_t size) {
    u16 sum = 0;
    for (size_t i = 0; i + 1 < size; i += 2) {
        sum += Be16(p + i);
    }
    return sum;
}

bool CardBlockValid(const u8* block, size_t dataOffset, size_t sumOffset) {
    u16 sum = 0;
    u16 inv = 0;
    for (size_t i = dataOffset; i + 1 < dataOffset + kBlockSize - 4; i += 2) {
        u16 w = Be16(block + i);
        sum += w;
        inv += (u16)~w;
    }
    if (sum == 0xFFFF) {
        sum = 0;
    }
    if (inv == 0xFFFF) {
        inv = 0;
    }
    return sum == Be16(block + sumOffset) && inv == Be16(block + sumOffset + 2);
}

int PickBlock(const std::vector<u8>& img, size_t a, size_t b, size_t counterOffset, size_t dataOffset,
              size_t sumOffset) {
    const u8* pa = img.data() + a * kBlockSize;
    const u8* pb = img.data() + b * kBlockSize;
    bool va = CardBlockValid(pa, dataOffset, sumOffset);
    bool vb = CardBlockValid(pb, dataOffset, sumOffset);
    if (va && vb) {
        return (s16)(Be16(pa + counterOffset) - Be16(pb + counterOffset)) >= 0 ? (int)a : (int)b;
    }
    if (va) {
        return (int)a;
    }
    if (vb) {
        return (int)b;
    }
    return -1;
}

void ParseEntry(const u8* e, CardFile& f) {
    f.gameCode.assign(reinterpret_cast<const char*>(e), 4);
    f.maker.assign(reinterpret_cast<const char*>(e + 4), 2);
    const char* name = reinterpret_cast<const char*>(e + 8);
    f.name.assign(name, strnlen(name, kNameLen));
}

bool LooksLikeEntry(const std::vector<u8>& buf, size_t at, bool exact) {
    if (buf.size() < at + kEntrySize) {
        return false;
    }
    const u8* e = buf.data() + at;
    for (int i = 0; i < 6; i++) {
        if (e[i] < 0x20 || e[i] > 0x7E) {
            return false;
        }
    }
    size_t end = at + kEntrySize + Be16(e + 0x38) * kBlockSize;
    return end > at + kEntrySize && (exact ? buf.size() == end : buf.size() >= end);
}

bool ReadSingle(const std::vector<u8>& buf, size_t at, bool exact, std::vector<CardFile>& files) {
    if (!LooksLikeEntry(buf, at, exact)) {
        return false;
    }
    CardFile f;
    ParseEntry(buf.data() + at, f);
    size_t size = Be16(buf.data() + at + 0x38) * kBlockSize;
    const u8* data = buf.data() + at + kEntrySize;
    f.data.assign(data, data + size);
    files.push_back(std::move(f));
    return true;
}

bool ReadCardImage(const std::vector<u8>& img, std::vector<CardFile>& files) {
    size_t blocks = img.size() / kBlockSize;
    if (img.size() % kBlockSize != 0 || blocks <= kFirstFileBlock) {
        return false;
    }
    int dir = PickBlock(img, 1, 2, 0x1FFA, 0, 0x1FFC);
    int bat = PickBlock(img, 3, 4, 0x0004, 4, 0);
    if (dir < 0 || bat < 0) {
        return false;
    }
    const u8* dirp = img.data() + dir * kBlockSize;
    const u8* batp = img.data() + bat * kBlockSize;
    for (size_t i = 0; i < kDirEntryCount; i++) {
        const u8* e = dirp + i * kEntrySize;
        if (Be32(e) == 0xFFFFFFFF) {
            continue;
        }
        CardFile f;
        ParseEntry(e, f);
        u16 block = Be16(e + 0x36);
        u16 count = Be16(e + 0x38);
        bool ok = count > 0;
        for (u16 n = 0; ok && n < count; n++) {
            if (block < kFirstFileBlock || block >= blocks) {
                ok = false;
                break;
            }
            const u8* data = img.data() + block * kBlockSize;
            f.data.insert(f.data.end(), data, data + kBlockSize);
            block = Be16(batp + 0x0A + (block - kFirstFileBlock) * 2);
        }
        if (ok) {
            files.push_back(std::move(f));
        } else {
            SPDLOG_WARN("save import: skipping damaged card file {}", f.name);
        }
    }
    return true;
}

template <typename T> bool ReadArea(const u8* gc, u8* out) {
    static_assert(sizeof(T) <= sizeof(mEv_area_c::data), "event save area");
    SaveGcRead(gc, *reinterpret_cast<T*>(out));
    return true;
}

bool ReadEventArea(int type, int id, const u8* gc, u8* out) {
    if (id == 15) {
        memcpy(out, gc, sizeof(mEv_area_c::data));
        return true;
    }
    if (id == mEv_EVENT_NUM) {
        return ReadArea<mEv_place_data_c>(gc, out);
    }
    switch (type) {
        case mEv_EVENT_NEW_YEARS_DAY:
            return id == 7 && ReadArea<aEv_hatumode_save_c>(gc, out);
        case mEv_EVENT_SPORTS_FAIR_TUG_OF_WAR:
            return id == 9 && ReadArea<aEv_tunahiki_c>(gc, out);
        case mEv_EVENT_SPORTS_FAIR_FOOT_RACE:
            return id == 8 && ReadArea<aEv_tokyoso_c>(gc, out);
        case mEv_EVENT_KK_SLIDER:
            return id == 10 && ReadArea<aNTT_event_save_c>(gc, out);
        case mEv_EVENT_GHOST:
            return id == 54 && ReadArea<mEv_gst_c>(gc, out);
        case mEv_EVENT_DOZAEMON:
            return id == 13 && ReadArea<mEv_dozaemon_c>(gc, out);
        case mEv_EVENT_SUMMER_CAMPER:
            return id == 0 && ReadArea<mActor_name_t>(gc, out);
        case mEv_EVENT_SONCHO_BRIDGE_MAKE:
            if (id != 34) {
                return false;
            }
            {
                u16 value = Be16(gc);
                memcpy(out, &value, sizeof(value));
                out[2] = gc[2];
            }
            return true;
        case mEv_EVENT_FISHING_TOURNEY_1:
        case mEv_EVENT_FISHING_TOURNEY_2:
            return id == 0 && ReadArea<aEANG_event_data_c>(gc, out);
        case mEv_EVENT_APRILFOOLS_DAY:
            return id == 0 && ReadArea<aAPC_event_save_data_c>(gc, out);
        case mEv_EVENT_TOY_DAY_JINGLE:
            return id == 0 && ReadArea<mEv_santa_event_c>(gc, out);
        case mEv_EVENT_HARVEST_FESTIVAL_FRANKLIN:
            return id == 0 && ReadArea<aEv_turkey_save_c>(gc, out);
        case mEv_EVENT_FIREWORKS_SHOW:
            return id == 0 && ReadArea<aEv_yomise_save_c>(gc, out);
        default:
            return false;
    }
}

void FixEventAreas(const u8* gc, mEv_save_common_data_c& common) {
    for (int i = 0; i < mEv_AREA_NUM; i++) {
        if ((common.area_use_bitfield & (1 << i)) == 0) {
            continue;
        }
        mEv_area_c& area = common.area[i];
        const u8* data = gc + offsetof(mEv_save_common_data_c, area) + i * sizeof(mEv_area_c) + offsetof(mEv_area_c, data);
        u8* out = reinterpret_cast<u8*>(area.data);
        memset(out, 0, sizeof(area.data));
        if (!ReadEventArea(area.info.type, area.info.id, data, out)) {
            SPDLOG_WARN("save import: released unknown event save area (type {}, id {})", area.info.type, area.info.id);
            common.area_use_bitfield &= ~(1 << i);
        }
    }
}

void FixErrand(const u8* gc, mQst_errand_c& errand) {
    const u8* info = gc + offsetof(mQst_errand_c, info);
    memset(&errand.info, 0, sizeof(errand.info));
    if (errand.errand_type == mQst_ERRAND_TYPE_FIRST_JOB) {
        SaveGcRead(info, errand.info.first_job);
    } else {
        SaveGcRead(info, errand.info.chain);
    }
}

void FixPrivate(const u8* gc, Private_c& priv) {
    for (size_t i = 0; i < std::size(priv.errands); i++) {
        FixErrand(gc + offsetof(Private_c, errands) + i * sizeof(mQst_errand_c), priv.errands[i]);
    }
}

void FixAnimal(const u8* gc, Animal_c& animal, bool islander) {
    for (size_t i = 0; i < std::size(animal.memories); i++) {
        Anmmem_c& memory = animal.memories[i];
        const u8* memuni = gc + offsetof(Animal_c, memories) + i * sizeof(Anmmem_c) + offsetof(Anmmem_c, memuni);
        memset(&memory.memuni, 0, sizeof(memory.memuni));
        if (islander) {
            SaveGcRead(memuni, memory.memuni.island);
        } else {
            SaveGcRead(memuni, memory.memuni.land);
        }
    }
    const u8* anmuni = gc + offsetof(Animal_c, anmuni);
    memset(&animal.anmuni, 0, sizeof(animal.anmuni));
    if (islander) {
        SaveGcRead(anmuni, animal.anmuni.island_ftr);
    } else {
        SaveGcRead(anmuni, animal.anmuni.previous_land_name);
    }
}

void FixSpecialEvent(const u8* gc, mEv_special_c& special) {
    const u8* event = gc + offsetof(mEv_special_c, event);
    memset(&special.event, 0, sizeof(special.event));
    switch (special.kind) {
        case mEv_SPNPC_DESIGNER:
            SaveGcRead(event, special.event.designer);
            break;
        case mEv_SPNPC_BROKER:
            SaveGcRead(event, special.event.broker);
            break;
        case mEv_SPNPC_ARTIST:
            SaveGcRead(event, special.event.artist);
            break;
        case mEv_SPNPC_ARABIAN:
            SaveGcRead(event, special.event.arabian);
            break;
        case mEv_SPNPC_GYPSY:
            SaveGcRead(event, special.event.gypsy);
            break;
        default:
            SaveGcRead(event, special.event.bargin);
            break;
    }
}

void FixSave(const u8* gc, Save_t& save) {
    for (size_t i = 0; i < std::size(save.private_data); i++) {
        FixPrivate(gc + offsetof(Save_t, private_data) + i * sizeof(Private_c), save.private_data[i]);
    }
    for (size_t i = 0; i < std::size(save.animals); i++) {
        FixAnimal(gc + offsetof(Save_t, animals) + i * sizeof(Animal_c), save.animals[i], false);
    }
    FixAnimal(gc + offsetof(Save_t, island) + offsetof(Island_c, animal), save.island.animal, true);

    const u8* events = gc + offsetof(Save_t, event_save_data);
    FixSpecialEvent(events + offsetof(mEv_event_save_c, special), save.event_save_data.special);
    memset(&save.event_save_data.weekly, 0, sizeof(save.event_save_data.weekly));
    SaveGcRead(events + offsetof(mEv_event_save_c, weekly), save.event_save_data.weekly.kabu_peddler);

    FixEventAreas(gc + offsetof(Save_t, event_save_common), save.event_save_common);
}

std::shared_ptr<Save> ReadLand(const u8* gc) {
    if (SumBe16(gc, sizeof(Save)) != 0) {
        return nullptr;
    }
    auto save = std::make_shared<Save>();
    memset(save.get(), 0, sizeof(Save));
    SaveGcRead(gc, save->save);
    FixSave(gc, save->save);
    if (!mFRm_CheckSaveData_common(&save->save.save_check, save->save.land_info.id)) {
        return nullptr;
    }
    return save;
}

struct KeepBuffer {
    alignas(32) u8 bytes[std::max({ mCD_KEEP_MAIL_SIZE, mCD_KEEP_ORIGINAL_SIZE, mCD_KEEP_DIARY_SIZE })];
};

bool ReadKeep(const u8* gc, u32 size, std::vector<u8>& out, void (*decode)(const u8* gc, u8* dst)) {
    if (SumBe16(gc, size) != 0) {
        return false;
    }
    auto buffer = std::make_unique<KeepBuffer>();
    memset(buffer->bytes, 0, sizeof(buffer->bytes));
    decode(gc, buffer->bytes);
    out.assign(buffer->bytes, buffer->bytes + std::min<size_t>(size, sizeof(buffer->bytes)));
    return true;
}

} // namespace

bool ReadContainer(const std::string& path, std::vector<CardFile>& files, std::string* error) {
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        *error = "Cannot open " + path + ".";
        return false;
    }
    std::vector<u8> buf((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    files.clear();
    if (buf.size() >= 6 && memcmp(buf.data(), "GCSAVE", 6) == 0 && ReadSingle(buf, kGcsHeaderSize, false, files)) {
        return true;
    }
    if (ReadCardImage(buf, files)) {
        return true;
    }
    if (ReadSingle(buf, 0, true, files)) {
        return true;
    }
    *error = "Not a GameCube save: use a .gci or .gcs file, or a whole memory card image (.raw).";
    return false;
}

const CardFile* FindFile(const std::vector<CardFile>& files, const std::string& name) {
    for (const auto& f : files) {
        if (f.gameCode == kGameCode && f.name == name) {
            return &f;
        }
    }
    return nullptr;
}

bool ReadTown(const CardFile& file, Town& out, std::string* error) {
    if (file.data.size() < kTownFileSize) {
        *error = "The town file is cut short (" + std::to_string(file.data.size()) + " bytes).";
        return false;
    }
    const u8* data = file.data.data();
    out = Town{};
    out.land = ReadLand(data + kLandOffset);
    if (!out.land) {
        out.land = ReadLand(data + kLandOffset + sizeof(Save));
        if (out.land) {
            SPDLOG_WARN("save import: main town copy is damaged, using the backup copy");
        }
    }
    ReadKeep(data + kMailOffset, mCD_KEEP_MAIL_SIZE, out.mail,
             [](const u8* gc, u8* dst) { SaveGcRead(gc, *reinterpret_cast<mCD_keep_mail_c*>(dst)); });
    ReadKeep(data + kOriginalOffset, mCD_KEEP_ORIGINAL_SIZE, out.original,
             [](const u8* gc, u8* dst) { SaveGcRead(gc, *reinterpret_cast<mCD_keep_original_c*>(dst)); });
    ReadKeep(data + kDiaryOffset, mCD_KEEP_DIARY_SIZE, out.diary,
             [](const u8* gc, u8* dst) { SaveGcRead(gc, *reinterpret_cast<mCD_keep_diary_c*>(dst)); });
    SaveGcRead(data + kProtectOffset, out.noLandCode.code);
    out.hasNoLandCode = mCD_CheckProtectCode(&out.noLandCode) == TRUE;
    if (!out.land && out.mail.empty() && out.original.empty() && out.diary.empty() && !out.hasNoLandCode) {
        *error = "The town file has no readable town, letters, designs or diary.";
        return false;
    }
    return true;
}

bool ReadPassport(const CardFile& file, mCD_foreigner_c& out) {
    if (file.data.size() < kPassportOffset + sizeof(mCD_foreigner_c)) {
        return false;
    }
    const u8* gc = file.data.data() + kPassportOffset;
    if (SumBe16(gc, sizeof(mCD_foreigner_c)) != 0) {
        return false;
    }
    memset(&out, 0, sizeof(out));
    SaveGcRead(gc, out);
    FixPrivate(gc + offsetof(mCD_foreigner_c, priv), out.priv);
    FixAnimal(gc + offsetof(mCD_foreigner_c, remove_animal), out.remove_animal, false);
    return mPr_NullCheckPersonalID(&out.priv.player_ID) == FALSE;
}

bool ReadFamicomSave(const CardFile& file, std::vector<uint8_t>& out) {
    static const u8 kName[FAMICOM_SAVE_DATA_NAME_LEN] = { 'G', 'A', 'F', 'E', 'F', 'S', 'C', 0x1A };
    const std::vector<u8>& d = file.data;
    for (size_t at = 0; at + FAMICOM_SAVE_TOTAL_SIZE <= d.size(); at += 4) {
        const u8* h = d.data() + at;
        if (memcmp(h, kName, sizeof(kName)) != 0 || h[0x0A] != FAMICOM_SAVE_HEADER_SIZE ||
            Be16(h + 0x0C) != FAMICOM_SAVE_SINGLE_SIZE) {
            continue;
        }
        u8 sum = 0;
        for (size_t i = 0; i < FAMICOM_SAVE_HEADER_SIZE; i++) {
            sum += h[i];
        }
        if (sum == 0) {
            out.assign(h, h + FAMICOM_SAVE_TOTAL_SIZE);
            return true;
        }
    }
    return false;
}

std::string DecodeName(const uint8_t* text, size_t len) {
    std::string s;
    for (size_t i = 0; i < len; i++) {
        s += (text[i] >= 0x20 && text[i] < 0x7F) ? (char)text[i] : '?';
    }
    while (!s.empty() && s.back() == ' ') {
        s.pop_back();
    }
    return s;
}

} // namespace SaveGci
