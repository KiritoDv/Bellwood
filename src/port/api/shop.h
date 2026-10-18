#pragma once

#include <stdint.h>

#include "api.h"
#include "m_name_table.h"

// Adds furniture, a carpet or a wallpaper to the shop's rotation. chance is the percentage of days it is on sale.
// Returns FALSE for any other kind of item.
BW_API int C_AddShopStock(mActor_name_t item, int chance);
// Removes an item added with C_AddShopStock. Copies already on sale stay until the next restock.
BW_API void C_RemoveShopStock(mActor_name_t item);
// Rolls the shop's stock again, as on a new day. Returns FALSE while the player is inside a shop or not in town.
BW_API int C_RestockShop(void);

// Adds an item to the current player's catalog, as if they had owned it.
BW_API void C_AddToCatalog(mActor_name_t item);
// Returns TRUE when the item is in the current player's catalog.
BW_API int C_IsInCatalog(mActor_name_t item);
