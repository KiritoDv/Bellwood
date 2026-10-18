#ifdef ENABLE_SCRIPTING

#include "port/api/GameText.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <utility>

namespace {

constexpr const char* kCharMap[256] = {
    "\xC2\xA1",
    "\xC2\xBF",
    "\xC3\x84",
    "\xC3\x80",
    "\xC3\x81",
    "\xC3\x82",
    "\xC3\x83",
    "\xC3\x85",
    "\xC3\x87",
    "\xC3\x88",
    "\xC3\x89",
    "\xC3\x8A",
    "\xC3\x8B",
    "\xC3\x8C",
    "\xC3\x8D",
    "\xC3\x8E",
    "\xC3\x8F",
    "\xC3\x90",
    "\xC3\x91",
    "\xC3\x92",
    "\xC3\x93",
    "\xC3\x94",
    "\xC3\x95",
    "\xC3\x96",
    "\xC3\x98",
    "\xC3\x99",
    "\xC3\x9A",
    "\xC3\x9B",
    "\xC3\x9C",
    "\xC3\x9F",
    "\xC3\x9E",
    "\xC3\xA0",
    " ",
    "!",
    "\"",
    "\xC3\xA1",
    "\xC3\xA2",
    "%",
    "&",
    "'",
    "(",
    ")",
    "~",
    "\xE2\x99\xA5",
    ",",
    "-",
    ".",
    "\xE2\x99\xAA",
    "0",
    "1",
    "2",
    "3",
    "4",
    "5",
    "6",
    "7",
    "8",
    "9",
    ":",
    "\xF0\x9F\x8C\xA2",
    "<",
    "=",
    ">",
    "?",
    "@",
    "A",
    "B",
    "C",
    "D",
    "E",
    "F",
    "G",
    "H",
    "I",
    "J",
    "K",
    "L",
    "M",
    "N",
    "O",
    "P",
    "Q",
    "R",
    "S",
    "T",
    "U",
    "V",
    "W",
    "X",
    "Y",
    "Z",
    "\xC3\xA3",
    "\xF0\x9F\x92\xA2",
    "\xC3\xA4",
    "\xC3\xA5",
    "_",
    "\xC3\xA7",
    "a",
    "b",
    "c",
    "d",
    "e",
    "f",
    "g",
    "h",
    "i",
    "j",
    "k",
    "l",
    "m",
    "n",
    "o",
    "p",
    "q",
    "r",
    "s",
    "t",
    "u",
    "v",
    "w",
    "x",
    "y",
    "z",
    "\xC3\xA8",
    "\xC3\xA9",
    "\xC3\xAA",
    "\xC3\xAB",
    "\x7F",
    "\xEF\xBF\xBD",
    "\xC3\xAC",
    "\xC3\xAD",
    "\xC3\xAE",
    "\xC3\xAF",
    "\xE2\x80\xA2",
    "\xC3\xB0",
    "\xC3\xB1",
    "\xC3\xB2",
    "\xC3\xB3",
    "\xC3\xB4",
    "\xC3\xB5",
    "\xC3\xB6",
    "\xE2\x81\xB0",
    "\xC3\xB9",
    "\xC3\xBA",
    "\xE3\x83\xBC",
    "\xC3\xBB",
    "\xC3\xBC",
    "\xC3\xBD",
    "\xC3\xBF",
    "\xC3\xBE",
    "\xC3\x9D",
    "\xC2\xA6",
    "\xC2\xA7",
    "\xE1\xB8\x8F",
    "\xE1\xB9\x89",
    "\xE2\x80\x96",
    "\xC2\xB5",
    "\xC2\xB3",
    "\xC2\xB2",
    "\xC2\xB9",
    "\xC2\xAF",
    "\xC2\xAC",
    "\xC3\x86",
    "\xC3\xA6",
    "\xE2\x80\x9E",
    "\xC2\xBB",
    "\xC2\xAB",
    "\xE2\x98\x80",
    "\xE2\x98\x81",
    "\xE2\x98\x82",
    "\xF0\x9F\x8C\xAC",
    "\xE2\x98\x83",
    "\xE2\x88\x8B",
    "\xE2\x88\x88",
    "/",
    "\xE2\x88\x9E",
    "\xE2\x97\x8B",
    "\xF0\x9F\x97\x99",
    "\xE2\x96\xA1",
    "\xE2\x96\xB3",
    "+",
    "\xE2\x9A\xA1",
    "\xE2\x99\x82",
    "\xE2\x99\x80",
    "\xF0\x9F\x8D\x80",
    "\xE2\x98\x85",
    "\xF0\x9F\x92\x80",
    "\xF0\x9F\x98\xAE",
    "\xF0\x9F\x98\x84",
    "\xF0\x9F\x98\xA3",
    "\xF0\x9F\x98\xA0",
    "\xF0\x9F\x98\x83",
    "\xC3\x97",
    "\xE2\x9E\x97",
    "\xF0\x9F\x94\xA8",
    "\xF0\x9F\x8E\x80",
    "\xE2\x9C\x89",
    "\xF0\x9F\x92\xB0",
    "\xF0\x9F\x90\xBE",
    "\xF0\x9F\x90\xB6",
    "\xF0\x9F\x90\xB1",
    "\xF0\x9F\x90\xB0",
    "\xF0\x9F\x90\xA6",
    "\xF0\x9F\x90\xAE",
    "\xF0\x9F\x90\xB7",
    "\n",
    "\xF0\x9F\x90\x9F",
    "\xF0\x9F\x90\x9E",
    ";",
    "#",
    "\xC3\x92",
    "\xC3\x93",
    "\xE2\x9A\xB7",
    "\xC3\x95",
    "\xC3\x96",
    "\xC3\x97",
    "\xC3\x98",
    "\xC3\x99",
    "\xC3\x9A",
    "\xC3\x9B",
    "\xC3\x9C",
    "\xE1\xBB\xB2",
    "\xEA\x9F\x93",
    "\xC3\x9F",
    "\xC3\xA0",
    "\xC3\xA1",
    "\xC3\xA2",
    "\xC3\xA3",
    "\xC3\xA4",
    "\xC3\xA5",
    "\xC3\xA6",
    "\xC3\xA7",
    "\xC3\xA8",
    "\xC3\xA9",
    "\xC3\xAA",
    "\xC3\xAB",
    "\xC3\xAC",
    "\xC3\xAD",
    "\xC3\xAE",
    "\xC3\xAF",
    "\xC3\xB0",
    "\xC3\xB1",
    "\xC3\xB2",
    "\xC3\xB3",
    "\xC3\xB4",
    "\xC3\xB5",
    "\xC3\xB6",
    "\xC3\xB7",
    "\xC3\xB8",
    "\xC3\xB9",
    "\xC3\xBA",
    "\xC3\xBB",
    "\xC3\xBC",
    "\xC3\xBD",
    "\xC3\xBE",
    "\xC3\xBF",
};

constexpr std::array<std::pair<const char*, uint8_t>, 123> kCommands = { {
    { "MSGEND", 2 },
    { "MSGCONTINUE", 2 },
    { "MSGCLEAR", 2 },
    { "PAUSE", 3 },
    { "BTN", 2 },
    { "TEXTCOLOR", 5 },
    { "ABLECANCEL", 2 },
    { "UNABLECANCEL", 2 },
    { "DEMOPLR", 5 },
    { "DEMONPC0", 5 },
    { "DEMONPC1", 5 },
    { "DEMONPC2", 5 },
    { "DEMONPCQST", 5 },
    { "OPENCHOICE", 2 },
    { "SETFORCEMSG", 4 },
    { "SETNEXTMSG0", 4 },
    { "SETNEXTMSG1", 4 },
    { "SETNEXTMSG2", 4 },
    { "SETNEXTMSG3", 4 },
    { "SETNEXTMSGRND2", 6 },
    { "SETNEXTMSGRND3", 8 },
    { "SETNEXTMSGRND4", 10 },
    { "SETSELSTR2", 6 },
    { "SETSELSTR3", 8 },
    { "SETSELSTR4", 10 },
    { "FORCENEXT", 2 },
    { "STR_PLAYERNAME", 2 },
    { "STR_TALKNAME", 2 },
    { "STR_TAIL", 2 },
    { "STR_YEAR", 2 },
    { "STR_MONTH", 2 },
    { "STR_WEEK", 2 },
    { "STR_DAY", 2 },
    { "STR_HOUR", 2 },
    { "STR_MIN", 2 },
    { "STR_SEC", 2 },
    { "STR_FREE0", 2 },
    { "STR_FREE1", 2 },
    { "STR_FREE2", 2 },
    { "STR_FREE3", 2 },
    { "STR_FREE4", 2 },
    { "STR_FREE5", 2 },
    { "STR_FREE6", 2 },
    { "STR_FREE7", 2 },
    { "STR_FREE8", 2 },
    { "STR_FREE9", 2 },
    { "STR_DETERMINATION", 2 },
    { "STR_COUNTRYNAME", 2 },
    { "STR_RNDNUM", 2 },
    { "STR_ITEM0", 2 },
    { "STR_ITEM1", 2 },
    { "STR_ITEM2", 2 },
    { "STR_ITEM3", 2 },
    { "STR_ITEM4", 2 },
    { "STR_FREE10", 2 },
    { "STR_FREE11", 2 },
    { "STR_FREE12", 2 },
    { "STR_FREE13", 2 },
    { "STR_FREE14", 2 },
    { "STR_FREE15", 2 },
    { "STR_FREE16", 2 },
    { "STR_FREE17", 2 },
    { "STR_FREE18", 2 },
    { "STR_FREE19", 2 },
    { "STR_MAIL", 2 },
    { "LUCK_NEUTRAL", 2 },
    { "LUCK_RELATIONSHIP", 2 },
    { "LUCK_UNPOPULAR", 2 },
    { "LUCK_BAD", 2 },
    { "LUCK_MONEY", 2 },
    { "LUCK_GOODS", 2 },
    { "LUCK_6", 2 },
    { "LUCK_7", 2 },
    { "LUCK_8", 2 },
    { "LUCK_9", 2 },
    { "MSGCONTENTS_NORMAL", 2 },
    { "MSGCONTENTS_ANGRY", 2 },
    { "MSGCONTENTS_SAD", 2 },
    { "MSGCONTENTS_FUN", 2 },
    { "MSGCONTENTS_SLEEPY", 2 },
    { "COLORCHARS", 6 },
    { "SNDCUT", 3 },
    { "LINEOFS", 3 },
    { "LINETYPE", 3 },
    { "CHARSCALE", 3 },
    { "BTN2", 2 },
    { "BGMMAKE", 4 },
    { "BGMDELETE", 4 },
    { "MSGTIMEEND", 3 },
    { "SNDTRGSYS", 3 },
    { "LINESCALE", 3 },
    { "SNDNOPAGE", 2 },
    { "VOICETRUE", 2 },
    { "VOICEFALSE", 2 },
    { "SELNOB", 2 },
    { "GIVEOPEN", 2 },
    { "GIVECLOSE", 2 },
    { "MSGCONTENTS_GLOOMY", 2 },
    { "SELNOBCLOSE", 2 },
    { "SETNEXTMSGRNDSECTION", 6 },
    { "AGBDUMMY0", 3 },
    { "AGBDUMMY1", 3 },
    { "AGBDUMMY2", 4 },
    { "SPACE", 3 },
    { "AGBDUMMY3", 2 },
    { "AGBDUMMY4", 2 },
    { "MALEFEMALECHK", 6 },
    { "AGBDUMMY5", 2 },
    { "AGBDUMMY6", 2 },
    { "AGBDUMMY7", 3 },
    { "AGBDUMMY8", 3 },
    { "AGBDUMMY9", 3 },
    { "AGBDUMMY10", 3 },
    { "STR_ISLANDNAME", 2 },
    { "SETCURSORJUST", 2 },
    { "CLRCUSRORJUST", 2 },
    { "CUTARTICLE", 2 },
    { "CAPTIALIZE", 2 },
    { "STR_AMPM", 2 },
    { "SETNEXTMSG4", 4 },
    { "SETNEXTMSG5", 4 },
    { "SETSELSTR5", 12 },
    { "SETSELSTR6", 14 },
} };

constexpr uint8_t kControlCode = 0x7F;
constexpr uint8_t kUnknownGlyph = 0x80;

const std::unordered_map<std::string, uint8_t>& GlyphIndices() {
    static const auto indices = [] {
        std::unordered_map<std::string, uint8_t> map;
        for (int i = 255; i >= 0; i--) {
            if (i != kControlCode && i != kUnknownGlyph) {
                map[kCharMap[i]] = (uint8_t)i;
            }
        }
        return map;
    }();
    return indices;
}

size_t Utf8Length(unsigned char lead) {
    if ((lead & 0xE0) == 0xC0) {
        return 2;
    }
    if ((lead & 0xF0) == 0xE0) {
        return 3;
    }
    if ((lead & 0xF8) == 0xF0) {
        return 4;
    }
    return 1;
}

std::string Trim(const std::string& text) {
    size_t start = 0;
    size_t end = text.size();
    while (start < end && std::isspace((unsigned char)text[start])) {
        start++;
    }
    while (end > start && std::isspace((unsigned char)text[end - 1])) {
        end--;
    }
    return text.substr(start, end - start);
}

bool EncodeCommand(const std::string& body, std::string& out) {
    const size_t argsStart = body.find('[');
    const std::string name = Trim(body.substr(0, argsStart));
    std::string args;

    for (size_t i = argsStart; i != std::string::npos && i < body.size(); i++) {
        if (std::isxdigit((unsigned char)body[i])) {
            args += body[i];
        }
    }
    for (size_t code = 0; code < kCommands.size(); code++) {
        if (name != kCommands[code].first) {
            continue;
        }
        const size_t size = kCommands[code].second;
        if (args.size() != (size - 2) * 2) {
            return false;
        }
        out += (char)kControlCode;
        out += (char)code;
        for (size_t i = 0; i < args.size(); i += 2) {
            out += (char)std::stoi(args.substr(i, 2), nullptr, 16);
        }
        return true;
    }
    return false;
}

} // namespace

std::string GameText_Encode(const std::string& text, bool commands) {
    const auto& glyphs = GlyphIndices();
    std::string out;

    for (size_t i = 0; i < text.size();) {
        if (commands && text.compare(i, 2, "<<") == 0) {
            const size_t end = text.find(">>", i + 2);
            if (end != std::string::npos && EncodeCommand(text.substr(i + 2, end - i - 2), out)) {
                i = end + 2;
                continue;
            }
        }
        if (text[i] == '\r') {
            i++;
            continue;
        }
        const size_t length = std::min(Utf8Length((unsigned char)text[i]), text.size() - i);
        const auto it = glyphs.find(text.substr(i, length));
        out += it != glyphs.end() ? (char)it->second : '?';
        i += length;
    }
    return out;
}

size_t GameText_CodeSize(uint8_t code) {
    return code < kCommands.size() ? kCommands[code].second : 2;
}

int GameText_CommandIndex(const char* name) {
    for (size_t code = 0; code < kCommands.size(); code++) {
        if (std::string(name) == kCommands[code].first) {
            return (int)code;
        }
    }
    return -1;
}

#endif
