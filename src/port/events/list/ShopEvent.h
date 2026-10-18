#pragma once

#include <stdint.h>

// Shop and catalog

// Fires after the shop's stock for the day has been rolled, including on shop upgrades.
// Write to items or rareItem to change what is for sale. Furniture fills items[0] to items[furnitureCount - 1].
DEFINE_EVENT(ShopStockGenerated,
    uint16_t* items;          // mutable stock, filled slots come first
    int32_t count;            // number of filled slots
    int32_t capacity;         // size of items
    int32_t furnitureCount;   // the first furnitureCount slots hold furniture
    uint16_t* rareItem;       // mutable rare item, NULL when this shop level has none
    int32_t shopLevel;        // 0 Nook's Cranny to 3 Nookington's
);

// Fires for every catalog page while the catalog opens, after the collected built-in entries are listed.
// Append item numbers to items and increase *count to list more entries.
DEFINE_EVENT(CatalogPageBuild,
    int32_t page;       // mCL_MENU_*
    uint16_t* items;    // mutable list of entries
    int32_t* count;     // mutable number of entries
    int32_t capacity;   // size of items
);
