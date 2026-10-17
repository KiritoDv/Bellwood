#include "port/events/Events.h"

#include "libultra/libultra.h"
#include "m_common_data.h"
#include "m_field_info.h"
#include "m_play.h"
#include "m_player_lib.h"
#include "m_private.h"

static struct {
    int valid;
    u32 wallet;
    mActor_name_t pockets[mPr_POCKETS_SLOT_COUNT];
    s16 weather;
    s16 weather_intensity;
    int acre_x;
    int acre_z;
} l_watch;

static void PortEvents_WatchPlayer(Private_c* priv) {
    int i;

    if (priv->inventory.wallet != l_watch.wallet) {
        u32 prev = l_watch.wallet;

        l_watch.wallet = priv->inventory.wallet;
        CALL_EVENT(PlayerMoneyChanged, prev, l_watch.wallet);
    }

    for (i = 0; i < mPr_POCKETS_SLOT_COUNT; i++) {
        if (priv->inventory.pockets[i] != l_watch.pockets[i]) {
            mActor_name_t prev = l_watch.pockets[i];

            l_watch.pockets[i] = priv->inventory.pockets[i];
            CALL_EVENT(PlayerInventoryChanged, i, prev, l_watch.pockets[i]);
        }
    }
}

static void PortEvents_WatchWeather(void) {
    s16 weather = Common_Get(weather);
    s16 intensity = Common_Get(weather_intensity);

    if (weather != l_watch.weather || intensity != l_watch.weather_intensity) {
        s16 prev = l_watch.weather;
        s16 prev_intensity = l_watch.weather_intensity;

        l_watch.weather = weather;
        l_watch.weather_intensity = intensity;
        CALL_EVENT(WeatherChanged, prev, weather, prev_intensity, intensity);
    }
}

static void PortEvents_WatchAcre(GAME_PLAY* play) {
    PLAYER_ACTOR* player = GET_PLAYER_ACTOR(play);
    int bx;
    int bz;

    if (player == NULL || Common_Get(field_type) != mFI_FIELDTYPE2_FG || mFI_CheckFieldData() != TRUE) {
        return;
    }

    if (mFI_Wpos2BlockNum(&bx, &bz, player->actor_class.world.position) == TRUE &&
        (bx != l_watch.acre_x || bz != l_watch.acre_z)) {
        int prev_x = l_watch.acre_x;
        int prev_z = l_watch.acre_z;

        l_watch.acre_x = bx;
        l_watch.acre_z = bz;
        CALL_EVENT(PlayerAcreChanged, prev_x, prev_z, bx, bz);
    }
}

extern void PortEvents_WatchReset(void) {
    Private_c* priv = Common_Get(now_private);

    l_watch.valid = TRUE;
    l_watch.wallet = priv != NULL ? priv->inventory.wallet : 0;
    if (priv != NULL) {
        bcopy(priv->inventory.pockets, l_watch.pockets, sizeof(l_watch.pockets));
    } else {
        bzero(l_watch.pockets, sizeof(l_watch.pockets));
    }
    l_watch.weather = Common_Get(weather);
    l_watch.weather_intensity = Common_Get(weather_intensity);
    l_watch.acre_x = -1;
    l_watch.acre_z = -1;
}

extern void PortEvents_Watch(GAME_PLAY* play) {
    Private_c* priv = Common_Get(now_private);

    if (!l_watch.valid) {
        PortEvents_WatchReset();
    }

    if (priv != NULL) {
        PortEvents_WatchPlayer(priv);
    }

    PortEvents_WatchWeather();
    PortEvents_WatchAcre(play);
}
