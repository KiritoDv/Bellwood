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
#include "m_choice.h"
#include "m_msg_data.h"
}
#undef this

#include <algorithm>
#include <cstring>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

std::unordered_map<int32_t, std::string> sMessages;
std::unordered_map<int32_t, std::string> sOverrides;
std::unordered_map<int32_t, std::string> sMessageOwners;
std::unordered_map<int32_t, std::string> sOverrideOwners;
std::unordered_map<int32_t, std::string> sChoices;
std::unordered_map<int32_t, std::string> sChoiceOwners;
std::unordered_map<int32_t, std::vector<int32_t>> sQuestionChoices;
int sLastChoice = -1;

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
    REGISTER_LISTENER(ChoiceStringLookup, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        auto* lookup = reinterpret_cast<ChoiceStringLookup*>(event);
        if (auto it = sChoices.find(lookup->strNo); it != sChoices.end()) {
            lookup->text = reinterpret_cast<const uint8_t*>(it->second.data());
            lookup->size = (uint32_t)it->second.size();
        }
    });
    REGISTER_LISTENER(ChoiceSelected, EVENT_PRIORITY_HIGH,
                      [](IEvent* event) { sLastChoice = reinterpret_cast<ChoiceSelected*>(event)->choice; });
    REGISTER_LISTENER(MessageLoaded, EVENT_PRIORITY_HIGH, [](IEvent* event) {
        auto* loaded = reinterpret_cast<MessageLoaded*>(event);
        if (auto it = sOverrides.find(loaded->msgNo); it != sOverrides.end()) {
            const size_t size = std::min<size_t>(it->second.size(), loaded->capacity);
            memcpy(loaded->text, it->second.data(), size);
        }
    });
    ModOwner::AddReleaser([](const std::string& mod) {
        std::erase_if(sQuestionChoices, [&](const auto& entry) {
            auto owner = sMessageOwners.find(entry.first);
            return owner != sMessageOwners.end() && owner->second == mod;
        });
        ReleaseOwned(sMessages, sMessageOwners, mod);
        ReleaseOwned(sOverrides, sOverrideOwners, mod);
        ReleaseOwned(sChoices, sChoiceOwners, mod);
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
    if (auto it = sQuestionChoices.find(msg_no); it != sQuestionChoices.end()) {
        for (int32_t choice_no : it->second) {
            C_UnregisterChoice(choice_no);
        }
        sQuestionChoices.erase(it);
    }
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

extern "C" int32_t C_RegisterChoice(const char* text) {
    for (int32_t choice_no = C_CHOICE_FIRST; choice_no <= C_CHOICE_LAST; choice_no++) {
        if (!sChoices.contains(choice_no)) {
            std::string encoded = GameText_Encode(text != nullptr ? text : "", true);
            encoded.resize(std::min<size_t>(encoded.size(), mChoice_CHOICE_STRING_LEN));
            sChoices[choice_no] = std::move(encoded);
            sChoiceOwners[choice_no] = ModOwner::Current();
            return choice_no;
        }
    }
    return -1;
}

extern "C" void C_UnregisterChoice(int32_t choice_no) {
    sChoices.erase(choice_no);
    sChoiceOwners.erase(choice_no);
}

extern "C" int32_t C_RegisterQuestion(const char* text, const char* const* choices, const int32_t* next_msgs,
                                      int count) {
    if (choices == nullptr || next_msgs == nullptr || count < 2 || count > C_QUESTION_MAX_CHOICES) {
        return -1;
    }
    std::vector<int32_t> choiceNos;
    for (int i = 0; i < count; i++) {
        const int32_t choice_no = C_RegisterChoice(choices[i]);
        if (choice_no < 0) {
            for (int32_t registered : choiceNos) {
                C_UnregisterChoice(registered);
            }
            return -1;
        }
        choiceNos.push_back(choice_no);
    }
    const auto code = [](std::string& out, const char* name) {
        out += (char)CHAR_CONTROL_CODE;
        out += (char)GameText_CommandIndex(name);
    };
    const auto u16 = [](std::string& out, int32_t value) {
        out += (char)((value >> 8) & 0xFF);
        out += (char)(value & 0xFF);
    };
    static const char* const kSelect[] = { "SETSELSTR2", "SETSELSTR3", "SETSELSTR4" };
    static const char* const kNext[] = { "SETNEXTMSG0", "SETNEXTMSG1", "SETNEXTMSG2", "SETNEXTMSG3" };
    std::string tail;
    code(tail, kSelect[count - 2]);
    for (int32_t choice_no : choiceNos) {
        u16(tail, choice_no);
    }
    code(tail, "BTN");
    code(tail, "OPENCHOICE");
    for (int i = 0; i < count; i++) {
        code(tail, kNext[i]);
        u16(tail, next_msgs[i] >= 0 ? next_msgs[i] : 0xFFFF);
    }
    code(tail, "FORCENEXT");
    code(tail, "MSGCONTINUE");
    std::string encoded = GameText_Encode(text != nullptr ? text : "", true);
    encoded.resize(std::min<size_t>(encoded.size(), mMsg_MSG_BUF_MAX - tail.size()));
    encoded += tail;

    for (int32_t msg_no = C_MESSAGE_FIRST; msg_no <= C_MESSAGE_LAST; msg_no++) {
        if (!sMessages.contains(msg_no)) {
            sMessages[msg_no] = std::move(encoded);
            sMessageOwners[msg_no] = ModOwner::Current();
            sQuestionChoices[msg_no] = std::move(choiceNos);
            return msg_no;
        }
    }
    for (int32_t choice_no : choiceNos) {
        C_UnregisterChoice(choice_no);
    }
    return -1;
}

extern "C" int C_GetLastChoice(void) {
    return sLastChoice;
}

#endif
