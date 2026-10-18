#ifdef ENABLE_SCRIPTING

#include "port/ShipInit.hpp"
#include "port/api/GameText.hpp"
#include "port/api/ModOwner.hpp"
#include "port/events/Events.h"

#define this this_
extern "C" {
#include "port/api/message.h"
#include "m_msg.h"
#include "m_font.h"
#include "m_msg_data.h"
}
#undef this

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>

namespace {

std::unordered_map<int32_t, std::string> sMessages;
std::unordered_map<int32_t, std::string> sOverrides;
std::unordered_map<int32_t, std::string> sMessageOwners;
std::unordered_map<int32_t, std::string> sOverrideOwners;

void ReleaseOwned(std::unordered_map<int32_t, std::string>& texts, std::unordered_map<int32_t, std::string>& owners,
                  const std::string& mod) {
    std::erase_if(owners, [&](const auto& entry) {
        if (entry.second != mod) {
            return false;
        }
        texts.erase(entry.first);
        return true;
    });
}

bool IsCustom(int32_t msg_no) {
    return msg_no >= C_MESSAGE_FIRST && msg_no <= C_MESSAGE_LAST;
}

bool EndsWithEndCode(const std::string& text) {
    static const int kEndCodes[] = { GameText_CommandIndex("MSGEND"), GameText_CommandIndex("MSGCONTINUE"),
                                     GameText_CommandIndex("MSGTIMEEND") };
    int last = -1;

    for (size_t i = 0; i < text.size();) {
        if ((uint8_t)text[i] == CHAR_CONTROL_CODE && i + 1 < text.size()) {
            last = (uint8_t)text[i + 1];
            i += GameText_CodeSize((uint8_t)text[i + 1]);
        } else {
            last = -1;
            i++;
        }
    }
    return std::find(std::begin(kEndCodes), std::end(kEndCodes), last) != std::end(kEndCodes);
}

std::string EncodeMessage(const char* text) {
    std::string encoded = GameText_Encode(text != nullptr ? text : "", true);
    if (encoded.size() > mMsg_MSG_BUF_MAX - 2) {
        encoded.resize(mMsg_MSG_BUF_MAX - 2);
    }
    if (!EndsWithEndCode(encoded)) {
        encoded += (char)CHAR_CONTROL_CODE;
        encoded += (char)GameText_CommandIndex("MSGEND");
    }
    return encoded;
}

void InitMessageApi() {
    REGISTER_LISTENER(MessageLookup, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        auto* lookup = reinterpret_cast<MessageLookup*>(event);
        if (auto it = sMessages.find(lookup->msgNo); it != sMessages.end()) {
            lookup->text = reinterpret_cast<const uint8_t*>(it->second.data());
            lookup->size = (uint32_t)it->second.size();
        }
    });
    REGISTER_LISTENER(MessageLoaded, EVENT_PRIORITY_HIGH, [](IEvent* event) {
        auto* loaded = reinterpret_cast<MessageLoaded*>(event);
        if (auto it = sOverrides.find(loaded->msgNo); it != sOverrides.end()) {
            const size_t size = std::min<size_t>(it->second.size(), loaded->capacity);
            memcpy(loaded->text, it->second.data(), size);
        }
    });
    ModOwner::AddReleaser([](const std::string& mod) {
        ReleaseOwned(sMessages, sMessageOwners, mod);
        ReleaseOwned(sOverrides, sOverrideOwners, mod);
    });
}

RegisterShipInitFunc sInitMessageApi(InitMessageApi);

} // namespace

extern "C" int32_t C_RegisterMessage(const char* text) {
    for (int32_t msg_no = C_MESSAGE_FIRST; msg_no <= C_MESSAGE_LAST; msg_no++) {
        if (!sMessages.contains(msg_no)) {
            sMessages[msg_no] = EncodeMessage(text);
            sMessageOwners[msg_no] = ModOwner::Current();
            return msg_no;
        }
    }
    return -1;
}

extern "C" void C_UnregisterMessage(int32_t msg_no) {
    sMessages.erase(msg_no);
    sMessageOwners.erase(msg_no);
}

extern "C" void C_SetMessage(int32_t msg_no, const char* text) {
    if (IsCustom(msg_no)) {
        if (sMessages.contains(msg_no)) {
            sMessages[msg_no] = EncodeMessage(text);
        }
    } else if (msg_no >= 0 && msg_no < MSG_MAX) {
        sOverrides[msg_no] = EncodeMessage(text);
        sOverrideOwners[msg_no] = ModOwner::Current();
    }
}

extern "C" void C_ResetMessage(int32_t msg_no) {
    sOverrides.erase(msg_no);
    sOverrideOwners.erase(msg_no);
}

extern "C" int C_EncodeText(const char* text, uint8_t* out, int capacity) {
    if (text == nullptr || out == nullptr || capacity <= 0) {
        return 0;
    }
    const std::string encoded = GameText_Encode(text, true);
    const int size = std::min<int>((int)encoded.size(), capacity);
    memcpy(out, encoded.data(), size);
    return size;
}

#endif
