#ifdef ENABLE_SCRIPTING

#include "port/events/Events.h"
#include "port/interpolation/FrameInterpolation.h"

#define this this_
extern "C" {
#include "port/api/actor.h"
#include "port/api/player.h"
#include "m_common_data.h"
#include "m_player_lib.h"
#include "m_private.h"
}
#undef this

#include <algorithm>

namespace {

Private_c* Player() {
    Private_c* priv = Common_Get(now_private);
    return priv != nullptr && priv->exists ? priv : nullptr;
}

} // namespace

extern "C" ACTOR* C_GetPlayer(void) {
    GAME_PLAY* play = C_GetPlay();
    return play != nullptr ? (ACTOR*)GET_PLAYER_ACTOR(play) : nullptr;
}

extern "C" int C_GetPlayerPosition(float* x, float* y, float* z) {
    ACTOR* player = C_GetPlayer();
    if (player == nullptr) {
        return FALSE;
    }
    if (x != nullptr) {
        *x = player->world.position.x;
    }
    if (y != nullptr) {
        *y = player->world.position.y;
    }
    if (z != nullptr) {
        *z = player->world.position.z;
    }
    return TRUE;
}

extern "C" int C_GiveItem(mActor_name_t item) {
    Private_c* priv = Player();
    if (priv == nullptr) {
        return -1;
    }
    const int pocket = mPr_GetPossessionItemIdx(priv, EMPTY_NO);
    if (pocket >= 0) {
        mPr_SetPossessionItem(priv, pocket, item, mPr_ITEM_COND_NORMAL);
    }
    return pocket;
}

extern "C" int C_TakeItem(mActor_name_t item) {
    Private_c* priv = Player();
    const int pocket = priv != nullptr ? mPr_GetPossessionItemIdx(priv, item) : -1;
    if (pocket < 0) {
        return FALSE;
    }
    mPr_SetPossessionItem(priv, pocket, EMPTY_NO, mPr_ITEM_COND_NORMAL);
    return TRUE;
}

extern "C" int C_CountItem(mActor_name_t item) {
    Private_c* priv = Player();
    return priv != nullptr ? (int)mPr_GetPossessionItemSum(priv, item) : 0;
}

extern "C" uint32_t C_GetWallet(void) {
    Private_c* priv = Player();
    return priv != nullptr ? priv->inventory.wallet : 0;
}

extern "C" void C_SetWallet(uint32_t bells) {
    if (Private_c* priv = Player()) {
        priv->inventory.wallet = std::min<uint32_t>(bells, mPr_WALLET_MAX);
    }
}

#endif
