#include <libultraship.h>
#include <ship/Context.h>

#include <array>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

extern "C" {
#include "gc_compat.h"
}

namespace fs = std::filesystem;

namespace {

constexpr s32 kSectorSize = 8 * 1024;
constexpr s32 kMemSizeMbits = 16;
constexpr s32 kUserBlocks = 251;
constexpr int kChanA = 0;

struct CardEntry {
    bool used = false;
    CARDDir dir{};
    std::vector<u8> data;
};

struct Card {
    bool present = false;
    bool mounted = false;
    s32 result = CARD_RESULT_NOCARD;
    fs::path root;
    std::array<CardEntry, CARD_MAX_FILE> entries;
};

Card sCards[CARD_NUM_CHANS];
bool sInitialized = false;

u32 CardTime() {
    using namespace std::chrono;
    auto secs = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    return (u32)(secs - 946684800LL);
}

Card* GetCard(s32 chan) {
    if (chan < 0 || chan >= CARD_NUM_CHANS) {
        return nullptr;
    }
    return &sCards[chan];
}

s32 SetResult(s32 chan, s32 result) {
    if (Card* card = GetCard(chan)) {
        card->result = result;
    }
    return result;
}

s32 Finish(s32 chan, s32 result, CARDCallback callback) {
    SetResult(chan, result);
    if (callback != nullptr) {
        callback(chan, result);
    }
    return result;
}

fs::path EntryPath(const Card& card, int fileNo) {
    return card.root / (std::to_string(fileNo) + ".bin");
}

void Persist(Card& card, int fileNo) {
    CardEntry& e = card.entries[fileNo];
    std::error_code ec;
    fs::create_directories(card.root, ec);
    fs::path path = EntryPath(card, fileNo);
    if (!e.used) {
        fs::remove(path, ec);
        return;
    }
    fs::path tmp = path;
    tmp += ".tmp";
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(&e.dir), sizeof(e.dir));
        out.write(reinterpret_cast<const char*>(e.data.data()), (std::streamsize)e.data.size());
    }
    fs::rename(tmp, path, ec);
    if (ec) {
        SPDLOG_ERROR("save stub: cannot write {}: {}", path.string(), ec.message());
    }
}

void Load(Card& card) {
    for (auto& e : card.entries) {
        e = CardEntry{};
    }
    std::error_code ec;
    if (!fs::is_directory(card.root, ec)) {
        return;
    }
    for (int i = 0; i < CARD_MAX_FILE; i++) {
        fs::path path = EntryPath(card, i);
        std::ifstream in(path, std::ios::binary);
        if (!in) {
            continue;
        }
        CardEntry& e = card.entries[i];
        in.read(reinterpret_cast<char*>(&e.dir), sizeof(e.dir));
        if (!in) {
            e = CardEntry{};
            continue;
        }
        e.data.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
        e.data.resize((size_t)e.dir.length * kSectorSize);
        e.used = true;
    }
}

int FindByName(const Card& card, const char* name) {
    for (int i = 0; i < CARD_MAX_FILE; i++) {
        const CardEntry& e = card.entries[i];
        if (e.used && strncmp((const char*)e.dir.fileName, name, CARD_FILENAME_MAX) == 0) {
            return i;
        }
    }
    return -1;
}

s32 UsedBlocks(const Card& card) {
    s32 blocks = 0;
    for (const auto& e : card.entries) {
        if (e.used) {
            blocks += e.dir.length;
        }
    }
    return blocks;
}

CardEntry* GetEntry(s32 chan, s32 fileNo) {
    Card* card = GetCard(chan);
    if (card == nullptr || !card->mounted || fileNo < 0 || fileNo >= CARD_MAX_FILE || !card->entries[fileNo].used) {
        return nullptr;
    }
    return &card->entries[fileNo];
}

void UpdateIconOffsets(const CARDDir* ent, CARDStat* stat) {
    u32 offset = ent->iconAddr;
    bool iconTlut = false;

    if (offset == 0xFFFFFFFF) {
        stat->bannerFormat = 0;
        stat->iconFormat = 0;
        stat->iconSpeed = 0;
        offset = 0;
    }

    switch (CARDGetBannerFormat(ent)) {
        case CARD_STAT_BANNER_C8:
            stat->offsetBanner = offset;
            offset += 96 * 32;
            stat->offsetBannerTlut = offset;
            offset += 2 * 256;
            break;
        case CARD_STAT_BANNER_RGB5A3:
            stat->offsetBanner = offset;
            offset += 2 * 96 * 32;
            stat->offsetBannerTlut = 0xFFFFFFFF;
            break;
        default:
            stat->offsetBanner = 0xFFFFFFFF;
            stat->offsetBannerTlut = 0xFFFFFFFF;
            break;
    }

    for (int i = 0; i < CARD_ICON_MAX; i++) {
        switch (CARDGetIconFormat(ent, i)) {
            case CARD_STAT_ICON_C8:
                stat->offsetIcon[i] = offset;
                offset += 32 * 32;
                iconTlut = true;
                break;
            case CARD_STAT_ICON_RGB5A3:
                stat->offsetIcon[i] = offset;
                offset += 2 * 32 * 32;
                break;
            default:
                stat->offsetIcon[i] = 0xFFFFFFFF;
                break;
        }
    }

    if (iconTlut) {
        stat->offsetIconTlut = offset;
        offset += 2 * 256;
    } else {
        stat->offsetIconTlut = 0xFFFFFFFF;
    }
    stat->offsetData = offset;
}

} // namespace

extern "C" {

void CARDInit(void) {
    if (sInitialized) {
        return;
    }
    sInitialized = true;
    for (int i = 0; i < CARD_NUM_CHANS; i++) {
        sCards[i] = Card{};
    }
    sCards[kChanA].present = true;
    sCards[kChanA].result = CARD_RESULT_READY;
    sCards[kChanA].root = Ship::Context::GetPathRelativeToAppDirectory("save/card_a");
}

s32 CARDProbeEx(s32 chan, s32* memSize, s32* sectorSize) {
    Card* card = GetCard(chan);
    if (card == nullptr || !card->present) {
        return CARD_RESULT_NOCARD;
    }
    if (memSize != nullptr) {
        *memSize = kMemSizeMbits;
    }
    if (sectorSize != nullptr) {
        *sectorSize = kSectorSize;
    }
    return CARD_RESULT_READY;
}

s32 CARDMountAsync(s32 chan, void* workArea, CARDCallback detachCallback, CARDCallback attachCallback) {
    Card* card = GetCard(chan);
    if (card == nullptr || !card->present) {
        return Finish(chan, CARD_RESULT_NOCARD, attachCallback);
    }
    if (!card->mounted) {
        Load(*card);
        card->mounted = true;
    }
    return Finish(chan, CARD_RESULT_READY, attachCallback);
}

s32 CARDMount(s32 chan, void* workArea, CARDCallback detachCallback) {
    return CARDMountAsync(chan, workArea, detachCallback, nullptr);
}

s32 CARDUnmount(s32 chan) {
    Card* card = GetCard(chan);
    if (card == nullptr || !card->present) {
        return CARD_RESULT_NOCARD;
    }
    card->mounted = false;
    return SetResult(chan, CARD_RESULT_READY);
}

s32 CARDCheckAsync(s32 chan, CARDCallback callback) {
    Card* card = GetCard(chan);
    if (card == nullptr || !card->present) {
        return Finish(chan, CARD_RESULT_NOCARD, callback);
    }
    return Finish(chan, card->mounted ? CARD_RESULT_READY : CARD_RESULT_NOCARD, callback);
}

s32 CARDCheck(s32 chan) {
    return CARDCheckAsync(chan, nullptr);
}

s32 CARDGetResultCode(s32 chan) {
    Card* card = GetCard(chan);
    return card != nullptr ? card->result : CARD_RESULT_FATAL_ERROR;
}

s32 CARDFreeBlocks(s32 chan, s32* byteNotUsed, s32* filesNotUsed) {
    Card* card = GetCard(chan);
    if (card == nullptr || !card->mounted) {
        return SetResult(chan, CARD_RESULT_NOCARD);
    }
    int files = 0;
    for (const auto& e : card->entries) {
        if (e.used) {
            files++;
        }
    }
    if (byteNotUsed != nullptr) {
        *byteNotUsed = (kUserBlocks - UsedBlocks(*card)) * kSectorSize;
    }
    if (filesNotUsed != nullptr) {
        *filesNotUsed = CARD_MAX_FILE - files;
    }
    return SetResult(chan, CARD_RESULT_READY);
}

s32 CARDFormatAsync(s32 chan, CARDCallback callback) {
    Card* card = GetCard(chan);
    if (card == nullptr || !card->mounted) {
        return Finish(chan, CARD_RESULT_NOCARD, callback);
    }
    for (int i = 0; i < CARD_MAX_FILE; i++) {
        if (card->entries[i].used) {
            card->entries[i] = CardEntry{};
            Persist(*card, i);
        }
    }
    return Finish(chan, CARD_RESULT_READY, callback);
}

s32 CARDOpen(s32 chan, const char* fileName, CARDFileInfo* fileInfo) {
    Card* card = GetCard(chan);
    if (card == nullptr || !card->mounted) {
        return SetResult(chan, CARD_RESULT_NOCARD);
    }
    int no = FindByName(*card, fileName);
    if (no < 0) {
        return SetResult(chan, CARD_RESULT_NOFILE);
    }
    fileInfo->chan = chan;
    fileInfo->fileNo = no;
    fileInfo->offset = 0;
    fileInfo->length = (s32)card->entries[no].data.size();
    fileInfo->iBlock = card->entries[no].dir.startBlock;
    return SetResult(chan, CARD_RESULT_READY);
}

s32 CARDFastOpen(s32 chan, s32 fileNo, CARDFileInfo* fileInfo) {
    CardEntry* e = GetEntry(chan, fileNo);
    if (e == nullptr) {
        return SetResult(chan, CARD_RESULT_NOFILE);
    }
    fileInfo->chan = chan;
    fileInfo->fileNo = fileNo;
    fileInfo->offset = 0;
    fileInfo->length = (s32)e->data.size();
    fileInfo->iBlock = e->dir.startBlock;
    return SetResult(chan, CARD_RESULT_READY);
}

s32 CARDClose(CARDFileInfo* fileInfo) {
    return SetResult(fileInfo->chan, CARD_RESULT_READY);
}

s32 CARDCreateAsync(s32 chan, const char* fileName, u32 size, CARDFileInfo* fileInfo, CARDCallback callback) {
    Card* card = GetCard(chan);
    if (card == nullptr || !card->mounted) {
        return Finish(chan, CARD_RESULT_NOCARD, callback);
    }
    if (strlen(fileName) > CARD_FILENAME_MAX) {
        return Finish(chan, CARD_RESULT_NAMETOOLONG, callback);
    }
    if (FindByName(*card, fileName) >= 0) {
        return Finish(chan, CARD_RESULT_EXIST, callback);
    }
    s32 blocks = (s32)((size + kSectorSize - 1) / kSectorSize);
    if (UsedBlocks(*card) + blocks > kUserBlocks) {
        return Finish(chan, CARD_RESULT_INSSPACE, callback);
    }
    int no = -1;
    for (int i = 0; i < CARD_MAX_FILE; i++) {
        if (!card->entries[i].used) {
            no = i;
            break;
        }
    }
    if (no < 0) {
        return Finish(chan, CARD_RESULT_NOENT, callback);
    }
    CardEntry& e = card->entries[no];
    e = CardEntry{};
    e.used = true;
    memcpy(e.dir.gameName, "GAFE", 4);
    memcpy(e.dir.company, "01", 2);
    strncpy((char*)e.dir.fileName, fileName, CARD_FILENAME_MAX);
    e.dir.time = CardTime();
    e.dir.iconAddr = 0xFFFFFFFF;
    e.dir.commentAddr = 0xFFFFFFFF;
    e.dir.permission = CARD_ATTR_PUBLIC;
    e.dir.startBlock = (u16)(5 + no);
    e.dir.length = (u16)blocks;
    e.data.assign((size_t)blocks * kSectorSize, 0);
    Persist(*card, no);

    fileInfo->chan = chan;
    fileInfo->fileNo = no;
    fileInfo->offset = 0;
    fileInfo->length = (s32)e.data.size();
    fileInfo->iBlock = e.dir.startBlock;
    return Finish(chan, CARD_RESULT_READY, callback);
}

s32 CARDReadAsync(CARDFileInfo* fileInfo, void* addr, s32 length, s32 offset, CARDCallback callback) {
    s32 chan = fileInfo->chan;
    CardEntry* e = GetEntry(chan, fileInfo->fileNo);
    if (e == nullptr) {
        return Finish(chan, CARD_RESULT_NOFILE, callback);
    }
    if (offset < 0 || length < 0 || (size_t)offset + (size_t)length > e->data.size()) {
        return Finish(chan, CARD_RESULT_LIMIT, callback);
    }
    memcpy(addr, e->data.data() + offset, (size_t)length);
    return Finish(chan, CARD_RESULT_READY, callback);
}

s32 CARDRead(CARDFileInfo* fileInfo, void* addr, s32 length, s32 offset) {
    return CARDReadAsync(fileInfo, addr, length, offset, nullptr);
}

s32 CARDWriteAsync(CARDFileInfo* fileInfo, const void* addr, s32 length, s32 offset, CARDCallback callback) {
    s32 chan = fileInfo->chan;
    CardEntry* e = GetEntry(chan, fileInfo->fileNo);
    if (e == nullptr) {
        return Finish(chan, CARD_RESULT_NOFILE, callback);
    }
    if (offset < 0 || length < 0 || (size_t)offset + (size_t)length > e->data.size()) {
        return Finish(chan, CARD_RESULT_LIMIT, callback);
    }
    memcpy(e->data.data() + offset, addr, (size_t)length);
    e->dir.time = CardTime();
    Persist(sCards[chan], fileInfo->fileNo);
    return Finish(chan, CARD_RESULT_READY, callback);
}

s32 CARDDeleteAsync(s32 chan, const char* fileName, CARDCallback callback) {
    Card* card = GetCard(chan);
    if (card == nullptr || !card->mounted) {
        return Finish(chan, CARD_RESULT_NOCARD, callback);
    }
    int no = FindByName(*card, fileName);
    if (no < 0) {
        return Finish(chan, CARD_RESULT_NOFILE, callback);
    }
    card->entries[no] = CardEntry{};
    Persist(*card, no);
    return Finish(chan, CARD_RESULT_READY, callback);
}

s32 CARDDelete(s32 chan, const char* fileName) {
    return CARDDeleteAsync(chan, fileName, nullptr);
}

s32 CARDRename(s32 chan, const char* oldName, const char* newName) {
    Card* card = GetCard(chan);
    if (card == nullptr || !card->mounted) {
        return SetResult(chan, CARD_RESULT_NOCARD);
    }
    if (strlen(newName) > CARD_FILENAME_MAX) {
        return SetResult(chan, CARD_RESULT_NAMETOOLONG);
    }
    int no = FindByName(*card, oldName);
    if (no < 0) {
        return SetResult(chan, CARD_RESULT_NOFILE);
    }
    if (FindByName(*card, newName) >= 0) {
        return SetResult(chan, CARD_RESULT_EXIST);
    }
    CardEntry& e = card->entries[no];
    memset(e.dir.fileName, 0, sizeof(e.dir.fileName));
    strncpy((char*)e.dir.fileName, newName, CARD_FILENAME_MAX);
    e.dir.time = CardTime();
    Persist(*card, no);
    return SetResult(chan, CARD_RESULT_READY);
}

s32 CARDGetStatus(s32 chan, s32 fileNo, CARDStat* stat) {
    CardEntry* e = GetEntry(chan, fileNo);
    if (e == nullptr) {
        return SetResult(chan, CARD_RESULT_NOFILE);
    }
    memset(stat, 0, sizeof(*stat));
    memcpy(stat->fileName, e->dir.fileName, CARD_FILENAME_MAX);
    stat->length = (u32)e->data.size();
    stat->time = e->dir.time;
    memcpy(stat->gameName, e->dir.gameName, 4);
    memcpy(stat->company, e->dir.company, 2);
    stat->bannerFormat = e->dir.bannerFormat;
    stat->iconAddr = e->dir.iconAddr;
    stat->iconFormat = e->dir.iconFormat;
    stat->iconSpeed = e->dir.iconSpeed;
    stat->commentAddr = e->dir.commentAddr;
    UpdateIconOffsets(&e->dir, stat);
    return SetResult(chan, CARD_RESULT_READY);
}

s32 CARDSetStatusAsync(s32 chan, s32 fileNo, CARDStat* stat, CARDCallback callback) {
    CardEntry* e = GetEntry(chan, fileNo);
    if (e == nullptr) {
        return Finish(chan, CARD_RESULT_NOFILE, callback);
    }
    e->dir.bannerFormat = stat->bannerFormat;
    e->dir.iconAddr = stat->iconAddr;
    e->dir.iconFormat = stat->iconFormat;
    e->dir.iconSpeed = stat->iconSpeed;
    e->dir.commentAddr = stat->commentAddr;
    e->dir.time = CardTime();
    UpdateIconOffsets(&e->dir, stat);
    Persist(sCards[chan], fileNo);
    return Finish(chan, CARD_RESULT_READY, callback);
}

s32 __CARDGetStatusEx(s32 chan, s32 fileNo, CARDDir* dirent) {
    CardEntry* e = GetEntry(chan, fileNo);
    if (e == nullptr) {
        return SetResult(chan, CARD_RESULT_NOFILE);
    }
    *dirent = e->dir;
    return SetResult(chan, CARD_RESULT_READY);
}

s32 __CARDSetStatusEx(s32 chan, s32 fileNo, CARDDir* dirent) {
    CardEntry* e = GetEntry(chan, fileNo);
    if (e == nullptr) {
        return SetResult(chan, CARD_RESULT_NOFILE);
    }
    u16 length = e->dir.length;
    u16 startBlock = e->dir.startBlock;
    e->dir = *dirent;
    e->dir.length = length;
    e->dir.startBlock = startBlock;
    Persist(sCards[chan], fileNo);
    return SetResult(chan, CARD_RESULT_READY);
}
}
