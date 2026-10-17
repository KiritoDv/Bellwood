#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "api.h"

BW_API int32_t CVarGetInteger(const char* name, int32_t defaultValue);
BW_API float CVarGetFloat(const char* name, float defaultValue);
BW_API const char* CVarGetString(const char* name, const char* defaultValue);
BW_API void CVarSetInteger(const char* name, int32_t value);
BW_API void CVarSetFloat(const char* name, float value);
BW_API void CVarSetString(const char* name, const char* value);
BW_API void CVarRegisterInteger(const char* name, int32_t defaultValue);
BW_API void CVarRegisterFloat(const char* name, float defaultValue);
BW_API void CVarRegisterString(const char* name, const char* defaultValue);
BW_API void CVarClear(const char* name);
BW_API bool CVarExists(const char* name);
BW_API void CVarSave(void);
