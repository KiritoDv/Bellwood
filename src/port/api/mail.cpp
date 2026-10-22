#ifdef ENABLE_SCRIPTING

#include "port/api/GameText.hpp"
#include "port/api/SizedDef.hpp"
#include "port/events/Events.h"
#include "port/interpolation/FrameInterpolation.h"

#define this this_
extern "C" {
#include "port/api/mail.h"
#include "m_common_data.h"
#include "m_font.h"
#include "m_house.h"
#include "m_mail.h"
#include "m_post_office.h"
#include "m_private.h"
}
#undef this

#include <algorithm>
#include <cstring>
#include <string>

namespace {

void CopyText(u8* out, size_t capacity, const char* text) {
    memset(out, CHAR_SPACE, capacity);
    const std::string encoded = GameText_Encode(text != nullptr ? text : "", true);
    memcpy(out, encoded.data(), std::min(encoded.size(), capacity));
}

int CopyHeader(u8* out, const char* text) {
    std::string front = text != nullptr ? text : "";
    std::string back;
    if (size_t split = front.find('\n'); split != std::string::npos) {
        back = front.substr(split + 1);
        front.resize(split);
    }
    const std::string encodedFront = GameText_Encode(front, true);
    const std::string encodedBack = GameText_Encode(back, true);
    memset(out, CHAR_SPACE, MAIL_HEADER_LEN);
    const size_t frontSize = std::min<size_t>(encodedFront.size(), MAIL_HEADER_LEN);
    memcpy(out, encodedFront.data(), frontSize);
    memcpy(out + frontSize, encodedBack.data(), std::min(encodedBack.size(), MAIL_HEADER_LEN - frontSize));
    return (int)frontSize;
}

} // namespace

extern "C" int C_SendLetterSized(const C_LetterDef* letter_def, int player, uint32_t size) {
    const SizedDef<C_LetterDef> sized(letter_def, size);
    const C_LetterDef* def = sized.get();
    if (def == nullptr) {
        return FALSE;
    }
    if (player < 0) {
        player = Common_Get(player_no);
    }
    if (player < 0 || player >= PLAYER_NUM) {
        return FALSE;
    }
    PersonalID_c* recipient = &Save_Get(private_data[player]).player_ID;
    if (mPr_NullCheckPersonalID(recipient)) {
        return FALSE;
    }

    Mail_c mail;
    mMl_clear_mail(&mail);
    mail.content.header_back_start = (u8)CopyHeader(mail.content.header, def->header);
    CopyText(mail.content.body, MAIL_BODY_LEN, def->body);
    CopyText(mail.content.footer, MAIL_FOOTER_LEN, def->footer);
    mail.content.font = mMl_FONT_RECV;
    mail.content.mail_type = 0;
    const mActor_name_t paper = def->paper != EMPTY_NO ? def->paper : ITM_PAPER_START;
    mail.content.paper_type = (u8)((paper - ITM_PAPER_START) % PAPER_UNIQUE_NUM);
    mail.present = def->present;
    mPr_CopyPersonalID(&mail.header.recipient.personalID, recipient);
    mail.header.recipient.type = mMl_NAME_TYPE_PLAYER;
    mPr_ClearPersonalID(&mail.header.sender.personalID);
    CopyText(mail.header.sender.personalID.player_name, PLAYER_NAME_LEN, def->sender);
    mail.header.sender.type = mMl_NAME_TYPE_MUSEUM;

    mHm_hs_c* house = Save_GetPointer(homes[mHS_get_arrange_idx(player)]);
    if (mPr_CheckCmpPersonalID(recipient, &house->ownerID) == TRUE) {
        const int free_idx = mMl_chk_mail_free_space(house->mailbox, HOME_MAILBOX_SIZE);
        if (free_idx != -1) {
            mMl_copy_mail(&house->mailbox[free_idx], &mail);
            return TRUE;
        }
    }
    return mPO_get_keep_mail_sum() < mPO_MAIL_STORAGE_SIZE && mPO_receipt_proc(&mail, mPO_SENDTYPE_MAIL);
}

#endif
