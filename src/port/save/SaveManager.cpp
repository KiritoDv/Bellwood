#include "port/save/SaveManager.h"
#include "port/save/SaveGci.h"
#include "port/save/SaveGifts.h"
#include "port/save/SaveJson.h"

#include <ship/Api.h>
#include <ship/Context.h>
#include <ship/window/Window.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "port/ShipCompat.h"
#include "port/events/Events.h"
#include "port/ui/cvar_prefixes.h"

#define this this_
extern "C" {
#include "audio.h"
#include "audio_defs.h"
#include "boot.h"
#include "Famicom/famicom.h"
#include "game.h"
#include "libu64/gfxprint.h"
#include "m_actor.h"
#include "m_event.h"
#include "m_flashrom.h"
#include "m_land.h"
#include "m_play.h"
#include "m_scene_table.h"
#include "m_time.h"
}
#undef this

API_EXPORT int32_t CVarGetInteger(const char* name, int32_t defaultValue);
API_EXPORT const char* CVarGetString(const char* name, const char* defaultValue);
API_EXPORT void CVarSetString(const char* name, const char* value);
API_EXPORT void CVarSetInteger(const char* name, int32_t value);
API_EXPORT void CVarSave();

namespace fs = std::filesystem;

namespace {

constexpr const char* kFormatName = "bellwood-save";
constexpr int kFormatVersion = 1;
constexpr const char* kHomeFile = "town.json";
constexpr const char* kTownDir = "towns";
constexpr const char* kLandFileName = "DobutsunomoriP_MURA";
constexpr const char* kPassportPrefix = "DobutsunomoriP_PL_";
constexpr int kSlotHome = mCD_SLOT_A;
constexpr int kSlotOther = mCD_SLOT_B;
constexpr int kCardPrivateMax = 19;
constexpr int kPassportMax = 127;
constexpr int kSaveFrames = 112;
constexpr int kEraseFrames = 170;

#define CVAR_SAVE_RAW_MIRROR CVAR_SETTING("Save.RawMirror")
#define CVAR_SAVE_QUIT_PROMPT CVAR_SETTING("Save.QuitPrompt")
#define CVAR_SAVE_UNSAFE_QUIT CVAR_SETTING("Save.AllowUnsafeQuit")
#define CVAR_SAVE_OTHER_TOWN CVAR_SETTING("Save.OtherTownFile")
#define CVAR_SAVE_SELF_TEST CVAR_DEVELOPER_TOOLS("SaveSelfTest")

struct KeepData {
    alignas(32) u8 mail[mCD_KEEP_MAIL_SIZE];
    alignas(32) u8 original[mCD_KEEP_ORIGINAL_SIZE];
    alignas(32) u8 diary[mCD_KEEP_DIARY_SIZE];

    mCD_keep_mail_c& Mail() {
        return *reinterpret_cast<mCD_keep_mail_c*>(mail);
    }
    mCD_keep_original_c& Original() {
        return *reinterpret_cast<mCD_keep_original_c*>(original);
    }
    mCD_keep_diary_c& Diary() {
        return *reinterpret_cast<mCD_keep_diary_c*>(diary);
    }
    const mCD_keep_mail_c& Mail() const {
        return *reinterpret_cast<const mCD_keep_mail_c*>(mail);
    }
    const mCD_keep_original_c& Original() const {
        return *reinterpret_cast<const mCD_keep_original_c*>(original);
    }
    const mCD_keep_diary_c& Diary() const {
        return *reinterpret_cast<const mCD_keep_diary_c*>(diary);
    }

    u8* Block(u32 idx) {
        switch (idx) {
            case mCD_ARAM_DATA_MAIL:
                return mail;
            case mCD_ARAM_DATA_DIARY:
                return diary;
            default:
                return original;
        }
    }

    static u32 BlockSize(u32 idx) {
        switch (idx) {
            case mCD_ARAM_DATA_MAIL:
                return sizeof(mail);
            case mCD_ARAM_DATA_DIARY:
                return sizeof(diary);
            default:
                return sizeof(original);
        }
    }

    void Init() {
        memset(this, 0, sizeof(*this));
        mCD_InitKeepOriginal(&Original());
        mCD_InitKeepMail(&Mail());
        mCD_InitKeepDiary(&Diary());
    }
};

struct Passport {
    std::string name;
    mCD_foreigner_c data;
};

struct TownFile {
    bool hasLand = false;
    std::shared_ptr<Save> land;
    bool hasOthers = false;
    std::shared_ptr<KeepData> others;
    bool hasNoLandCode = false;
    mCD_LandProtectCode_c noLandCode{};
    std::vector<Passport> passports;
    SaveJson ship = SaveJson::object();

    std::shared_ptr<TownFile> Clone() const {
        auto copy = std::make_shared<TownFile>(*this);
        if (land) {
            copy->land = std::make_shared<Save>(*land);
        }
        if (others) {
            copy->others = std::make_shared<KeepData>(*others);
        }
        return copy;
    }

    Passport* FindPassport(const std::string& name) {
        for (auto& p : passports) {
            if (p.name == name) {
                return &p;
            }
        }
        return nullptr;
    }

    void PutPassport(const std::string& name, const mCD_foreigner_c& data) {
        if (Passport* p = FindPassport(name)) {
            p->data = data;
        } else {
            passports.push_back({ name, data });
        }
    }

    void ErasePassport(const std::string& name) {
        passports.erase(std::remove_if(passports.begin(), passports.end(),
                                       [&](const Passport& p) { return p.name == name; }),
                        passports.end());
    }
};

enum class FileState { Ok, Missing, Corrupt };

struct SlotCache {
    bool valid = false;
    std::string path;
    fs::file_time_type mtime;
    uintmax_t size = 0;
    FileState state = FileState::Missing;
    std::shared_ptr<TownFile> file;
};

struct CardPrivate {
    bool used = false;
    PersonalID_c pid;
    std::string name;
    mCD_foreigner_c data;
};

enum JobKind {
    JOB_NONE,
    JOB_SAVE_HOME,
    JOB_GAME_START,
    JOB_ERASE_PLAYER,
    JOB_ERASE_LAND,
    JOB_ERASE_BROKEN,
    JOB_ERASE_PASSPORT,
    JOB_NEXT_LAND,
    JOB_PASSPORT,
};

struct Job {
    int kind = JOB_NONE;
    int frames = 0;
    int minFrames = 0;
    int busy = mCD_TRANS_ERR_BUSY;
    u8 sfx = 0;
    int result = mCD_TRANS_ERR_NONE;
    int chan = kSlotHome;
    std::future<bool> io;
    std::function<void(bool)> done;
};

struct SaveHomeContext {
    int slot = kSlotHome;
    int landSaved = 0;
    u16 copyProtect = 0;
    std::shared_ptr<TownFile> file;
};

std::unique_ptr<KeepData> sAram;
std::mutex sRawMutex;
SlotCache sCache[mCD_SLOT_NUM];
std::string sOtherPath;
bool sPicking = false;
bool sPickerCancelled = false;

Job sJob;

std::shared_ptr<Save> sKeepSave;
bool sKeepSaveSet = false;
std::shared_ptr<KeepData> sKeepOthers;
mCD_foreigner_c sForeigner;
u16 sForeignerCopyProtect = 0xFFFF;
mCD_LandProtectCode_c sNoLandCode;
int sKeepStartCond = 0;
mCD_PrivateItem_c sPrivateItem;
int sStashed = FALSE;
CardPrivate sCardPrivateTable[kCardPrivateMax];
CardPrivate sCardPrivate;
std::string sStationPassport;

bool sSessionActive = false;
bool sLandLoaded = false;
bool sImportRequested = false;
std::string sImportRequestPath;
fs::path sSaveDirOverride;
bool sQuitApproved = false;
bool sSaveAndQuitRequested = false;
u32 sFrame = 0;
u32 sPromptDrawnFrame = 0;
u32 sPlayFrames = 0;
bool sQuitTestArmed = false;
SaveManager::QuitPrompt sQuitPrompt = SaveManager::QuitPrompt::None;

struct {
    bool read;
    bool write;
    bool corrupt;
    bool backup;
    bool raw;
} sErr;

fs::path SaveDir() {
    if (!sSaveDirOverride.empty()) {
        return sSaveDirOverride;
    }
    return fs::path(Ship::Context::GetPathRelativeToAppDirectory("save"));
}

fs::path HomePath() {
    return SaveDir() / kHomeFile;
}

fs::path SlotPath(int slot) {
    if (slot == kSlotHome) {
        return HomePath();
    }
    if (slot == kSlotOther && !sOtherPath.empty()) {
        return fs::path(sOtherPath);
    }
    return {};
}

fs::path BackupPath(const fs::path& path) {
    fs::path p = path;
    p += ".bak";
    return p;
}

std::string Timestamp() {
    std::time_t now = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", std::localtime(&now));
    return buf;
}

void SealBlock(u8* block, u32 size) {
    u16* checksum = reinterpret_cast<u16*>(block);
    *checksum = mFRm_GetFlatCheckSum(reinterpret_cast<u16*>(block), (int)size, *checksum);
}

bool BlockValid(const u8* block, u32 size) {
    return mFRm_ReturnCheckSum(reinterpret_cast<u16*>(const_cast<u8*>(block)), (int)size) == 0;
}

void SealSaveChecksum(Save* save) {
    save->save.save_check.checksum =
        mFRm_GetFlatCheckSum(reinterpret_cast<u16*>(save), sizeof(Save), save->save.save_check.checksum);
}

void SealForeigner(mCD_foreigner_c* foreigner) {
    foreigner->checksum =
        mFRm_GetFlatCheckSum(reinterpret_cast<u16*>(foreigner), sizeof(mCD_foreigner_c), foreigner->checksum);
}

std::shared_ptr<Save> NewSave() {
    auto save = std::make_shared<Save>();
    memset(save.get(), 0, sizeof(Save));
    return save;
}

std::shared_ptr<KeepData> NewKeep() {
    auto keep = std::make_shared<KeepData>();
    memset(keep.get(), 0, sizeof(KeepData));
    return keep;
}

bool LandValid(const TownFile& f) {
    return f.hasLand && f.land &&
           mFRm_CheckSaveData_common(&f.land->save.save_check, f.land->save.land_info.id);
}

bool IsThisLand(const TownFile& f) {
    return LandValid(f) && mLd_CheckThisLand(f.land->save.land_info.name, f.land->save.land_info.id);
}

bool CopyProtectMatches(const TownFile& f) {
    return f.land->save.copy_protect == (u16)Common_Get(copy_protect);
}

SaveJson ToJson(const TownFile& f) {
    SaveJson j = SaveJson::object();
    j["format"] = kFormatName;
    j["version"] = kFormatVersion;

    SaveJson ship = f.ship.is_object() ? f.ship : SaveJson::object();
    ship["game"] = "GAFE01";
    ship["layout"] = kSaveJsonLayoutHash;
    ship["savedAt"] = Timestamp();
    if (!ship.contains("features")) {
        ship["features"] = SaveJson::object();
    }
    j["shipSaveData"] = ship;

    if (f.hasLand && f.land) {
        j["land"] = f.land->save;
    } else {
        j["land"] = nullptr;
    }

    if (f.hasOthers && f.others) {
        SaveJson others = SaveJson::object();
        others["mail"] = f.others->Mail();
        others["original"] = f.others->Original();
        others["diary"] = f.others->Diary();
        j["others"] = others;
    } else {
        j["others"] = nullptr;
    }

    if (f.hasNoLandCode) {
        j["noLandCode"] = SaveJson::array();
        for (u16 c : f.noLandCode.code) {
            j["noLandCode"].push_back(c);
        }
    }

    j["passports"] = SaveJson::array();
    for (const auto& p : f.passports) {
        SaveJson e = SaveJson::object();
        e["name"] = p.name;
        e["data"] = p.data;
        j["passports"].push_back(e);
    }
    return j;
}

void FromJson(const SaveJson& j, TownFile& f) {
    if (!j.is_object() || j.value("format", std::string()) != kFormatName) {
        throw std::runtime_error("not an acport save file");
    }
    if (j.value("version", 0) > kFormatVersion) {
        throw std::runtime_error("save file is from a newer version");
    }
    if (j.contains("shipSaveData") && j["shipSaveData"].is_object()) {
        f.ship = j["shipSaveData"];
    }

    auto land = j.find("land");
    if (land != j.end() && !land->is_null()) {
        f.land = NewSave();
        from_json(*land, f.land->save);
        SealSaveChecksum(f.land.get());
        f.hasLand = true;
    }

    auto others = j.find("others");
    if (others != j.end() && others->is_object()) {
        f.others = NewKeep();
        f.others->Init();
        if (others->contains("mail")) {
            from_json((*others)["mail"], f.others->Mail());
        }
        if (others->contains("original")) {
            from_json((*others)["original"], f.others->Original());
        }
        if (others->contains("diary")) {
            from_json((*others)["diary"], f.others->Diary());
        }
        SealBlock(f.others->mail, sizeof(f.others->mail));
        SealBlock(f.others->original, sizeof(f.others->original));
        SealBlock(f.others->diary, sizeof(f.others->diary));
        f.hasOthers = true;
    }

    auto code = j.find("noLandCode");
    if (code != j.end() && code->is_array() && code->size() == 4) {
        for (int i = 0; i < 4; i++) {
            f.noLandCode.code[i] = (*code)[i].get<u16>();
        }
        f.hasNoLandCode = true;
    }

    auto passports = j.find("passports");
    if (passports != j.end() && passports->is_array()) {
        for (const auto& e : *passports) {
            Passport p;
            p.name = e.value("name", std::string());
            memset(&p.data, 0, sizeof(p.data));
            from_json(e.at("data"), p.data);
            SealForeigner(&p.data);
            f.passports.push_back(std::move(p));
        }
    }
}

bool ParseFile(const fs::path& path, TownFile& out) {
    try {
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            return false;
        }
        std::stringstream buf;
        buf << in.rdbuf();
        SaveJson j = SaveJson::parse(buf.str());
        TownFile f;
        FromJson(j, f);
        out = std::move(f);
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("save: cannot read {}: {}", path.string(), e.what());
        return false;
    }
}

bool WriteFile(const fs::path& path, const TownFile& f) {
    try {
        std::string text = ToJson(f).dump(1);
        std::error_code ec;
        fs::create_directories(path.parent_path(), ec);
        fs::path tmp = path;
        tmp += ".tmp";
        {
            std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
            out.write(text.data(), (std::streamsize)text.size());
            out.flush();
            if (!out) {
                SPDLOG_ERROR("save: cannot write {}", tmp.string());
                return false;
            }
        }
        if (fs::exists(path, ec)) {
            fs::rename(path, BackupPath(path), ec);
        }
        fs::rename(tmp, path, ec);
        if (ec) {
            SPDLOG_ERROR("save: cannot replace {}: {}", path.string(), ec.message());
            return false;
        }
        SPDLOG_INFO("save: wrote {} ({} bytes)", path.string(), text.size());
        return true;
    } catch (const std::exception& e) {
        SPDLOG_ERROR("save: cannot write {}: {}", path.string(), e.what());
        return false;
    }
}

constexpr u32 kRawHeaderSize = CARD_COMMENT_SIZE + 0xE00 + 0x600;
constexpr u32 kRawProtectOffset = kRawHeaderSize;
constexpr u32 kRawMailOffset = kRawHeaderSize + 32;
constexpr u32 kRawOriginalOffset = kRawMailOffset + mCD_KEEP_MAIL_SIZE;
constexpr u32 kRawDiaryOffset = kRawOriginalOffset + mCD_KEEP_ORIGINAL_SIZE;
constexpr u32 kRawOthersSize = mCD_ALIGN_SECTORSIZE(kRawDiaryOffset + mCD_KEEP_DIARY_SIZE);
constexpr u32 kRawLandSize = kRawOthersSize + 2 * sizeof(Save);

bool RawMirrorEnabled() {
    return sSaveDirOverride.empty() && CVarGetInteger(CVAR_SAVE_RAW_MIRROR, 1) != 0;
}

std::vector<u8> RawPack(const TownFile& f) {
    std::vector<u8> buf(kRawLandSize, 0);
    static const char comment0[] = "Animal Crossing";
    static const char comment1[] = " Town Data";
    memcpy(buf.data(), comment0, sizeof(comment0));
    memcpy(buf.data() + 32, comment1, sizeof(comment1));
    if (f.hasNoLandCode) {
        memcpy(buf.data() + kRawProtectOffset, &f.noLandCode, sizeof(f.noLandCode));
    }
    if (f.hasOthers && f.others) {
        memcpy(buf.data() + kRawMailOffset, f.others->mail, sizeof(f.others->mail));
        memcpy(buf.data() + kRawOriginalOffset, f.others->original, sizeof(f.others->original));
        memcpy(buf.data() + kRawDiaryOffset, f.others->diary, sizeof(f.others->diary));
    }
    if (f.hasLand && f.land) {
        memcpy(buf.data() + kRawOthersSize, f.land.get(), sizeof(Save));
        memcpy(buf.data() + kRawOthersSize + sizeof(Save), f.land.get(), sizeof(Save));
    }
    return buf;
}

bool RawUnpack(const std::vector<u8>& buf, TownFile& out) {
    if (buf.size() < kRawLandSize) {
        return false;
    }
    TownFile f;
    for (int copy = 0; copy < 2; copy++) {
        auto save = NewSave();
        memcpy(save.get(), buf.data() + kRawOthersSize + copy * sizeof(Save), sizeof(Save));
        if (BlockValid(reinterpret_cast<u8*>(save.get()), sizeof(Save)) &&
            mFRm_CheckSaveData_common(&save->save.save_check, save->save.land_info.id)) {
            f.land = save;
            f.hasLand = true;
            break;
        }
    }
    auto others = NewKeep();
    others->Init();
    bool any = false;
    struct {
        u32 offset;
        u8* dst;
        u32 size;
    } blocks[] = {
        { kRawMailOffset, others->mail, sizeof(others->mail) },
        { kRawOriginalOffset, others->original, sizeof(others->original) },
        { kRawDiaryOffset, others->diary, sizeof(others->diary) },
    };
    for (auto& b : blocks) {
        if (BlockValid(buf.data() + b.offset, b.size)) {
            memcpy(b.dst, buf.data() + b.offset, b.size);
            any = true;
        }
    }
    if (any) {
        f.others = others;
        f.hasOthers = true;
    }
    mCD_LandProtectCode_c code;
    memcpy(&code, buf.data() + kRawProtectOffset, sizeof(code));
    if (mCD_CheckProtectCode(&code)) {
        f.noLandCode = code;
        f.hasNoLandCode = true;
    }
    if (!f.hasLand && !f.hasOthers && !f.hasNoLandCode) {
        return false;
    }
    out = std::move(f);
    return true;
}

bool RawWrite(const TownFile& f) {
    std::vector<u8> buf = RawPack(f);
    CARDFileInfo info;
    if (CARDMount(kSlotHome, nullptr, nullptr) != CARD_RESULT_READY) {
        return false;
    }
    s32 result = CARDOpen(kSlotHome, kLandFileName, &info);
    if (result == CARD_RESULT_READY && info.length < (s32)kRawLandSize) {
        CARDClose(&info);
        CARDDelete(kSlotHome, kLandFileName);
        result = CARD_RESULT_NOFILE;
    }
    if (result == CARD_RESULT_NOFILE) {
        result = CARDCreateAsync(kSlotHome, kLandFileName, kRawLandSize, &info, nullptr);
    }
    if (result == CARD_RESULT_READY) {
        result = CARDWriteAsync(&info, buf.data(), (s32)kRawLandSize, 0, nullptr);
        CARDClose(&info);
    }
    CARDUnmount(kSlotHome);
    return result == CARD_RESULT_READY;
}

bool RawRead(TownFile& out) {
    CARDFileInfo info;
    if (CARDMount(kSlotHome, nullptr, nullptr) != CARD_RESULT_READY) {
        return false;
    }
    std::vector<u8> buf;
    s32 result = CARDOpen(kSlotHome, kLandFileName, &info);
    if (result == CARD_RESULT_READY && info.length >= (s32)kRawLandSize) {
        buf.resize(kRawLandSize);
        result = CARDRead(&info, buf.data(), (s32)kRawLandSize, 0);
        CARDClose(&info);
    } else if (result == CARD_RESULT_READY) {
        CARDClose(&info);
        result = CARD_RESULT_BROKEN;
    }
    CARDUnmount(kSlotHome);
    return result == CARD_RESULT_READY && RawUnpack(buf, out);
}

FileState ReadSlot(int slot, std::shared_ptr<TownFile>& out) {
    fs::path path = SlotPath(slot);
    if (path.empty()) {
        return FileState::Missing;
    }
    std::error_code ec;
    bool exists = fs::exists(path, ec);
    bool hasBackup = fs::exists(BackupPath(path), ec);
    auto f = std::make_shared<TownFile>();
    if (exists && ParseFile(path, *f)) {
        out = f;
        return FileState::Ok;
    }
    if (exists) {
        sErr.corrupt = true;
    }
    if (hasBackup && ParseFile(BackupPath(path), *f)) {
        SPDLOG_WARN("save: {} unreadable, using {}", path.string(), BackupPath(path).string());
        sErr.backup = true;
        out = f;
        return FileState::Ok;
    }
    bool raw = false;
    if (slot == kSlotHome && RawMirrorEnabled()) {
        std::lock_guard<std::mutex> lock(sRawMutex);
        raw = RawRead(*f);
    }
    if (raw) {
        SPDLOG_WARN("save: {} {}, using the raw-bytes mirror", path.string(), exists ? "unreadable" : "missing");
        sErr.raw = true;
        out = f;
        return FileState::Ok;
    }
    return exists ? FileState::Corrupt : FileState::Missing;
}

std::shared_ptr<TownFile> LoadSlot(int slot, FileState* state = nullptr) {
    SlotCache& c = sCache[slot];
    fs::path path = SlotPath(slot);
    std::error_code ec;
    fs::file_time_type mtime{};
    uintmax_t size = 0;
    if (!path.empty() && fs::exists(path, ec)) {
        mtime = fs::last_write_time(path, ec);
        size = fs::file_size(path, ec);
    }
    if (!c.valid || c.path != path.string() || c.mtime != mtime || c.size != size) {
        c = SlotCache{};
        c.path = path.string();
        c.mtime = mtime;
        c.size = size;
        c.state = ReadSlot(slot, c.file);
        c.valid = true;
    }
    if (state != nullptr) {
        *state = c.state;
    }
    return c.state == FileState::Ok ? c.file : nullptr;
}

void InvalidateSlots() {
    for (auto& c : sCache) {
        c.valid = false;
    }
}

std::shared_ptr<TownFile> SlotFileForWrite(int slot) {
    auto f = LoadSlot(slot);
    return f ? f->Clone() : std::make_shared<TownFile>();
}

struct PendingWrite {
    fs::path path;
    bool mirror;
    std::shared_ptr<TownFile> file;
};

PendingWrite MakeWrite(int slot, std::shared_ptr<TownFile> file) {
    return { SlotPath(slot), slot == kSlotHome && RawMirrorEnabled(), std::move(file) };
}

bool DoWrite(const PendingWrite& w) {
    if (w.path.empty()) {
        return false;
    }
    bool ok = WriteFile(w.path, *w.file);
    if (ok && w.mirror) {
        std::lock_guard<std::mutex> lock(sRawMutex);
        if (!RawWrite(*w.file)) {
            SPDLOG_WARN("save: raw-bytes mirror not written");
        }
    }
    return ok;
}

int FindThisLand(bool* otherTown = nullptr) {
    if (otherTown != nullptr) {
        *otherTown = false;
    }
    for (int slot = 0; slot < mCD_SLOT_NUM; slot++) {
        auto f = LoadSlot(slot);
        if (f == nullptr) {
            continue;
        }
        if (IsThisLand(*f)) {
            return slot;
        }
        if (otherTown != nullptr && LandValid(*f) && mLd_CheckId(f->land->save.land_info.id)) {
            *otherTown = true;
        }
    }
    return -1;
}

int FindNoLandSlot() {
    for (int slot = 0; slot < mCD_SLOT_NUM; slot++) {
        auto f = LoadSlot(slot);
        if (f != nullptr && !LandValid(*f) && f->hasNoLandCode && mCD_CompNoLandCode(&f->noLandCode, &sNoLandCode)) {
            return slot;
        }
    }
    return -1;
}

std::shared_ptr<Save> SealedCommonSave() {
    auto save = NewSave();
    memcpy(&save->save, &Common_Get(save).save, sizeof(Save_t));
    mCD_SealSave(save.get());
    return save;
}

std::shared_ptr<KeepData> OthersFromAram() {
    auto keep = NewKeep();
    if (sAram) {
        *keep = *sAram;
    } else {
        keep->Init();
    }
    keep->Mail().landid = Save_Get(land_info).id;
    SealBlock(keep->mail, sizeof(keep->mail));
    keep->Original().landid = Save_Get(land_info).id;
    SealBlock(keep->original, sizeof(keep->original));
    SealBlock(keep->diary, sizeof(keep->diary));
    return keep;
}

void OthersToAram(const KeepData& keep, bool diary) {
    if (!sAram) {
        return;
    }
    static const u32 blocks[] = { mCD_ARAM_DATA_MAIL, mCD_ARAM_DATA_ORIGINAL, mCD_ARAM_DATA_DIARY };
    for (u32 idx : blocks) {
        if (idx == mCD_ARAM_DATA_DIARY && !diary) {
            continue;
        }
        const u8* src = const_cast<KeepData&>(keep).Block(idx);
        if (BlockValid(src, KeepData::BlockSize(idx))) {
            memcpy(sAram->Block(idx), src, KeepData::BlockSize(idx));
        }
    }
}

std::string NextPassportName(const TownFile* f) {
    for (int n = 0; n < kPassportMax; n++) {
        std::string name = kPassportPrefix + std::to_string(n);
        if (f == nullptr || const_cast<TownFile*>(f)->FindPassport(name) == nullptr) {
            return name;
        }
    }
    return std::string(kPassportPrefix) + "0";
}

void ClearCardPrivateTable() {
    for (auto& c : sCardPrivateTable) {
        c = CardPrivate{};
    }
}

void ClearNpcLists() {
    memset(Common_Get(npclist), 0, sizeof(Common_Get(npclist)));
    memset(Common_Get(island_npclist), 0, sizeof(Common_Get(island_npclist)));
    memset(Common_Get(npc_schedule), 0, sizeof(Common_Get(npc_schedule)));
}

void SaveFamicom() {
    if (sSaveDirOverride.empty() && mLd_PlayerManKindCheck() == FALSE) {
        famicom_internal_data_save();
    }
}

void LoadFamicom() {
    if (sSaveDirOverride.empty() && mLd_PlayerManKindCheck() == FALSE) {
        famicom_internal_data_load();
    }
}

std::future<bool> WriteAsync(std::vector<PendingWrite> writes, size_t required = 1) {
    return std::async(std::launch::async, [writes = std::move(writes), required]() {
        for (size_t i = 0; i < writes.size(); i++) {
            if (!DoWrite(writes[i]) && i < required) {
                return false;
            }
        }
        return true;
    });
}

void ResetJob() {
    if (sJob.io.valid()) {
        sJob.io.wait();
    }
    if (sJob.sfx != 0) {
        sAdo_SysLevStop(sJob.sfx);
    }
    sJob = Job{};
}

int RunJob(int kind, s32* chan, const std::function<void(Job&)>& start) {
    if (sJob.kind != kind) {
        ResetJob();
        sJob.kind = kind;
        start(sJob);
        if (sJob.sfx != 0) {
            sAdo_SysLevStart(sJob.sfx);
        }
    }
    sJob.frames++;
    if (sJob.io.valid()) {
        if (sJob.io.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            return sJob.busy;
        }
        bool ok = sJob.io.get();
        InvalidateSlots();
        if (!ok) {
            sErr.write = true;
        }
        if (sJob.done) {
            sJob.done(ok);
        }
    }
    if (sJob.frames < sJob.minFrames) {
        return sJob.busy;
    }
    if (sJob.sfx != 0) {
        sAdo_SysLevStop(sJob.sfx);
    }
    int result = sJob.result;
    if (chan != nullptr) {
        *chan = sJob.chan;
    }
    sJob = Job{};
    return result;
}

int SaveHome_ChangeErrCode(int err_code) {
    switch (err_code) {
        case mCD_TRANS_ERR_TOWN_INVALID:
        case mCD_TRANS_ERR_16:
        case mCD_TRANS_ERR_GENERIC:
            return mCD_TRANS_ERR_NONE;
        case mCD_TRANS_ERR_OTHER_TOWN:
            return mCD_TRANS_ERR_NOCARD;
        default:
            return err_code;
    }
}

int GameStart_ChangeErrCode(int err_code) {
    switch (err_code) {
        case mCD_TRANS_ERR_BROKEN_WRONGENCODING:
        case mCD_TRANS_ERR_REPAIR:
        case mCD_TRANS_ERR_TOWN_INVALID:
        case mCD_TRANS_ERR_16:
        case mCD_TRANS_ERR_GENERIC:
            return mCD_TRANS_ERR_IOERROR;
        case mCD_TRANS_ERR_NO_SPACE:
        case mCD_TRANS_ERR_OTHER_TOWN:
            return mCD_TRANS_ERR_NOCARD;
        default:
            return err_code;
    }
}

int EraseLand_ChangeErrCode(int err_code) {
    return GameStart_ChangeErrCode(err_code);
}

int SelectSaveHomeSlot(int* slot) {
    *slot = kSlotHome;
    if (Save_Get(save_exist) == FALSE) {
        if (mCD_CheckInitProtectCode(&sNoLandCode) == FALSE && mCD_CheckProtectCode(&sNoLandCode) == TRUE) {
            int s = FindNoLandSlot();
            if (s < 0) {
                return mCD_TRANS_ERR_INVALID_NOLAND_CODE;
            }
            *slot = s;
            return mCD_TRANS_ERR_NONE;
        }
        FileState state;
        auto f = LoadSlot(kSlotHome, &state);
        if (state == FileState::Corrupt) {
            std::error_code ec;
            fs::path aside = HomePath();
            aside += ".broken";
            fs::rename(HomePath(), aside, ec);
            SPDLOG_WARN("save: moved unreadable {} to {}", HomePath().string(), aside.string());
            InvalidateSlots();
            return mCD_TRANS_ERR_NONE;
        }
        if (f != nullptr && LandValid(*f)) {
            return mCD_TRANS_ERR_LAND_EXIST;
        }
        if (f != nullptr && !f->passports.empty()) {
            return mCD_TRANS_ERR_PASSPORT_EXIST;
        }
        return mCD_TRANS_ERR_NONE;
    }

    int s = FindThisLand();
    if (s < 0) {
        return mCD_TRANS_ERR_NOCARD;
    }
    *slot = s;
    if (!CopyProtectMatches(*LoadSlot(s))) {
        return mCD_TRANS_ERR_WRONG_LAND;
    }
    return mCD_TRANS_ERR_NONE;
}

int PrepareSaveHome(int mode, SaveHomeContext& ctx) {
    int err = SelectSaveHomeSlot(&ctx.slot);
    if (err != mCD_TRANS_ERR_NONE) {
        return err;
    }
    ctx.landSaved = Save_Get(save_exist);
    ctx.copyProtect = (u16)Common_Get(copy_protect);
    SaveGifts::Deliver();
    mCD_SaveHome_SetData(mode);
    ctx.file = SlotFileForWrite(ctx.slot);
    ctx.file->land = SealedCommonSave();
    ctx.file->hasLand = true;
    ctx.file->others = OthersFromAram();
    ctx.file->hasOthers = true;
    ctx.file->hasNoLandCode = false;
    return mCD_TRANS_ERR_NONE;
}

int FinishSaveHome(int mode, const SaveHomeContext& ctx, bool ok) {
    if (!ok) {
        Save_Set(save_exist, ctx.landSaved);
        Common_Set(copy_protect, ctx.copyProtect);
        return mCD_TRANS_ERR_IOERROR;
    }
    mCD_SaveHome_Written(mode);
    memset(&sNoLandCode, 0, sizeof(sNoLandCode));
    if (mode == 0) {
        sSessionActive = false;
    }
    CALL_EVENT(SaveWritten, mode);
    return mCD_TRANS_ERR_NONE;
}

int EvaluateOtherSlot(int other) {
    FileState state;
    auto f = LoadSlot(other, &state);
    bool foreigner = mLd_PlayerManKindCheck() != FALSE;

    if (state == FileState::Corrupt) {
        return mCD_TRANS_ERR_CORRUPT;
    }
    if (f != nullptr && f->hasLand) {
        if (!LandValid(*f) || !mLd_CheckId(f->land->save.land_info.id)) {
            return mCD_TRANS_ERR_CORRUPT;
        }
        if (mLd_CheckThisLand(f->land->save.land_info.name, f->land->save.land_info.id)) {
            return mCD_TRANS_ERR_NO_SPACE;
        }
        if (sKeepStartCond == mCD_START_COND_INCOMING_FOREIGNER && foreigner) {
            return mCD_TRANS_ERR_TRAVEL_DATA_MISSING;
        }
        return mCD_TRANS_ERR_NONE_NEXTLAND;
    }

    if (foreigner && sKeepStartCond != mCD_START_COND_OUTGOING_FOREIGNER) {
        Passport* p = f != nullptr ? f->FindPassport(sCardPrivate.name) : nullptr;
        if (p != nullptr && p->data.copy_protect == sForeignerCopyProtect) {
            sStationPassport = p->name;
            return mCD_TRANS_ERR_NONE;
        }
        return mCD_TRANS_ERR_TRAVEL_DATA_MISSING;
    }

    if (f != nullptr && Now_Private != NULL) {
        for (auto& p : f->passports) {
            if (mPr_CheckCmpPersonalID(&Now_Private->player_ID, &p.data.priv.player_ID) == TRUE) {
                sStationPassport = p.name;
                return mCD_TRANS_ERR_TRAVEL_DATA_EXISTS;
            }
        }
    }
    sStationPassport.clear();
    return mCD_TRANS_ERR_NONE;
}

std::vector<SaveManager::PickerEntry> ScanTownFiles(const fs::path& dir, const fs::path& home) {
    std::vector<SaveManager::PickerEntry> out;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) {
        return out;
    }
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        if (!e.is_regular_file(ec) || e.path().extension() != ".json") {
            continue;
        }
        if (fs::equivalent(e.path(), home, ec)) {
            continue;
        }
        out.push_back({ e.path().string(), e.path().filename().string() });
    }
    std::sort(out.begin(), out.end(),
              [](const SaveManager::PickerEntry& a, const SaveManager::PickerEntry& b) { return a.label < b.label; });
    return out;
}

bool GameInPlay() {
    return gamePT != nullptr && gamePT->exec == play_main;
}

struct GcCard {
    SaveGci::Town town;
    std::vector<Passport> passports;
    std::vector<u8> famicom;
    std::vector<SaveGci::CardFile> nesGames;
    std::vector<std::pair<std::string, std::vector<u8>>> nesSaves;
    std::vector<SaveGci::CardFile> gifts;
};

bool ReadGcCard(const std::string& path, GcCard& card, std::string* error) {
    std::vector<SaveGci::CardFile> files;
    if (!SaveGci::ReadContainer(path, files, error)) {
        return false;
    }
    const SaveGci::CardFile* town = SaveGci::FindFile(files, SaveGci::kTownFileName);
    if (town == nullptr) {
        auto other = std::find_if(files.begin(), files.end(), [](const SaveGci::CardFile& f) {
            return f.name == SaveGci::kTownFileName && f.gameCode != SaveGci::kGameCode;
        });
        if (other != files.end()) {
            *error = "This town is from another release of the game (" + other->gameCode +
                     "). Only the North American release (GAFE) is supported.";
        } else {
            *error = "No Animal Crossing town (" + std::string(SaveGci::kTownFileName) + ") in this file.";
        }
        return false;
    }
    if (!SaveGci::ReadTown(*town, card.town, error)) {
        return false;
    }
    for (const auto& f : files) {
        if (f.gameCode != SaveGci::kGameCode || f.name.rfind(SaveGci::kPassportFilePrefix, 0) != 0) {
            continue;
        }
        Passport p;
        p.name = f.name;
        if (SaveGci::ReadPassport(f, p.data)) {
            card.passports.push_back(std::move(p));
        } else {
            SPDLOG_WARN("save import: passport {} is damaged, skipped", f.name);
        }
    }
    if (const SaveGci::CardFile* nes = SaveGci::FindFile(files, SaveGci::kFamicomSaveFileName)) {
        if (!SaveGci::ReadFamicomSave(*nes, card.famicom)) {
            SPDLOG_WARN("save import: NES save {} is damaged, skipped", nes->name);
        }
    }
    for (const auto& f : files) {
        if (f.gameCode != SaveGci::kGameCode) {
            continue;
        }
        if (f.name.rfind(SaveGci::kFamicomGamePrefix, 0) == 0 && f.name != SaveGci::kFamicomSaveFileName) {
            card.nesGames.push_back(f);
        } else if (f.name.rfind(SaveGci::kFamicomGameSavePrefix, 0) == 0) {
            std::vector<u8> save;
            if (SaveGci::ReadFamicomSave(f, save, "GAFEFSE\x1A")) {
                card.nesSaves.emplace_back(f.name, std::move(save));
            }
        } else if (f.name.rfind(SaveGci::kGiftFilePrefix, 0) == 0) {
            card.gifts.push_back(f);
        }
    }
    return true;
}

std::string FileStamp() {
    std::time_t now = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y%m%d-%H%M%S", std::localtime(&now));
    return buf;
}

std::string SafeFileName(const std::string& name) {
    std::string out;
    for (char c : name) {
        out += (std::isalnum((unsigned char)c) || c == ' ' || c == '-' || c == '_') ? c : '_';
    }
    return out.empty() ? std::string("town") : out;
}

bool BackupBeforeImport(fs::path* dir) {
    std::error_code ec;
    fs::path target = SaveDir() / "backup" / ("before-import-" + FileStamp());
    const fs::path items[] = { HomePath(), BackupPath(HomePath()), SaveDir() / "famicom.bin", SaveDir() / "card_a" };
    bool any = false;
    for (const auto& item : items) {
        if (!fs::exists(item, ec)) {
            continue;
        }
        fs::create_directories(target, ec);
        fs::copy(item, target / item.filename(), fs::copy_options::recursive | fs::copy_options::overwrite_existing,
                 ec);
        if (ec) {
            SPDLOG_ERROR("save import: cannot back up {}: {}", item.string(), ec.message());
            return false;
        }
        any = true;
    }
    *dir = any ? target : fs::path();
    return true;
}

bool WriteBinary(const fs::path& path, const std::vector<u8>& data) {
    std::error_code ec;
    fs::create_directories(path.parent_path(), ec);
    fs::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(data.data()), (std::streamsize)data.size());
        if (!out) {
            return false;
        }
    }
    fs::rename(tmp, path, ec);
    return !ec;
}

void CloseWindow() {
    if (auto window = ShipCompat::GetWindow()) {
        window->Close();
    }
}

} // namespace

extern "C" {

void mCD_init_card(void) {
    std::error_code ec;
    fs::create_directories(SaveDir() / kTownDir, ec);
    sOtherPath = CVarGetString(CVAR_SAVE_OTHER_TOWN, "");
    if (!sOtherPath.empty() && fs::equivalent(fs::path(sOtherPath), HomePath(), ec)) {
        sOtherPath.clear();
    }
    CARDInit();
    InvalidateSlots();
}

void mCD_InitAll(void) {
    ResetJob();
    sLandLoaded = false;
    sKeepSave.reset();
    sKeepSaveSet = false;
    ClearCardPrivateTable();
    sCardPrivate = CardPrivate{};
    sForeignerCopyProtect = 0xFFFF;
    mCD_ClearForeignerFile(&sForeigner);
    memset(&sNoLandCode, 0, sizeof(sNoLandCode));
    memset(&sErr, 0, sizeof(sErr));
}

void mCD_save_data_aram_malloc(void) {
    if (!sAram) {
        sAram = std::make_unique<KeepData>();
    }
    memset(sAram.get(), 0, sizeof(KeepData));
}

int mCD_save_data_aram_to_main(void* dst, u32 size, u32 idx) {
    if (idx >= mCD_ARAM_DATA_NUM) {
        idx = 0;
    }
    if (!sAram) {
        return FALSE;
    }
    memcpy(dst, sAram->Block(idx), std::min<u32>(size, KeepData::BlockSize(idx)));
    return TRUE;
}

int mCD_save_data_main_to_aram(void* src, u32 size, u32 idx) {
    if (idx >= mCD_ARAM_DATA_NUM) {
        idx = 0;
    }
    if (!sAram) {
        return FALSE;
    }
    memcpy(sAram->Block(idx), src, std::min<u32>(size, KeepData::BlockSize(idx)));
    return TRUE;
}

void mCD_set_aram_save_data(void) {
    if (sAram) {
        sAram->Init();
    }
}

void mCD_PrintErrInfo(gfxprint_t* gfxprint) {
    gfxprint_color(gfxprint, 250, 100, 250, 255);
    gfxprint_locate8x8(gfxprint, 22, 3);
    if (sErr.write) {
        gfxprint_printf(gfxprint, "W");
    }
    if (sErr.read) {
        gfxprint_printf(gfxprint, "R");
    }
    if (sErr.corrupt) {
        gfxprint_printf(gfxprint, "C");
    }
    if (sErr.backup) {
        gfxprint_printf(gfxprint, "O");
    }
    if (sErr.raw) {
        gfxprint_printf(gfxprint, "B");
    }
}

void mCD_LoadLand(void) {
    FileState state;
    auto f = LoadSlot(kSlotHome, &state);
    int cond = mFRm_ERROR_BAD_RTC;
    bool hasTown = false;

    sSessionActive = false;
    sLandLoaded = true;
    if (f != nullptr) {
        int version = f->land ? f->land->save.save_check.version : 0;
        if (LandValid(*f) && (version == 6 || version == 5)) {
            memcpy(Common_GetPointer(save), f->land.get(), sizeof(Save));
            Common_Set(copy_protect, Save_Get(copy_protect));
            hasTown = true;
            if (f->hasOthers) {
                OthersToAram(*f->others, true);
            }
        } else if (f->hasNoLandCode && mCD_CheckProtectCode(&f->noLandCode)) {
            sNoLandCode = f->noLandCode;
            if (f->hasOthers) {
                OthersToAram(*f->others, false);
            }
        } else if (f->hasLand) {
            cond = mFRm_ERROR_DATA_CORRUPTED;
        }
    } else if (state == FileState::Corrupt) {
        sErr.read = true;
        cond = mFRm_ERROR_DATA_CORRUPTED;
    }

    Common_Set(save_error_type, cond);
    Common_Set(memcard_slot, kSlotHome);

    if (mFRm_CheckSaveData_common(Save_GetPointer(save_check), Save_Get(land_info).id) &&
        Save_Get(save_check).version == 5) {
        memcpy(Save_GetPointer(saved_auto_nwrite_time), &Save_Get(save_check).time, sizeof(lbRTC_time_c));
    }
    CALL_EVENT(SaveLoaded, hasTown);
}

void mCD_ReCheckLoadLand(GAME_PLAY* play) {
    int scene = Save_Get(scene_no);

    mCD_LoadLand();
    Save_Set(scene_no, scene);

    if (Common_Get(save_error_type) != mFRm_ERROR_BAD_RTC) {
        scene = SCENE_PLAYERSELECT_3;
        Common_Set(house_owner_name, RSV_NO);
        Common_Set(last_field_id, RSV_NO);
    } else if (mFRm_CheckSaveData() == FALSE) {
        scene = SCENE_PLAYERSELECT;
        Common_Set(house_owner_name, RSV_NO);
        Common_Set(last_field_id, RSV_NO);
    } else {
        int rtc_on = Common_Get(time.rtc_enabled);
        Common_Set(time.rtc_enabled, TRUE);
        mTM_rtcTime_limit_check();
        scene = SCENE_PLAYERSELECT_2;
        Common_Set(time.rtc_enabled, rtc_on);
        mEv_ClearEventInfo();
    }

    play->next_scene_no = scene;
}

int mCD_GetThisLandSlotNo(void) {
    return FindThisLand();
}

int mCD_GetThisLandSlotNo_code(int* player_no, s32* slot_card_results) {
    int ret = -1;

    if (player_no != NULL) {
        *player_no = -1;
    }
    if (slot_card_results != NULL) {
        slot_card_results[kSlotHome] = CARD_RESULT_NOCARD;
        slot_card_results[kSlotOther] = CARD_RESULT_NOCARD;
    }
    if (Now_Private == NULL) {
        return -1;
    }

    mLd_land_info_c* info = Save_GetPointer(land_info);
    for (int slot = 0; slot < mCD_SLOT_NUM; slot++) {
        FileState state;
        auto f = LoadSlot(slot, &state);
        if (slot_card_results != NULL) {
            slot_card_results[slot] = state == FileState::Ok        ? CARD_RESULT_READY
                                      : state == FileState::Corrupt ? CARD_RESULT_BROKEN
                                      : SlotPath(slot).empty()      ? CARD_RESULT_NOCARD
                                                                    : CARD_RESULT_NOFILE;
        }
        if (f == nullptr || !LandValid(*f)) {
            continue;
        }
        Save_t& save = f->land->save;
        if (mLd_CheckCmpLand(save.land_info.name, save.land_info.id, info->name, info->id) == TRUE) {
            if (player_no != NULL) {
                for (int k = 0; k < PLAYER_NUM; k++) {
                    if (mPr_CheckCmpPersonalID(&Now_Private->player_ID, &save.private_data[k].player_ID) == TRUE) {
                        *player_no = k;
                        break;
                    }
                }
            }
            ret = slot;
            break;
        }
    }
    return ret;
}

int mCD_GetSaveHomeSlotNo(void) {
    if (Save_Get(save_exist) != FALSE) {
        return mCD_GetThisLandSlotNo();
    }
    if (mCD_CheckInitProtectCode(&sNoLandCode) == FALSE && mCD_CheckProtectCode(&sNoLandCode) == TRUE) {
        return FindNoLandSlot();
    }
    FileState state;
    auto f = LoadSlot(kSlotHome, &state);
    if (f != nullptr && (LandValid(*f) || !f->passports.empty())) {
        return -1;
    }
    return kSlotHome;
}

int mCD_card_format_bg(s32 chan) {
    return mCD_RESULT_SUCCESS;
}

int mCD_SaveHome_bg(int param_1, int* chan) {
    int mode = param_1;
    int ret = RunJob(JOB_SAVE_HOME, chan, [&](Job& job) {
        auto ctx = std::make_shared<SaveHomeContext>();
        job.minFrames = kSaveFrames;
        job.sfx = NA_SE_47;
        SoftResetEnable = FALSE;
        job.result = PrepareSaveHome(mode, *ctx);
        job.chan = ctx->slot;
        if (job.result != mCD_TRANS_ERR_NONE) {
            return;
        }
        job.io = WriteAsync({ MakeWrite(ctx->slot, ctx->file) });
        job.done = [mode, ctx](bool ok) { sJob.result = FinishSaveHome(mode, *ctx, ok); };
    });

    if (ret == mCD_TRANS_ERR_BUSY) {
        return ret;
    }
    if (ret == mCD_TRANS_ERR_NONE) {
        SaveFamicom();
    }
    ret = SaveHome_ChangeErrCode(ret);
    SoftResetEnable = TRUE;
    return ret;
}

int mCD_InitGameStart_bg(int player_no, int card_private_idx, int start_cond, s32* mounted_chan) {
    if (start_cond < 0 || start_cond >= mCD_START_COND_NUM) {
        start_cond = mCD_START_COND_0;
    }
    bool quiet = start_cond == mCD_START_COND_0;

    int ret = RunJob(JOB_GAME_START, mounted_chan, [&](Job& job) {
        job.minFrames = quiet ? 0 : kSaveFrames;
        job.sfx = quiet ? 0 : NA_SE_47;
        SoftResetEnable = FALSE;

        int slot = kSlotHome;
        if (start_cond == mCD_START_COND_1 || start_cond == mCD_START_COND_INCOMING_FOREIGNER ||
            start_cond == mCD_START_COND_OUTGOING_FOREIGNER) {
            bool otherTown = false;
            slot = FindThisLand(&otherTown);
            if (slot < 0) {
                job.result = otherTown ? mCD_TRANS_ERR_WRONG_LAND : mCD_TRANS_ERR_NOCARD;
                ClearNpcLists();
                return;
            }
            if (slot == kSlotOther && !CopyProtectMatches(*LoadSlot(slot))) {
                job.chan = slot;
                job.result = mCD_TRANS_ERR_WRONG_LAND;
                ClearNpcLists();
                return;
            }
        }
        job.chan = slot;

        const mCD_foreigner_c* cardPassport = nullptr;
        CardPrivate card;
        if (start_cond == mCD_START_COND_INCOMING_FOREIGNER && card_private_idx >= 0 &&
            card_private_idx < kCardPrivateMax && sCardPrivateTable[card_private_idx].used) {
            card = sCardPrivateTable[card_private_idx];
            cardPassport = &card.data;
        }

        u16 copyProtect = (u16)Common_Get(copy_protect);
        int passportMode = 0;
        if (!mCD_GameStart_MakeData(player_no, start_cond, &sForeigner, cardPassport, &passportMode)) {
            job.result = mCD_TRANS_ERR_IOERROR;
            ClearNpcLists();
            return;
        }
        sKeepStartCond = start_cond;
        if (cardPassport != nullptr) {
            sCardPrivate = card;
        }
        if ((start_cond == mCD_START_COND_INCOMING_FOREIGNER || start_cond == mCD_START_COND_OUTGOING_FOREIGNER) &&
            mLd_PlayerManKindCheckNo((u8)player_no) == TRUE) {
            sForeignerCopyProtect = sForeigner.copy_protect;
        }

        if (start_cond == mCD_START_COND_0 || start_cond == mCD_START_COND_2) {
            job.chan = kSlotHome;
            job.result = mCD_TRANS_ERR_NONE;
            sSessionActive = true;
            return;
        }

        auto file = SlotFileForWrite(slot);
        file->land = SealedCommonSave();
        file->hasLand = true;
        std::vector<PendingWrite> writes = { MakeWrite(slot, file) };
        int other = slot ^ 1;
        if (passportMode != 0 && !SlotPath(other).empty()) {
            auto otherFile = SlotFileForWrite(other);
            if (passportMode == 1) {
                otherFile->ErasePassport(sCardPrivate.name);
            } else {
                otherFile->PutPassport(sCardPrivate.name, sForeigner);
            }
            writes.push_back(MakeWrite(other, otherFile));
        }
        job.io = WriteAsync(std::move(writes));
        job.done = [slot, copyProtect](bool ok) {
            sJob.chan = slot;
            if (ok) {
                mCD_GameStart_SetDecoy();
                sJob.result = mCD_TRANS_ERR_NONE;
                sSessionActive = true;
            } else {
                Common_Set(copy_protect, copyProtect);
                ClearNpcLists();
                sJob.result = mCD_TRANS_ERR_IOERROR;
            }
        };
    });

    if (ret == mCD_TRANS_ERR_BUSY) {
        return ret;
    }
    if (quiet || ret == mCD_TRANS_ERR_NONE) {
        LoadFamicom();
    }
    ret = GameStart_ChangeErrCode(ret);
    SoftResetEnable = TRUE;
    return ret;
}

int mCD_SaveErasePlayer_bg(int* mounted_chan) {
    int ret = RunJob(JOB_ERASE_PLAYER, mounted_chan, [&](Job& job) {
        job.minFrames = kSaveFrames;
        job.sfx = NA_SE_53;
        int slot = Save_Get(save_exist) != FALSE ? FindThisLand() : -1;
        if (slot < 0) {
            job.result = mCD_TRANS_ERR_NOCARD;
            return;
        }
        job.chan = slot;
        auto file = SlotFileForWrite(slot);
        file->land = SealedCommonSave();
        file->hasLand = true;
        job.io = WriteAsync({ MakeWrite(slot, file) });
        job.done = [](bool ok) { sJob.result = ok ? mCD_TRANS_ERR_NONE : mCD_TRANS_ERR_IOERROR; };
    });
    return ret == mCD_TRANS_ERR_BUSY ? ret : GameStart_ChangeErrCode(ret);
}

int mCD_EraseLand_bg(int* slot_out) {
    int ret = RunJob(JOB_ERASE_LAND, slot_out, [&](Job& job) {
        job.minFrames = kEraseFrames;
        job.sfx = NA_SE_53;
        int slot = Save_Get(save_exist) != FALSE ? FindThisLand() : -1;
        if (slot < 0) {
            job.result = mCD_TRANS_ERR_NOCARD;
            return;
        }
        job.chan = slot;
        auto file = SlotFileForWrite(slot);
        file->land.reset();
        file->hasLand = false;
        mCD_MakeProtectCode(&file->noLandCode);
        file->hasNoLandCode = true;
        job.io = WriteAsync({ MakeWrite(slot, file) });
        job.done = [](bool ok) { sJob.result = ok ? mCD_TRANS_ERR_NONE : mCD_TRANS_ERR_IOERROR; };
    });
    return ret == mCD_TRANS_ERR_BUSY ? ret : EraseLand_ChangeErrCode(ret);
}

int mCD_EraseBrokenLand_bg(int* slot_out) {
    int ret = RunJob(JOB_ERASE_BROKEN, slot_out, [&](Job& job) {
        job.minFrames = kEraseFrames;
        job.sfx = NA_SE_53;
        job.chan = kSlotHome;
        FileState state;
        auto f = LoadSlot(kSlotHome, &state);
        bool broken = state == FileState::Corrupt || (f != nullptr && f->hasLand && !LandValid(*f) && !f->hasNoLandCode);
        if (!broken) {
            job.result = mCD_TRANS_ERR_NONE;
            return;
        }
        std::shared_ptr<TownFile> file;
        if (state == FileState::Corrupt) {
            std::error_code ec;
            fs::path aside = HomePath();
            aside += ".broken";
            fs::rename(HomePath(), aside, ec);
            SPDLOG_WARN("save: moved unreadable {} to {}", HomePath().string(), aside.string());
            file = std::make_shared<TownFile>();
        } else {
            file = f->Clone();
        }
        file->land.reset();
        file->hasLand = false;
        mCD_MakeProtectCode(&file->noLandCode);
        file->hasNoLandCode = true;
        job.io = WriteAsync({ MakeWrite(kSlotHome, file) });
        job.done = [](bool ok) { sJob.result = ok ? mCD_TRANS_ERR_NONE : mCD_TRANS_ERR_IOERROR; };
    });
    return ret == mCD_TRANS_ERR_BUSY ? ret : EraseLand_ChangeErrCode(ret);
}

int mCD_CheckPassportFile(void) {
    for (int slot = 0; slot < mCD_SLOT_NUM; slot++) {
        auto f = LoadSlot(slot);
        if (f != nullptr && !f->passports.empty()) {
            return slot;
        }
    }
    return -1;
}

int mCD_CheckBrokenPassportFile(int slot) {
    if (slot != kSlotHome && slot != kSlotOther) {
        return FALSE;
    }
    auto f = LoadSlot(slot);
    if (f == nullptr) {
        return FALSE;
    }
    for (auto& p : f->passports) {
        if (mPr_NullCheckPersonalID(&p.data.priv.player_ID) == TRUE) {
            return TRUE;
        }
    }
    return FALSE;
}

int mCD_ErasePassportFile_bg(int slot) {
    return RunJob(JOB_ERASE_PASSPORT, nullptr, [&](Job& job) {
        job.minFrames = kEraseFrames;
        job.sfx = NA_SE_53;
        job.busy = mCD_RESULT_BUSY;
        FileState state;
        auto f = (slot == kSlotHome || slot == kSlotOther) ? LoadSlot(slot, &state) : nullptr;
        if (f == nullptr) {
            job.result = mCD_RESULT_ERROR;
            return;
        }
        auto file = f->Clone();
        file->passports.erase(std::remove_if(file->passports.begin(), file->passports.end(),
                                             [](Passport& p) {
                                                 return mPr_NullCheckPersonalID(&p.data.priv.player_ID) == TRUE;
                                             }),
                              file->passports.end());
        job.io = WriteAsync({ MakeWrite(slot, file) });
        job.done = [](bool ok) { sJob.result = ok ? mCD_RESULT_SUCCESS : mCD_RESULT_ERROR; };
    });
}

int mCD_GetPlayerNum(void) {
    int slot = FindThisLand();
    if (slot < 0) {
        return -1;
    }
    ClearCardPrivateTable();
    auto f = LoadSlot(slot ^ 1);
    int count = 0;
    if (f != nullptr) {
        for (auto& p : f->passports) {
            if (count >= kCardPrivateMax) {
                break;
            }
            if (!BlockValid(reinterpret_cast<u8*>(&p.data), sizeof(p.data)) ||
                mPr_NullCheckPersonalID(&p.data.priv.player_ID)) {
                continue;
            }
            CardPrivate& c = sCardPrivateTable[count++];
            c.used = true;
            mPr_CopyPersonalID(&c.pid, &p.data.priv.player_ID);
            c.name = p.name;
            c.data = p.data;
        }
    }
    return count;
}

int mCD_GetCardPrivateNameCopy(u8* dst, int idx) {
    if (dst == NULL || idx < 0 || idx >= kCardPrivateMax) {
        return FALSE;
    }
    memcpy(dst, sCardPrivateTable[idx].pid.player_name, sizeof(sCardPrivateTable[idx].pid.player_name));
    return TRUE;
}

int mCD_CheckCardPlayerNative(int idx) {
    if (idx < 0 || idx >= kCardPrivateMax) {
        return -1;
    }
    CardPrivate& c = sCardPrivateTable[idx];
    if (mPr_NullCheckPersonalID(&c.pid)) {
        return -1;
    }
    Private_c* priv = Save_Get(private_data);
    for (int i = 0; i < PLAYER_NUM; i++) {
        if (mPr_CheckCmpPersonalID(&c.pid, &priv[i].player_ID) == TRUE) {
            return i;
        }
    }
    return mPr_FOREIGNER;
}

int mCD_CheckStation_bg(s32* chan) {
    if (sPicking) {
        return mCD_TRANS_ERR_BUSY;
    }
    int slot = FindThisLand();
    if (slot < 0) {
        *chan = kSlotHome;
        return mCD_TRANS_ERR_NOCARD;
    }
    if (!CopyProtectMatches(*LoadSlot(slot))) {
        *chan = slot;
        return mCD_TRANS_ERR_WRONG_LAND;
    }
    int other = slot ^ 1;
    *chan = other;
    if (SlotPath(other).empty()) {
        if (sPickerCancelled) {
            sPickerCancelled = false;
            return mCD_TRANS_ERR_NOCARD;
        }
        SaveManager_PickTownFile();
        return mCD_TRANS_ERR_BUSY;
    }
    sPickerCancelled = false;
    return EvaluateOtherSlot(other);
}

int mCD_SaveStation_NextLand_bg(s32* chan) {
    int ret = RunJob(JOB_NEXT_LAND, chan, [&](Job& job) {
        job.minFrames = kSaveFrames;
        job.sfx = NA_SE_47;
        int slot = FindThisLand();
        if (slot < 0) {
            job.result = mCD_TRANS_ERR_NOCARD;
            return;
        }
        job.chan = slot;
        if (!CopyProtectMatches(*LoadSlot(slot))) {
            job.result = mCD_TRANS_ERR_WRONG_LAND;
            return;
        }
        SoftResetEnable = FALSE;

        int other = slot ^ 1;
        FileState state;
        auto next = LoadSlot(other, &state);
        if (state == FileState::Corrupt) {
            job.chan = other;
            job.result = mCD_TRANS_ERR_CORRUPT;
            return;
        }
        if (next == nullptr || !next->hasLand) {
            job.chan = other;
            job.result = sKeepStartCond == mCD_START_COND_INCOMING_FOREIGNER ? mCD_TRANS_ERR_TRAVEL_DATA_MISSING
                                                                            : mCD_TRANS_ERR_NO_TOWN_DATA;
            return;
        }
        Save_t& nextSave = next->land->save;
        if (!LandValid(*next) || mLd_CheckId(nextSave.land_info.id) != TRUE) {
            job.chan = other;
            job.result = mCD_TRANS_ERR_CORRUPT;
            return;
        }
        if (mLd_CheckThisLand(nextSave.land_info.name, nextSave.land_info.id) == TRUE) {
            job.chan = other;
            job.result = mCD_TRANS_ERR_NO_TOWN_DATA;
            return;
        }
        if (sKeepStartCond == mCD_START_COND_INCOMING_FOREIGNER && mLd_PlayerManKindCheck()) {
            job.chan = other;
            job.result = mCD_TRANS_ERR_TRAVEL_DATA_MISSING;
            return;
        }

        sKeepSave = std::make_shared<Save>(*next->land);
        sKeepSaveSet = true;
        sKeepOthers = NewKeep();
        sKeepOthers->Init();
        if (next->hasOthers) {
            *sKeepOthers = *next->others;
        }

        u16 copyProtect = (u16)Common_Get(copy_protect);
        mCD_Travel_SetData(&sForeigner, &sPrivateItem, &sStashed, TRUE);
        auto file = SlotFileForWrite(slot);
        file->land = SealedCommonSave();
        file->hasLand = true;
        file->others = OthersFromAram();
        file->hasOthers = true;
        job.io = WriteAsync({ MakeWrite(slot, file) });
        job.done = [copyProtect](bool ok) {
            if (ok) {
                sJob.result = mCD_TRANS_ERR_NONE;
            } else {
                mCD_Travel_Undo(&sPrivateItem, sStashed);
                Common_Set(copy_protect, copyProtect);
                sKeepSave.reset();
                sKeepSaveSet = false;
                sJob.result = mCD_TRANS_ERR_IOERROR;
            }
        };
    });

    if (ret == mCD_TRANS_ERR_BUSY) {
        return ret;
    }
    if (ret == mCD_TRANS_ERR_NONE && mLd_PlayerManKindCheck() == FALSE) {
        SaveFamicom();
    }
    if (ret != mCD_TRANS_ERR_NONE) {
        sKeepSaveSet = false;
    }
    SoftResetEnable = TRUE;
    return ret;
}

int mCD_SaveStation_Passport_bg(s32* chan) {
    int ret = RunJob(JOB_PASSPORT, chan, [&](Job& job) {
        job.minFrames = kSaveFrames;
        job.sfx = NA_SE_47;
        int slot = FindThisLand();
        if (slot < 0) {
            job.result = mCD_TRANS_ERR_NOCARD;
            return;
        }
        job.chan = slot;
        if (!CopyProtectMatches(*LoadSlot(slot))) {
            job.result = mCD_TRANS_ERR_WRONG_LAND;
            return;
        }
        SoftResetEnable = FALSE;

        int other = slot ^ 1;
        if (SlotPath(other).empty()) {
            job.chan = other;
            job.result = mCD_TRANS_ERR_NO_TOWN_DATA;
            return;
        }
        FileState state;
        auto dest = LoadSlot(other, &state);
        if (state == FileState::Corrupt) {
            job.chan = other;
            job.result = mCD_TRANS_ERR_CORRUPT;
            return;
        }

        std::string name;
        if (mLd_PlayerManKindCheck() && sKeepStartCond != mCD_START_COND_OUTGOING_FOREIGNER) {
            Passport* p = dest != nullptr ? dest->FindPassport(sCardPrivate.name) : nullptr;
            if (p == nullptr || p->data.copy_protect != sForeignerCopyProtect) {
                job.chan = other;
                job.result = mCD_TRANS_ERR_TRAVEL_DATA_MISSING;
                return;
            }
            name = p->name;
        } else {
            if (dest != nullptr && Now_Private != NULL) {
                for (auto& p : dest->passports) {
                    if (mPr_CheckCmpPersonalID(&Now_Private->player_ID, &p.data.priv.player_ID) == TRUE) {
                        name = p.name;
                        break;
                    }
                }
            }
            if (name.empty()) {
                if (dest != nullptr && LandValid(*dest)) {
                    job.chan = other;
                    job.result = mCD_TRANS_ERR_LAND_EXIST;
                    return;
                }
                name = NextPassportName(dest.get());
            }
        }

        u16 copyProtect = (u16)Common_Get(copy_protect);
        mCD_Travel_SetData(&sForeigner, &sPrivateItem, &sStashed, FALSE);
        auto passportFile = dest != nullptr ? dest->Clone() : std::make_shared<TownFile>();
        passportFile->PutPassport(name, sForeigner);
        auto home = SlotFileForWrite(slot);
        home->land = SealedCommonSave();
        home->hasLand = true;
        home->others = OthersFromAram();
        home->hasOthers = true;
        job.io = WriteAsync({ MakeWrite(other, passportFile), MakeWrite(slot, home) }, 2);
        job.done = [copyProtect, slot](bool ok) {
            sJob.chan = slot;
            if (ok) {
                sJob.result = mCD_TRANS_ERR_NONE;
            } else {
                mCD_Travel_Undo(&sPrivateItem, sStashed);
                Common_Set(copy_protect, copyProtect);
                sJob.result = mCD_TRANS_ERR_IOERROR;
            }
        };
    });

    if (ret == mCD_TRANS_ERR_BUSY) {
        return ret;
    }
    if (ret == mCD_TRANS_ERR_NONE && mLd_PlayerManKindCheck() == FALSE) {
        SaveFamicom();
    }
    SoftResetEnable = TRUE;
    return ret;
}

void mCD_toNextLand(void) {
    if (!sKeepSaveSet || !sKeepSave) {
        return;
    }
    Save* save = sKeepSave.get();
    if (mLd_CheckId(save->save.land_info.id) != TRUE || !BlockValid(reinterpret_cast<u8*>(save), sizeof(Save))) {
        return;
    }

    mCD_ApplyNextLand(save, &sForeigner);

    ResetJob();
    ClearCardPrivateTable();
    sCardPrivate = CardPrivate{};
    sForeignerCopyProtect = 0xFFFF;
    memset(&sNoLandCode, 0, sizeof(sNoLandCode));
    memset(&sErr, 0, sizeof(sErr));
    sKeepSave.reset();
    sKeepSaveSet = false;

    if (sKeepOthers) {
        OthersToAram(*sKeepOthers, true);
        sKeepOthers.reset();
    }
}

void SaveManager_PickTownFile(void) {
    sPicking = true;
    sPickerCancelled = false;
}

int SaveManager_IsPickingTownFile(void) {
    return sPicking ? 1 : 0;
}

int SaveManager_IsSaving(void) {
    return sJob.kind != JOB_NONE ? 1 : 0;
}

void SaveManager_OpenImport(void) {
    SaveManager::RequestImport("");
}

const char* SaveManager_GetTownFile(void) {
    return sOtherPath.empty() ? nullptr : sOtherPath.c_str();
}

void SaveManager_SetTownFile(const char* path) {
    std::string p = path != nullptr ? path : "";
    std::error_code ec;
    if (!p.empty() && fs::exists(p, ec) && fs::equivalent(fs::path(p), HomePath(), ec)) {
        SPDLOG_WARN("save: {} is the home town file, not a second town", p);
        return;
    }
    sOtherPath = p;
    CVarSetString(CVAR_SAVE_OTHER_TOWN, sOtherPath.c_str());
    CVarSave();
    InvalidateSlots();
}

} // extern "C"

namespace SaveManager {

void CloseTownPicker(bool picked) {
    sPicking = false;
    sPickerCancelled = !picked || sOtherPath.empty();
}

std::string TownDirectory() {
    return (SaveDir() / kTownDir).string();
}

void RequestImport(const std::string& path) {
    sImportRequested = true;
    sImportRequestPath = path;
}

bool TakeImportRequest(std::string* path) {
    if (!sImportRequested) {
        return false;
    }
    sImportRequested = false;
    *path = sImportRequestPath;
    sImportRequestPath.clear();
    return true;
}

bool CanImportGameCubeSave(std::string* reason) {
    if (sJob.kind != JOB_NONE || sSessionActive || sLandLoaded) {
        if (reason != nullptr) {
            *reason = "Import from the title screen, before pressing START.";
        }
        return false;
    }
    return true;
}

ImportPreview PreviewGameCubeSave(const std::string& path) {
    ImportPreview p;
    GcCard card;
    if (!ReadGcCard(path, card, &p.error)) {
        return p;
    }
    p.ok = true;
    if (card.town.land) {
        const Save_t& save = card.town.land->save;
        p.land = true;
        p.town = SaveGci::DecodeName(save.land_info.name, sizeof(save.land_info.name));
        for (const Private_c& priv : save.private_data) {
            if (priv.exists && mPr_NullCheckPersonalID(const_cast<PersonalID_c*>(&priv.player_ID)) == FALSE) {
                p.players.push_back(SaveGci::DecodeName(priv.player_ID.player_name, sizeof(priv.player_ID.player_name)));
            }
        }
    }
    p.letters = !card.town.mail.empty();
    p.designs = !card.town.original.empty();
    p.diary = !card.town.diary.empty();
    p.passports = (int)card.passports.size();
    p.famicom = !card.famicom.empty();
    p.nesGames = (int)card.nesGames.size();
    p.gifts = (int)card.gifts.size();
    return p;
}

bool ImportGameCubeSave(const std::string& path, std::string* message) {
    std::string error;
    if (!CanImportGameCubeSave(&error)) {
        *message = error;
        return false;
    }
    GcCard card;
    if (!ReadGcCard(path, card, &error)) {
        *message = error;
        return false;
    }

    auto f = std::make_shared<TownFile>();
    if (card.town.land) {
        f->land = card.town.land;
        SealSaveChecksum(f->land.get());
        f->hasLand = true;
    }
    if (!card.town.mail.empty() || !card.town.original.empty() || !card.town.diary.empty()) {
        f->others = NewKeep();
        f->others->Init();
        auto put = [](u8* dst, size_t size, const std::vector<u8>& src) {
            if (!src.empty()) {
                memcpy(dst, src.data(), std::min(size, src.size()));
            }
            SealBlock(dst, (u32)size);
        };
        put(f->others->mail, sizeof(f->others->mail), card.town.mail);
        put(f->others->original, sizeof(f->others->original), card.town.original);
        put(f->others->diary, sizeof(f->others->diary), card.town.diary);
        f->hasOthers = true;
    }
    if (card.town.hasNoLandCode) {
        f->noLandCode = card.town.noLandCode;
        f->hasNoLandCode = true;
    }
    f->ship["importedFrom"] = fs::path(path).filename().string();

    fs::path backup;
    if (!BackupBeforeImport(&backup)) {
        *message = "Could not back up the current town, nothing was imported.";
        return false;
    }
    if (!DoWrite(MakeWrite(kSlotHome, f))) {
        *message = "Could not write " + HomePath().string() + ".";
        return false;
    }
    InvalidateSlots();

    std::string town = f->land ? SaveGci::DecodeName(f->land->save.land_info.name,
                                                     sizeof(f->land->save.land_info.name))
                               : std::string();
    std::ostringstream out;
    if (f->land) {
        out << "Imported the town of " << town << ".";
    } else {
        out << "Imported the letters, designs and diary of an erased town.";
    }

    if (!card.passports.empty()) {
        auto travel = std::make_shared<TownFile>();
        travel->passports = card.passports;
        for (auto& p : travel->passports) {
            SealForeigner(&p.data);
        }
        fs::path dir = SaveDir() / kTownDir;
        fs::path file = dir / (SafeFileName(town) + " passports.json");
        std::error_code ec;
        for (int n = 2; fs::exists(file, ec); n++) {
            file = dir / (SafeFileName(town) + " passports " + std::to_string(n) + ".json");
        }
        if (WriteFile(file, *travel)) {
            out << " Travel passports were saved to " << file.string()
                << "; pick that file as the other town to come home.";
        } else {
            out << " The travel passports could not be saved.";
        }
    }
    if (!card.famicom.empty()) {
        if (WriteBinary(SaveDir() / "famicom.bin", card.famicom)) {
            out << " NES game saves were imported.";
        } else {
            out << " The NES game saves could not be written.";
        }
    }
    fs::path famicomDir = Ship::Context::GetPathRelativeToAppDirectory("famicom");
    std::vector<std::pair<fs::path, std::vector<u8>>> extra;
    for (const auto& game : card.nesGames) {
        extra.emplace_back(famicomDir / (game.name + ".gci"), SaveGci::WriteGci(game));
    }
    for (const auto& [name, data] : card.nesSaves) {
        extra.emplace_back(SaveDir() / "famicom" / (name + ".bin"), data);
    }
    for (const auto& gift : card.gifts) {
        extra.emplace_back(fs::path(SaveGifts::Directory()) / (gift.name + ".gci"), SaveGci::WriteGci(gift));
    }
    for (const auto& [path, data] : extra) {
        if (!WriteBinary(path, data)) {
            SPDLOG_WARN("save import: cannot write {}", path.string());
        }
    }
    if (!card.nesGames.empty() || !card.gifts.empty()) {
        out << " Copied " << card.nesGames.size() << " memory card NES games and " << card.gifts.size()
            << " gift letter files.";
    }
    if (!backup.empty()) {
        out << " The previous save is in " << backup.string() << ".";
    }
    *message = out.str();
    SPDLOG_INFO("save import: {} from {}", *message, path);
    CALL_EVENT(SaveImported, path.c_str());
    return true;
}

std::vector<PickerEntry> ListTownFiles() {
    auto out = ScanTownFiles(SaveDir() / kTownDir, HomePath());
    auto top = ScanTownFiles(SaveDir(), HomePath());
    out.insert(out.end(), top.begin(), top.end());
    return out;
}

bool UnsafeQuitAllowed() {
    return CVarGetInteger(CVAR_SAVE_UNSAFE_QUIT, 0) != 0;
}

bool OnQuitRequest() {
    if (sQuitApproved) {
        return true;
    }
    if (sQuitPrompt != QuitPrompt::None) {
        return sQuitPrompt != QuitPrompt::Saving && sFrame - sPromptDrawnFrame > 30;
    }
    if (CVarGetInteger(CVAR_SAVE_QUIT_PROMPT, 1) == 0 || !sSessionActive || !GameInPlay()) {
        return true;
    }
    sQuitPrompt = mLd_PlayerManKindCheck() ? QuitPrompt::Visiting : QuitPrompt::SaveAndQuit;
    sPromptDrawnFrame = sFrame;
    return false;
}

void NotePromptDrawn() {
    sPromptDrawnFrame = sFrame;
}

void RequestSaveAndQuit() {
    sQuitPrompt = QuitPrompt::Saving;
    sSaveAndQuitRequested = true;
}

QuitPrompt GetQuitPrompt() {
    return sQuitPrompt;
}

void DismissQuitPrompt() {
    sQuitPrompt = QuitPrompt::None;
}

void QuitWithoutSaving() {
    sQuitApproved = true;
    sQuitPrompt = QuitPrompt::None;
    CloseWindow();
}

void SaveAndQuit() {
    if (sJob.kind != JOB_NONE) {
        sSaveAndQuitRequested = true;
        return;
    }
    if (!GameInPlay() || !sSessionActive) {
        QuitWithoutSaving();
        return;
    }
    Actor_info_save_actor(reinterpret_cast<GAME_PLAY*>(gamePT));

    SaveHomeContext ctx;
    int err = PrepareSaveHome(0, ctx);
    if (err == mCD_TRANS_ERR_NONE) {
        bool ok = DoWrite(MakeWrite(ctx.slot, ctx.file));
        InvalidateSlots();
        err = FinishSaveHome(0, ctx, ok);
        if (err == mCD_TRANS_ERR_NONE) {
            SaveFamicom();
        }
    }
    if (err != mCD_TRANS_ERR_NONE) {
        SPDLOG_ERROR("save: save & quit failed ({})", err);
        sQuitPrompt = QuitPrompt::Failed;
        return;
    }
    SPDLOG_INFO("save: saved before quitting (slot {}, reset code {})", ctx.slot,
                ctx.file->land->save.private_data[Common_Get(player_no) & 3].reset_code);
    sQuitApproved = true;
    sQuitPrompt = QuitPrompt::None;
    CloseWindow();
}

bool RoundTripSelfTest(std::string* report) {
    std::ostringstream out;
    bool ok = true;
    auto compare = [&](const char* name, const void* a, const void* b, size_t size, size_t jsonSize) {
        size_t bad = 0;
        for (size_t i = 0; i < size; i++) {
            bad += static_cast<const u8*>(a)[i] != static_cast<const u8*>(b)[i];
        }
        out << name << ": " << size << " bytes, json " << jsonSize << " bytes, "
            << (bad == 0 ? std::string("lossless") : "MISMATCH in " + std::to_string(bad) + " bytes") << "\n";
        ok &= bad == 0;
    };

    auto a = NewSave();
    auto b = NewSave();
    memcpy(&a->save, &Common_Get(save).save, sizeof(Save_t));
    SaveJson_ClearPadding(SAVEJSON_SAVE, &a->save);
    std::string text = SaveJson(a->save).dump(1);
    from_json(SaveJson::parse(text), b->save);
    compare("Save_t", &a->save, &b->save, sizeof(Save_t), text.size());

    if (sAram) {
        auto ka = NewKeep();
        auto kb = NewKeep();
        *ka = *sAram;
        SaveJson_ClearPadding(SAVEJSON_KEEP_MAIL, ka->mail);
        SaveJson_ClearPadding(SAVEJSON_KEEP_ORIGINAL, ka->original);
        SaveJson_ClearPadding(SAVEJSON_KEEP_DIARY, ka->diary);
        text = SaveJson(ka->Mail()).dump(1);
        from_json(SaveJson::parse(text), kb->Mail());
        compare("mCD_keep_mail_c", ka->mail, kb->mail, sizeof(mCD_keep_mail_c), text.size());
        text = SaveJson(ka->Original()).dump(1);
        from_json(SaveJson::parse(text), kb->Original());
        compare("mCD_keep_original_c", ka->original, kb->original, sizeof(mCD_keep_original_c), text.size());
        text = SaveJson(ka->Diary()).dump(1);
        from_json(SaveJson::parse(text), kb->Diary());
        compare("mCD_keep_diary_c", ka->diary, kb->diary, sizeof(mCD_keep_diary_c), text.size());
    }

    auto fa = std::make_unique<mCD_foreigner_c>(sForeigner);
    auto fb = std::make_unique<mCD_foreigner_c>();
    memset(fb.get(), 0, sizeof(mCD_foreigner_c));
    SaveJson_ClearPadding(SAVEJSON_FOREIGNER, fa.get());
    text = SaveJson(*fa).dump(1);
    from_json(SaveJson::parse(text), *fb);
    compare("mCD_foreigner_c", fa.get(), fb.get(), sizeof(mCD_foreigner_c), text.size());

    if (report != nullptr) {
        *report = out.str();
    }
    return ok;
}

bool InPlay() {
    return GameInPlay();
}

void Tick() {
    sFrame++;
    if (sSaveAndQuitRequested) {
        sSaveAndQuitRequested = false;
        SaveAndQuit();
    }

    int mode = CVarGetInteger(CVAR_SAVE_SELF_TEST, 0);
    if (mode == 0 || !GameInPlay()) {
        sPlayFrames = 0;
        return;
    }
    if (++sPlayFrames < 120) {
        return;
    }
    CVarSetInteger(CVAR_SAVE_SELF_TEST, 0);
    std::string json;
    std::string io;
    bool ok = RoundTripSelfTest(&json);
    ok &= IoSelfTest(&io);
    SPDLOG_INFO("save self-test {}:\n{}{}", ok ? "passed" : "FAILED", json, io);

    if (mode == 2) {
        fs::path dir = fs::temp_directory_path() / ("bellwood-save-quittest-" + std::to_string(std::time(nullptr)));
        std::error_code ec;
        fs::create_directories(dir, ec);
        sSaveDirOverride = dir;
        InvalidateSlots();
        u16 id = Save_Get(land_info).id;
        if (!mLd_CHECK_LAND_ID(id)) {
            Save_GetPointer(land_info)->id = mLd_BITMASK | (id & 0xFF);
        }
        Common_Set(now_private, Save_GetPointer(private_data[0]));
        Common_Set(player_no, 0);
        Save_Set(save_exist, FALSE);
        Now_Private->reset_code = 1;
        sSessionActive = true;
        sQuitTestArmed = true;
        SPDLOG_INFO("save: quit test armed, saving into {}", dir.string());
    }
}

bool QuitTestArmed() {
    return sQuitTestArmed;
}

template <typename F> int Poll(F&& bg) {
    for (int i = 0; i < 100000; i++) {
        int r = bg();
        if (r != mCD_TRANS_ERR_BUSY) {
            return r;
        }
        if (sJob.io.valid()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }
    return mCD_TRANS_ERR_BUSY;
}

bool IoSelfTest(std::string* report) {
    std::ostringstream out;
    bool ok = true;
    auto expect = [&](bool cond, const std::string& what) {
        out << (cond ? "ok   " : "FAIL ") << what << "\n";
        ok &= cond;
    };

    auto common = std::make_unique<common_data_t>(common_data);
    auto aram = NewKeep();
    if (sAram) {
        *aram = *sAram;
    }
    u8 softReset = SoftResetEnable;
    bool session = sSessionActive;
    fs::path dir = fs::temp_directory_path() / ("bellwood-save-selftest-" + std::to_string(std::time(nullptr)));
    std::error_code ec;
    fs::create_directories(dir, ec);
    sSaveDirOverride = dir;
    InvalidateSlots();
    ResetJob();
    memset(&sNoLandCode, 0, sizeof(sNoLandCode));

    if (Now_Private == NULL) {
        Common_Set(now_private, Save_GetPointer(private_data[0]));
    }
    u16 landId = Save_Get(land_info).id;
    if (!mLd_CHECK_LAND_ID(landId)) {
        Save_GetPointer(land_info)->id = mLd_BITMASK | (landId & 0xFF);
    }
    out << "town id 0x" << std::hex << Save_Get(land_info).id << std::dec << (landId != Save_Get(land_info).id ? " (made valid)" : "")
        << ", player " << (int)Common_Get(player_no) << "\n";
    Save_Set(save_exist, FALSE);
    int chan = -1;
    auto t0 = std::chrono::steady_clock::now();
    int r = Poll([&] { return mCD_SaveHome_bg(1, &chan); });
    auto ms = [](auto from) {
        return (long long)std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - from)
            .count();
    };
    expect(r == mCD_TRANS_ERR_NONE && chan == kSlotHome,
           "SaveHome on an empty slot A: " + std::to_string(r) + " (" + std::to_string(ms(t0)) + " ms)");
    expect(fs::exists(HomePath()), "town.json written: " + HomePath().string());

    auto written = LoadSlot(kSlotHome);
    expect(written != nullptr && LandValid(*written), "town.json parses with a valid land");
    expect(mCD_GetThisLandSlotNo() == kSlotHome, "GetThisLandSlotNo finds slot A");
    if (written != nullptr) {
        TownFile raw;
        bool same = RawUnpack(RawPack(*written), raw) && raw.hasLand && raw.hasOthers &&
                    memcmp(raw.land.get(), written->land.get(), sizeof(Save)) == 0 &&
                    memcmp(raw.others.get(), written->others.get(), sizeof(KeepData)) == 0;
        expect(same, "raw-bytes mirror layout packs and unpacks the town");
    }

    r = Poll([&] { return mCD_SaveHome_bg(0, &chan); });
    expect(r == mCD_TRANS_ERR_NONE, "SaveHome over the existing town: " + std::to_string(r));
    expect(fs::exists(BackupPath(HomePath())), "previous file kept as town.json.bak");

    auto saved = SealedCommonSave();
    SaveJson_ClearPadding(SAVEJSON_SAVE, &saved->save);
    memset(Common_GetPointer(save), 0, sizeof(Save));
    InvalidateSlots();
    t0 = std::chrono::steady_clock::now();
    mCD_LoadLand();
    expect(Common_Get(save_error_type) == mFRm_ERROR_BAD_RTC,
           "LoadLand reports no error (" + std::to_string(ms(t0)) + " ms)");
    expect(mFRm_CheckSaveData() == TRUE, "loaded land passes mFRm_CheckSaveData");
    expect(mFRm_ReturnCheckSum(reinterpret_cast<u16*>(Common_GetPointer(save)), sizeof(Save)) == 0,
           "checksum recomputed on load");
    auto loaded = NewSave();
    memcpy(loaded.get(), Common_GetPointer(save), sizeof(Save));
    SaveJson_ClearPadding(SAVEJSON_SAVE, &loaded->save);
    loaded->save.save_check.checksum = 0;
    saved->save.save_check.checksum = 0;
    size_t diff = 0;
    for (size_t i = 0; i < sizeof(Save_t); i++) {
        diff += reinterpret_cast<u8*>(&loaded->save)[i] != reinterpret_cast<u8*>(&saved->save)[i];
    }
    expect(diff == 0, "loaded Save_t matches the saved one (" + std::to_string(diff) + " bytes differ)");

    int slot = -1;
    Save_Set(save_exist, TRUE);
    r = Poll([&] { return mCD_EraseLand_bg(&slot); });
    expect(r == mCD_TRANS_ERR_NONE, "EraseLand: " + std::to_string(r));
    mCD_LoadLand();
    expect(Common_Get(save_error_type) == mFRm_ERROR_BAD_RTC, "erased town loads without error");
    expect(mCD_CheckProtectCode(&sNoLandCode) == TRUE, "erased town keeps its letters under a protect code");
    Save_Set(save_exist, FALSE);
    expect(mCD_GetSaveHomeSlotNo() == kSlotHome, "a new town saves back into the erased slot");
    r = Poll([&] { return mCD_SaveHome_bg(1, &chan); });
    auto reused = LoadSlot(kSlotHome);
    expect(r == mCD_TRANS_ERR_NONE && reused != nullptr && LandValid(*reused) && reused->hasOthers &&
               !reused->hasNoLandCode,
           "new town written over the erased one, letters kept: " + std::to_string(r));

    {
        std::ofstream broken(HomePath(), std::ios::trunc);
        broken << "{ not json";
    }
    InvalidateSlots();
    mCD_LoadLand();
    expect(Common_Get(save_error_type) == mFRm_ERROR_BAD_RTC && mFRm_CheckSaveData() == TRUE,
           "unreadable town.json falls back to town.json.bak");
    {
        std::ofstream broken(BackupPath(HomePath()), std::ios::trunc);
        broken << "{ not json";
    }
    InvalidateSlots();
    mCD_LoadLand();
    expect(Common_Get(save_error_type) == mFRm_ERROR_DATA_CORRUPTED, "unreadable town and backup report corruption");
    r = Poll([&] { return mCD_EraseBrokenLand_bg(&slot); });
    expect(r == mCD_TRANS_ERR_NONE, "EraseBrokenLand: " + std::to_string(r));
    mCD_LoadLand();
    expect(Common_Get(save_error_type) == mFRm_ERROR_BAD_RTC, "after EraseBrokenLand the slot loads cleanly");
    fs::path aside = HomePath();
    aside += ".broken";
    expect(fs::exists(aside), "the unreadable file is kept as town.json.broken");
    expect(mCD_card_format_bg(kSlotHome) == mCD_RESULT_SUCCESS, "format reports success");

    std::string otherPath = sOtherPath;
    int keepStartCond = sKeepStartCond;
    auto foreigner = std::make_unique<mCD_foreigner_c>(sForeigner);
    auto prepareTown = [&]() {
        memcpy(&common_data, common.get(), sizeof(common_data_t));
        ResetJob();
        u16 id = Save_Get(land_info).id;
        if (!mLd_CHECK_LAND_ID(id)) {
            Save_GetPointer(land_info)->id = mLd_BITMASK | (id & 0xFF);
        }
        Private_c* priv = Save_GetPointer(private_data[0]);
        if (mPr_NullCheckPersonalID(&priv->player_ID)) {
            memset(priv->player_ID.player_name, 'A', sizeof(priv->player_ID.player_name));
            priv->player_ID.player_id = 0xF001;
        }
        memcpy(priv->player_ID.land_name, Save_Get(land_info).name, sizeof(priv->player_ID.land_name));
        priv->player_ID.land_id = Save_Get(land_info).id;
        priv->exists = TRUE;
        Common_Set(now_private, priv);
        Common_Set(player_no, 0);
        sKeepStartCond = mCD_START_COND_1;
        sOtherPath.clear();
        memset(&sNoLandCode, 0, sizeof(sNoLandCode));
        fs::remove(HomePath(), ec);
        fs::remove(BackupPath(HomePath()), ec);
        InvalidateSlots();
        Save_Set(save_exist, FALSE);
        return Poll([&] { return mCD_SaveHome_bg(1, &chan); }) == mCD_TRANS_ERR_NONE;
    };

    expect(prepareTown(), "station: home town saved in slot A");
    {
        auto other = SlotFileForWrite(kSlotHome);
        other->land->save.land_info.id = Save_Get(land_info).id ^ 0x00FF;
        other->land->save.land_info.name[0] ^= 1;
        other->land->save.save_check.land_id = other->land->save.land_info.id;
        SealSaveChecksum(other->land.get());
        sOtherPath = (dir / "other.json").string();
        WriteFile(sOtherPath, *other);
        InvalidateSlots();
    }
    s32 stationChan = -1;
    r = mCD_CheckStation_bg(&stationChan);
    expect(r == mCD_TRANS_ERR_NONE_NEXTLAND && stationChan == kSlotOther,
           "station sees the other town in slot B: " + std::to_string(r));
    r = Poll([&] { return mCD_SaveStation_NextLand_bg(&stationChan); });
    auto away = LoadSlot(kSlotHome);
    expect(r == mCD_TRANS_ERR_NONE && sKeepSaveSet, "SaveStation_NextLand: " + std::to_string(r));
    expect(away != nullptr && away->land->save.private_data[0].exists == FALSE,
           "the traveller is marked away in the home file");
    u16 nextId = Save_Get(land_info).id ^ 0x00FF;
    mCD_toNextLand();
    expect(Save_Get(land_info).id == nextId && Common_Get(player_no) == mPr_FOREIGNER &&
               Now_Private == &sForeigner.priv,
           "toNextLand switches to the other town with the player as a visitor");
    expect(mCD_GetThisLandSlotNo() == kSlotOther, "the visited town is slot B");
    r = mCD_CheckStation_bg(&stationChan);
    expect(r == mCD_TRANS_ERR_NONE_NEXTLAND && stationChan == kSlotHome,
           "from the visited town the station offers the way home: " + std::to_string(r));

    expect(prepareTown(), "passport: home town saved in slot A");
    sOtherPath = (dir / "passport.json").string();
    InvalidateSlots();
    r = mCD_CheckStation_bg(&stationChan);
    expect(r == mCD_TRANS_ERR_NONE && stationChan == kSlotOther,
           "station offers a passport on an empty slot B: " + std::to_string(r));
    r = Poll([&] { return mCD_SaveStation_Passport_bg(&stationChan); });
    auto passport = LoadSlot(kSlotOther);
    away = LoadSlot(kSlotHome);
    expect(r == mCD_TRANS_ERR_NONE && passport != nullptr && passport->passports.size() == 1,
           "SaveStation_Passport writes one passport: " + std::to_string(r));
    expect(away != nullptr && away->land->save.private_data[0].exists == FALSE,
           "the passport holder is marked away at home");
    expect(mCD_CheckPassportFile() == kSlotOther && mCD_CheckBrokenPassportFile(kSlotOther) == FALSE,
           "the passport is found and intact");
    expect(mCD_GetPlayerNum() == 1 && mCD_CheckCardPlayerNative(0) == 0,
           "player select lists the passport holder as a native of this town");
    r = mCD_CheckStation_bg(&stationChan);
    expect(r == mCD_TRANS_ERR_TRAVEL_DATA_EXISTS, "a second trip finds the existing passport: " + std::to_string(r));

    sOtherPath = otherPath;
    sKeepStartCond = keepStartCond;
    sForeigner = *foreigner;
    sKeepSave.reset();
    sKeepSaveSet = false;
    sKeepOthers.reset();
    ClearCardPrivateTable();

    ResetJob();
    sSaveDirOverride.clear();
    InvalidateSlots();
    memcpy(&common_data, common.get(), sizeof(common_data_t));
    if (sAram) {
        *sAram = *aram;
    }
    memset(&sNoLandCode, 0, sizeof(sNoLandCode));
    SoftResetEnable = softReset;
    sSessionActive = session;
    fs::remove_all(dir, ec);

    if (report != nullptr) {
        *report = out.str();
    }
    return ok;
}

} // namespace SaveManager
