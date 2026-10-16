#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "port/save/SaveGame.h"

namespace SaveGci {

constexpr const char* kGameCode = "GAFE";
constexpr const char* kTownFileName = "DobutsunomoriP_MURA";
constexpr const char* kPassportFilePrefix = "DobutsunomoriP_PL_";
constexpr const char* kFamicomSaveFileName = "DobutsunomoriP_F_SAVE";
constexpr const char* kFamicomGamePrefix = "DobutsunomoriP_F_";
constexpr const char* kFamicomGameSavePrefix = "DobutsunomoriP_S_";
constexpr const char* kGiftFilePrefix = "DobutsunomoriP_Omake_";

struct CardFile {
    std::string gameCode;
    std::string maker;
    std::string name;
    std::vector<uint8_t> entry;
    std::vector<uint8_t> data;
};

struct Town {
    std::shared_ptr<Save> land;
    std::vector<uint8_t> mail;
    std::vector<uint8_t> original;
    std::vector<uint8_t> diary;
    bool hasNoLandCode = false;
    mCD_LandProtectCode_c noLandCode{};
};

bool ReadContainer(const std::string& path, std::vector<CardFile>& files, std::string* error);
const CardFile* FindFile(const std::vector<CardFile>& files, const std::string& name);
bool ReadTown(const CardFile& file, Town& out, std::string* error);
bool ReadPassport(const CardFile& file, mCD_foreigner_c& out);
bool ReadFamicomSave(const CardFile& file, std::vector<uint8_t>& out, const char* identifier = "GAFEFSC\x1A");
std::string DecodeName(const uint8_t* text, size_t len);
size_t DataOffset(const CardFile& file);
std::vector<uint8_t> WriteGci(const CardFile& file);

} // namespace SaveGci
