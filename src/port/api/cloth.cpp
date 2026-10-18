#ifdef ENABLE_SCRIPTING

#include "port/ShipInit.hpp"
#include "port/api/GameText.hpp"
#include "port/api/StableIds.hpp"
#include "port/api/TextureData.hpp"
#include "port/events/Events.h"

#include <spdlog/spdlog.h>

#define this this_
extern "C" {
#include "port/api/cloth.h"
#include "port/api/furniture.h"
#include "ac_my_room.h"
#include "ac_port.h"
#include "jsyswrap.h"
#include "m_huusui_room.h"
#include "m_player_lib.h"
#include "m_room_type.h"
}
#undef this

#include <array>
#include <string>
#include <unordered_map>

namespace {

constexpr int kCount = C_CLOTH_LAST + 1 - C_CLOTH_FIRST;
constexpr uint32_t kTexSize = 0x200;
constexpr uint32_t kPalSize = 0x20;

struct CustomCloth {
    bool used = false;
    std::string id;
    std::string name;
    C_ClothDef def{};
    mActor_name_t furnitureItem = EMPTY_NO;
    int furniture = -1;
};

std::array<CustomCloth, kCount> sClothes;
std::unordered_map<int, int> sByFurniture;
StableIds sIds("custom_clothes.json", C_CLOTH_FIRST, C_CLOTH_LAST);
aFTR_vtable_c sMannequinFuncs{};

CustomCloth* Find(int item) {
    const int slot = item - C_CLOTH_FIRST;
    if (slot < 0 || slot >= kCount || !sClothes[slot].used) {
        return nullptr;
    }
    return &sClothes[slot];
}

void MannequinDma(mActor_name_t ftr_name, u8* data) {
    const mActor_name_t item = C_FurnitureCloth(mRmTp_FtrItemNo2FtrIdx(ftr_name));
    mPlib_Load_PlayerTexAndPallet(data, data + kTexSize, C_ClothTexIdx(item));
}

void InitClothApi() {
    REGISTER_LISTENER(ClothLookup, EVENT_PRIORITY_NORMAL, [](IEvent* event) {
        auto* lookup = reinterpret_cast<ClothLookup*>(event);
        CustomCloth* cloth = nullptr;
        if (lookup->item != EMPTY_NO) {
            cloth = Find(lookup->item);
        } else if (auto it = sByFurniture.find(lookup->furniture); it != sByFurniture.end()) {
            cloth = Find(it->second);
        }
        if (cloth != nullptr) {
            lookup->definition = &cloth->def;
            lookup->foundItem = (uint16_t)(C_CLOTH_FIRST + (cloth - sClothes.data()));
            lookup->foundFurniture = cloth->furniture;
        }
    });
}

RegisterShipInitFunc sInitClothApi(InitClothApi);

} // namespace

extern "C" void C_UnregisterCloth(mActor_name_t item) {
    CustomCloth* cloth = Find(item);
    if (cloth == nullptr) {
        return;
    }
    cloth->used = false;
    sByFurniture.erase(cloth->furniture);
    if (cloth->furnitureItem != EMPTY_NO) {
        C_UnregisterFurniture(cloth->furnitureItem);
    }
    cloth->furnitureItem = EMPTY_NO;
    cloth->furniture = -1;
}

extern "C" mActor_name_t C_RegisterCloth(const C_ClothDef* def) {
    if (def == nullptr || def->id == nullptr || def->id[0] == '\0' || def->texture == nullptr ||
        def->palette == nullptr) {
        return EMPTY_NO;
    }
    uint8_t texture[kTexSize];
    uint8_t palette[kPalSize];
    if (!TextureData::ReadCI4(def->texture, 32, 32, texture) || !TextureData::ReadPalette16(def->palette, palette)) {
        SPDLOG_ERROR("Cloth: could not load {}", def->id);
        return EMPTY_NO;
    }
    const int number = sIds.Get(def->id);
    if (number < 0) {
        return EMPTY_NO;
    }
    const mActor_name_t item = (mActor_name_t)number;
    C_UnregisterCloth(item);

    const uint32_t texIdx = C_CLOTH_TEX_FIRST + (number - C_CLOTH_FIRST);
    AC_AramWriteRecord(RESOURCE_TEX_BOY, texIdx * kTexSize, texture, kTexSize);
    AC_AramWriteRecord(RESOURCE_PALLET_BOY, texIdx * kPalSize, palette, kPalSize);

    CustomCloth& cloth = sClothes[number - C_CLOTH_FIRST];
    cloth.id = def->id;
    cloth.name = GameText_Encode(def->name != nullptr ? def->name : "", false);
    cloth.name.resize(C_CLOTH_NAME_LEN, ' ');
    cloth.def = *def;
    cloth.def.id = cloth.id.c_str();
    cloth.def.name = cloth.name.c_str();
    cloth.used = true;

    u8 set_type = aFTR_SET_TYPE_NORMAL;
    if (aFTR_PROFILE* profile = aMR_GetBuiltInFurniture(mRmTp_FtrItemNo2FtrIdx(FTR_CLOTH_START), &set_type)) {
        if (profile->vtable != nullptr) {
            sMannequinFuncs = *profile->vtable;
            sMannequinFuncs.dma_proc = &MannequinDma;
        }
        const std::string furnitureId = cloth.id + ".mannequin";
        C_FurnitureDef furniture{};
        furniture.id = furnitureId.c_str();
        furniture.name = def->name;
        furniture.article = def->article;
        furniture.price = def->price;
        furniture.set_type = set_type;
        furniture.huusui_type = mHsRm_HUUSUI_NONE;
        furniture.profile = *profile;
        furniture.profile.vtable = profile->vtable != nullptr ? &sMannequinFuncs : nullptr;
        furniture.not_for_sale = def->not_for_sale;
        cloth.furnitureItem = C_RegisterFurniture(&furniture);
        if (cloth.furnitureItem != EMPTY_NO) {
            cloth.furniture = mRmTp_FtrItemNo2FtrIdx(cloth.furnitureItem);
            sByFurniture[cloth.furniture] = number;
        }
    }
    return item;
}

#endif
