#pragma once

#include <cstddef>
#include <cstdint>

#include <nlohmann/json.hpp>

#include "port/save/SaveGame.h"

using SaveJson = nlohmann::ordered_json;

enum SaveJsonRoot {
    SAVEJSON_SAVE,
    SAVEJSON_KEEP_MAIL,
    SAVEJSON_KEEP_ORIGINAL,
    SAVEJSON_KEEP_DIARY,
    SAVEJSON_FOREIGNER,
};

struct SaveJsonPadding {
    uint32_t offset;
    uint32_t length;
    uint8_t mask;
};

extern const char* const kSaveJsonLayoutHash;

void to_json(SaveJson& j, const Save_t& v);
void from_json(const SaveJson& j, Save_t& v);
void to_json(SaveJson& j, const mCD_keep_mail_c& v);
void from_json(const SaveJson& j, mCD_keep_mail_c& v);
void to_json(SaveJson& j, const mCD_keep_original_c& v);
void from_json(const SaveJson& j, mCD_keep_original_c& v);
void to_json(SaveJson& j, const mCD_keep_diary_c& v);
void from_json(const SaveJson& j, mCD_keep_diary_c& v);
void to_json(SaveJson& j, const mCD_foreigner_c& v);
void from_json(const SaveJson& j, mCD_foreigner_c& v);

const SaveJsonPadding* SaveJson_Padding(SaveJsonRoot root, size_t* count);

void SaveJson_ClearPadding(SaveJsonRoot root, void* data);
