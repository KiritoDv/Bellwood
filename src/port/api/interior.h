#pragma once

#include <stdint.h>

#include "api.h"
#include "m_name_table.h"
#include "port/events/Events.h"

#define C_CARPET_FIRST FLOOR_ALL_NUM
#define C_WALLPAPER_FIRST WALL_ALL_NUM
#define C_INTERIOR_LAST 0xFF
#define C_INTERIOR_NAME_LEN 16

typedef struct C_InteriorDef_s {
    const char* id;       // stable unique key, e.g. "mymod.tiles", keeps the same item number across sessions
    const char* name;     // shown in game, up to 16 characters (UTF-8)
    uint8_t article;      // mIN_ARTICLE_*
    uint32_t price;       // in Bells
    const void* texture;  // CI4: carpets 128x128 (the room's top-left quarter), wallpapers 64x128, or one 64x64 tile
    const void* palette;  // 16 colour palette for the texture
    uint8_t not_for_sale; // shows "Not for Sale" in the catalog
} C_InteriorDef;

// Registers a carpet. Returns its item number, or EMPTY_NO.
#define C_RegisterCarpet(def) C_RegisterCarpetSized((def), sizeof(*(def)))
BW_API mActor_name_t C_RegisterCarpetSized(const C_InteriorDef* def, uint32_t size);
// Registers a wallpaper. Returns its item number, or EMPTY_NO.
#define C_RegisterWallpaper(def) C_RegisterWallpaperSized((def), sizeof(*(def)))
BW_API mActor_name_t C_RegisterWallpaperSized(const C_InteriorDef* def, uint32_t size);
// Unregisters a carpet. Rooms using it show the default floor until it is registered again.
BW_API void C_UnregisterCarpet(mActor_name_t item);
// Unregisters a wallpaper. Rooms using it show the default wall until it is registered again.
BW_API void C_UnregisterWallpaper(mActor_name_t item);

static inline const C_InteriorDef* C_FindInteriorIndex(int index, int wall) {
    if (index < (wall ? C_WALLPAPER_FIRST : C_CARPET_FIRST) || index > C_INTERIOR_LAST) {
        return NULL;
    }
    {
        CALL_EVENT(InteriorLookup, index, (uint8_t)(wall != 0), NULL, NULL);
        return InteriorLookup_.definition;
    }
}

static inline const C_InteriorDef* C_FindCarpet(mActor_name_t item) {
    if (item < ITM_CARPET_START + C_CARPET_FIRST || item > ITM_CARPET_START + C_INTERIOR_LAST) {
        return NULL;
    }
    return C_FindInteriorIndex(item - ITM_CARPET_START, 0);
}

static inline const C_InteriorDef* C_FindWallpaper(mActor_name_t item) {
    if (item < ITM_WALL_START + C_WALLPAPER_FIRST || item > ITM_WALL_START + C_INTERIOR_LAST) {
        return NULL;
    }
    return C_FindInteriorIndex(item - ITM_WALL_START, 1);
}

static inline const C_InteriorDef* C_FindInterior(mActor_name_t item) {
    const C_InteriorDef* carpet = C_FindCarpet(item);

    return carpet != NULL ? carpet : C_FindWallpaper(item);
}

static inline const uint8_t* C_InteriorData(int index, int wall) {
    if (index < (wall ? C_WALLPAPER_FIRST : C_CARPET_FIRST) || index > C_INTERIOR_LAST) {
        return NULL;
    }
    {
        CALL_EVENT(InteriorLookup, index, (uint8_t)(wall != 0), NULL, NULL);
        return InteriorLookup_.data;
    }
}
