#pragma once

#include <stdint.h>

#include "api.h"

// Values kept in the save file of the town being played, under keys like "mymod.visits". A town's values are read
// before SaveLoaded fires and written whenever the game saves the town. A new town starts with none.

BW_API int C_SaveDataHas(const char* key);
BW_API void C_SaveDataRemove(const char* key);

BW_API int32_t C_SaveDataGetInt(const char* key, int32_t defaultValue);
BW_API void C_SaveDataSetInt(const char* key, int32_t value);
BW_API float C_SaveDataGetFloat(const char* key, float defaultValue);
BW_API void C_SaveDataSetFloat(const char* key, float value);
// The string stays valid until the key is set again or another town loads.
BW_API const char* C_SaveDataGetString(const char* key, const char* defaultValue);
BW_API void C_SaveDataSetString(const char* key, const char* value);
// Copies up to capacity bytes into out and returns the size of the stored data, 0 when there is none.
BW_API uint32_t C_SaveDataGetBytes(const char* key, void* out, uint32_t capacity);
BW_API void C_SaveDataSetBytes(const char* key, const void* data, uint32_t size);
