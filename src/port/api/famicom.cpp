#ifdef ENABLE_SCRIPTING

#include "port/ShipCompat.h"
#include "port/ShipInit.hpp"
#include "port/events/Events.h"

#define this this_
extern "C" {
#include "port/api/famicom.h"
#include "ac_my_room.h"
#include "m_common_data.h"
}
#undef this

#include <ship/resource/ResourceManager.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <spdlog/spdlog.h>

#include <array>
#include <cctype>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr int kMaxGames = C_FAMICOM_LAST - C_FAMICOM_FIRST + 1;
constexpr size_t kMaxSaveName = 32;

struct CustomGame {
    bool used = false;
    std::string saveName;
    std::vector<uint8_t> rom;
    std::vector<uint8_t> tags;
};

std::array<CustomGame, kMaxGames> sGames;

std::vector<uint8_t> MakeTags(const std::vector<uint8_t>& rom) {
    std::vector<uint8_t> tags = { 'G', 'I', 'D', 0 };
    const bool battery = rom.size() >= 16 && memcmp(rom.data(), "NES\x1A", 4) == 0 && (rom[6] & 0x02) != 0;

    if (battery) {
        tags.insert(tags.end(), { 'B', 'B', 'R', 4, 0x00, 0x00, 0x20, 0x00 });
    }
    tags.insert(tags.end(), { 'E', 'N', 'D', 0 });
    return tags;
}

std::string MakeSaveName(const char* id) {
    std::string name = "Bellwood_";
    for (const char* c = id; *c != '\0' && name.size() < kMaxSaveName; c++) {
        name += std::isalnum((unsigned char)*c) || *c == '-' || *c == '_' || *c == '.' ? *c : '_';
    }
    return name;
}

int Register(const char* id, std::vector<uint8_t> rom) {
    if (id == nullptr || id[0] == '\0' || rom.size() < 16) {
        return -1;
    }
    for (int slot = 0; slot < kMaxGames; slot++) {
        CustomGame& game = sGames[slot];
        if (game.used) {
            continue;
        }
        game.used = true;
        game.saveName = MakeSaveName(id);
        game.tags = MakeTags(rom);
        game.rom = std::move(rom);
        return C_FAMICOM_FIRST + slot;
    }
    return -1;
}

void InitFamicomApi() {
    REGISTER_LISTENER(FamicomRomLookup, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        auto* lookup = reinterpret_cast<FamicomRomLookup*>(event);
        const int slot = lookup->romId - C_FAMICOM_FIRST;
        if (slot < 0 || slot >= kMaxGames || !sGames[slot].used) {
            return;
        }
        const CustomGame& game = sGames[slot];
        lookup->rom = game.rom.data();
        lookup->size = (uint32_t)game.rom.size();
        lookup->tags = game.tags.data();
        lookup->tagsSize = (uint32_t)game.tags.size();
        lookup->saveName = game.saveName.c_str();
    });
}

RegisterShipInitFunc sInitFamicomApi(InitFamicomApi);

} // namespace

extern "C" int C_RegisterFamicomGame(const char* id, const uint8_t* rom, uint32_t size) {
    if (rom == nullptr) {
        return -1;
    }
    return Register(id, std::vector<uint8_t>(rom, rom + size));
}

extern "C" int C_RegisterFamicomGameFile(const char* id, const char* path) {
    if (path == nullptr) {
        return -1;
    }
    const auto file = ShipCompat::GetResourceManager()->GetArchiveManager()->LoadFile(path);
    if (file == nullptr || !file->IsLoaded || file->Buffer == nullptr) {
        SPDLOG_ERROR("Famicom: could not read {}", path);
        return -1;
    }
    return Register(id, std::vector<uint8_t>(file->Buffer->begin(), file->Buffer->end()));
}

extern "C" void C_UnregisterFamicomGame(int rom_no) {
    const int slot = rom_no - C_FAMICOM_FIRST;
    if (slot >= 0 && slot < kMaxGames) {
        sGames[slot] = {};
    }
}

extern "C" void C_FamicomFurnitureMove(FTR_ACTOR* ftr_actor, ACTOR* my_room, GAME* game, int rom_no) {
    aMR_Clip_c* clip = Common_Get(clip).my_room_clip;
    if (clip != nullptr && clip->famicom_emu_common_move_proc != nullptr) {
        clip->famicom_emu_common_move_proc(ftr_actor, my_room, game, rom_no, 0xFF);
    }
}

#endif
