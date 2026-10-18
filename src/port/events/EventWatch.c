#include "port/events/Events.h"

#include "libultra/libultra.h"
#include "m_common_data.h"
#include "m_field_info.h"
#include "m_play.h"
#include "m_player_lib.h"
#include "m_private.h"
#include "m_house.h"
#include "m_mail.h"
#include "m_npc.h"

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

static struct {
    int valid;
    u16 land_id;
    mActor_name_t animals[ANIMAL_NUM_MAX];
    u8 mail[PLAYER_NUM][HOME_MAILBOX_SIZE];
    u32 loan[PLAYER_NUM];
    s8 house_size[PLAYER_NUM];
} l_town;

static int PortEvents_HouseOwner(mHm_hs_c* house) {
    return mPr_NullCheckPersonalID(&house->ownerID) ? -1 : mPr_GetPrivateIdx(&house->ownerID);
}

static void PortEvents_TownSnapshot(int fire) {
    int i;
    int slot;

    for (i = 0; i < ANIMAL_NUM_MAX; i++) {
        Animal_c* animal = Save_GetPointer(animals[i]);
        mActor_name_t npc_id = mNpc_CheckFreeAnimalInfo(animal) ? EMPTY_NO : animal->id.npc_id;

        if (npc_id != l_town.animals[i]) {
            mActor_name_t prev = l_town.animals[i];

            l_town.animals[i] = npc_id;
            if (fire && prev != EMPTY_NO) {
                CALL_EVENT(VillagerMovedOut, prev, i);
            }
            if (fire && npc_id != EMPTY_NO) {
                CALL_EVENT(VillagerMovedIn, npc_id, i);
            }
        }
    }

    for (i = 0; i < PLAYER_NUM; i++) {
        mHm_hs_c* house = Save_GetPointer(homes[i]);
        int player = PortEvents_HouseOwner(house);
        Private_c* priv = Save_GetPointer(private_data[i]);
        s8 size = house->size_info.size;

        for (slot = 0; slot < HOME_MAILBOX_SIZE; slot++) {
            u8 used = !mMl_check_not_used_mail(&house->mailbox[slot]);

            if (used != l_town.mail[i][slot]) {
                l_town.mail[i][slot] = used;
                if (fire && used && player >= 0) {
                    CALL_EVENT(LetterReceived, player, slot);
                }
            }
        }

        if (size != l_town.house_size[i]) {
            s8 prev = l_town.house_size[i];

            l_town.house_size[i] = size;
            if (fire && player >= 0) {
                CALL_EVENT(HouseSizeChanged, player, prev, size);
            }
        }

        if (!mPr_NullCheckPersonalID(&priv->player_ID) && priv->inventory.loan != l_town.loan[i]) {
            u32 prev = l_town.loan[i];

            l_town.loan[i] = priv->inventory.loan;
            if (fire) {
                CALL_EVENT(LoanChanged, i, prev, priv->inventory.loan);
            }
        }
    }
}

extern void PortEvents_WatchTownReset(void) {
    l_town.valid = TRUE;
    l_town.land_id = Save_Get(land_info).id;
    PortEvents_TownSnapshot(FALSE);
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

    if (Save_Get(save_exist) || priv != NULL) {
        if (!l_town.valid || l_town.land_id != Save_Get(land_info).id) {
            PortEvents_WatchTownReset();
        } else {
            PortEvents_TownSnapshot(TRUE);
        }
    }
}
