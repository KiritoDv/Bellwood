#pragma once

#ifdef __cplusplus
#define BW_API extern "C"
#else
#define BW_API extern
#endif

// Declares a game asset by its path inside the game archive, e.g. "data/model/obj_e_ghog/obj_e_ghog_model".
#define C_ASSET(name, path) static const char name[] __attribute__((aligned(2))) = "__OTR__" path
