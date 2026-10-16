#include "port/save/SaveGifts.h"
#include "port/save/SaveGc.h"
#include "port/save/SaveGci.h"

#include <ship/Context.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <fstream>

#define this this_
extern "C" {
#include "m_house.h"
#include "m_mail.h"
#include "m_private.h"
}
#undef this

namespace fs = std::filesystem;

namespace SaveGifts {
namespace {

constexpr const char* kGiftPrefix = "DobutsunomoriP_Omake_";
constexpr size_t kFileSize = 0x2000;
constexpr size_t kChecksumOffset = CARD_COMMENT_SIZE + 0xE00 + 0x600;
constexpr size_t kCountOffset = kChecksumOffset + 2;
constexpr size_t kLettersOffset = kChecksumOffset + 4;
constexpr int kLetterCount = 9;

static_assert(kLettersOffset + kLetterCount * sizeof(Mail_c) <= kFileSize, "gift file layout");

uint16_t Be16(const uint8_t* p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

void Put16(uint8_t* p, uint16_t v) {
    p[0] = v >> 8;
    p[1] = v & 0xFF;
}

uint16_t Sum(const uint8_t* p, size_t size) {
    uint16_t sum = 0;
    for (size_t i = 0; i + 1 < size; i += 2) {
        sum += Be16(p + i);
    }
    return sum;
}

void PutPersonalId(uint8_t* p, const PersonalID_c& pid) {
    memcpy(p + offsetof(PersonalID_c, player_name), pid.player_name, sizeof(pid.player_name));
    memcpy(p + offsetof(PersonalID_c, land_name), pid.land_name, sizeof(pid.land_name));
    Put16(p + offsetof(PersonalID_c, player_id), pid.player_id);
    Put16(p + offsetof(PersonalID_c, land_id), pid.land_id);
}

bool DeliverFile(SaveGci::CardFile& file, int* delivered) {
    uint8_t* data = file.data.data();
    if (file.data.size() < kFileSize || Sum(data, kFileSize) != 0) {
        return false;
    }
    int count = Be16(data + kCountOffset);
    if (count <= 0 || count > kLetterCount) {
        return false;
    }

    int player_no = Common_Get(player_no);
    Private_c* priv = Now_Private;
    if (player_no >= PLAYER_NUM || priv == nullptr) {
        return false;
    }
    mHm_hs_c* house = Save_GetPointer(homes[mHS_get_arrange_idx(player_no)]);
    if (mPr_CheckCmpPersonalID(&priv->player_ID, &house->ownerID) != TRUE) {
        return false;
    }

    bool changed = false;
    for (int i = 0; i < kLetterCount && count > 0; i++) {
        uint8_t* gc = data + kLettersOffset + i * sizeof(Mail_c);
        Mail_c letter;
        memset(&letter, 0, sizeof(letter));
        SaveGcRead(gc, letter);
        if (mPr_NullCheckPersonalID(&letter.header.recipient.personalID) != TRUE) {
            continue;
        }
        int free_idx = mMl_chk_mail_free_space(house->mailbox, HOME_MAILBOX_SIZE);
        if (free_idx == -1) {
            break;
        }

        letter.content.font = mMl_FONT_RECV;
        mPr_CopyPersonalID(&letter.header.recipient.personalID, &priv->player_ID);
        letter.header.recipient.type = mMl_NAME_TYPE_PLAYER;
        mMl_copy_mail(&house->mailbox[free_idx], &letter);

        uint8_t* recipient = gc + offsetof(Mail_c, header) + offsetof(Mail_hdr_c, recipient);
        PutPersonalId(recipient + offsetof(Mail_nm_c, personalID), priv->player_ID);
        recipient[offsetof(Mail_nm_c, type)] = mMl_NAME_TYPE_PLAYER;
        gc[offsetof(Mail_c, content) + offsetof(Mail_ct_c, font)] = mMl_FONT_RECV;
        count--;
        (*delivered)++;
        changed = true;
    }
    if (!changed) {
        return false;
    }

    Put16(data + kCountOffset, (uint16_t)count);
    Put16(data + kChecksumOffset, 0);
    Put16(data + kChecksumOffset, (uint16_t)(0 - Sum(data, kFileSize)));
    return true;
}

} // namespace

std::string Directory() {
    return (fs::path(Ship::Context::GetPathRelativeToAppDirectory("save")) / "gifts").string();
}

int Deliver() {
    std::error_code ec;
    fs::path dir = Directory();
    if (!fs::is_directory(dir, ec)) {
        return 0;
    }
    std::vector<fs::path> paths;
    for (const auto& e : fs::directory_iterator(dir, ec)) {
        std::string ext = e.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
        if (e.is_regular_file(ec) && ext == ".gci") {
            paths.push_back(e.path());
        }
    }
    std::sort(paths.begin(), paths.end());

    int delivered = 0;
    for (const auto& path : paths) {
        std::vector<SaveGci::CardFile> files;
        std::string error;
        if (!SaveGci::ReadContainer(path.string(), files, &error) || files.size() != 1) {
            continue;
        }
        SaveGci::CardFile& file = files[0];
        if (file.gameCode != SaveGci::kGameCode || file.name.rfind(kGiftPrefix, 0) != 0) {
            continue;
        }
        if (!DeliverFile(file, &delivered)) {
            continue;
        }
        std::vector<uint8_t> gci = SaveGci::WriteGci(file);
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(gci.data()), (std::streamsize)gci.size());
        if (!out) {
            SPDLOG_ERROR("save: cannot update gift file {}", path.string());
        }
    }
    if (delivered > 0) {
        SPDLOG_INFO("save: delivered {} gift letters", delivered);
    }
    return delivered;
}

} // namespace SaveGifts
