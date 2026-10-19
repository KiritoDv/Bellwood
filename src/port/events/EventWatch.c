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
#include "m_field_make.h"
#include "m_player.h"

static struct {
    int valid;
    u32 wallet;
    mActor_name_t pockets[mPr_POCKETS_SLOT_COUNT];
    s16 weather;
    s16 weather_intensity;
    int acre_x;
    int acre_z;
} l_watch;

static struct {
    int valid;
    u8 has_block[BLOCK_Z_NUM][BLOCK_X_NUM];
    mActor_name_t items[BLOCK_Z_NUM][BLOCK_X_NUM][UT_TOTAL_NUM];
    int action;     // the player's last action that changes the ground, FIELD_CHANGE_*
    int action_age; // frames since it
} l_field;

#define FIELD_ACTION_FRAMES 30

static int PortEvents_FieldAction(int main_index) {
    switch (main_index) {
        case mPlayer_INDEX_PUTIN_ITEM:
        case mPlayer_INDEX_THROW_MONEY:
            return FIELD_CHANGE_DROP;
        case mPlayer_INDEX_PICKUP:
        case mPlayer_INDEX_PICKUP_JUMP:
        case mPlayer_INDEX_PICKUP_EXCHANGE:
        case mPlayer_INDEX_GET_SCOOP:
            return FIELD_CHANGE_PICK_UP;
        case mPlayer_INDEX_PUTIN_SCOOP:
            return FIELD_CHANGE_BURY;
        case mPlayer_INDEX_DIG_SCOOP:
            return FIELD_CHANGE_DIG;
        case mPlayer_INDEX_FILL_SCOOP:
            return FIELD_CHANGE_FILL;
        case mPlayer_INDEX_SWING_AXE:
            return FIELD_CHANGE_CHOP;
        case mPlayer_INDEX_SHAKE_TREE:
            return FIELD_CHANGE_SHAKE;
        case mPlayer_INDEX_REMOVE_GRASS:
            return FIELD_CHANGE_WEED;
        case mPlayer_INDEX_PUSH:
        case mPlayer_INDEX_PULL:
        case mPlayer_INDEX_ROTATE_FURNITURE:
        case mPlayer_INDEX_PICKUP_FURNITURE:
            return FIELD_CHANGE_FURNITURE;
        default:
            return FIELD_CHANGE_GAME;
    }
}

static int PortEvents_FieldReach(int cause) {
    switch (cause) {
        case FIELD_CHANGE_SHAKE:
        case FIELD_CHANGE_FURNITURE:
            return 3;
        case FIELD_CHANGE_CHOP:
            return 2;
        default:
            return 1;
    }
}

static void PortEvents_FieldSnapshot(void) {
    int bx;
    int bz;

    for (bz = 0; bz < BLOCK_Z_NUM; bz++) {
        for (bx = 0; bx < BLOCK_X_NUM; bx++) {
            mActor_name_t* items =
                (bx < mFI_GetBlockXMax() && bz < mFI_GetBlockZMax()) ? mFI_BkNumtoUtFGTop(bx, bz) : NULL;

            l_field.has_block[bz][bx] = items != NULL;
            if (items != NULL) {
                bcopy(items, l_field.items[bz][bx], sizeof(l_field.items[bz][bx]));
            }
        }
    }
    l_field.valid = TRUE;
}

static void PortEvents_WatchField(GAME_PLAY* play) {
    PLAYER_ACTOR* player = GET_PLAYER_ACTOR(play);
    int player_x = -100;
    int player_z = -100;
    int bx;
    int bz;
    int ut;

    if (!l_field.valid) {
        PortEvents_FieldSnapshot();
        return;
    }

    if (player != NULL) {
        int action = PortEvents_FieldAction(player->now_main_index);
        int pbx;
        int pbz;
        int put_x;
        int put_z;

        if (action != FIELD_CHANGE_GAME) {
            l_field.action = action;
            l_field.action_age = 0;
        } else if (l_field.action_age < FIELD_ACTION_FRAMES) {
            l_field.action_age++;
        }
        if (mFI_Wpos2BkandUtNuminBlock(&pbx, &pbz, &put_x, &put_z, player->actor_class.world.position)) {
            player_x = pbx * UT_X_NUM + put_x;
            player_z = pbz * UT_Z_NUM + put_z;
        }
    }

    for (bz = 0; bz < BLOCK_Z_NUM; bz++) {
        for (bx = 0; bx < BLOCK_X_NUM; bx++) {
            mActor_name_t* items =
                (bx < mFI_GetBlockXMax() && bz < mFI_GetBlockZMax()) ? mFI_BkNumtoUtFGTop(bx, bz) : NULL;

            if (items == NULL || !l_field.has_block[bz][bx]) {
                continue;
            }
            for (ut = 0; ut < UT_TOTAL_NUM; ut++) {
                mActor_name_t prev = l_field.items[bz][bx][ut];
                int ut_x;
                int ut_z;
                int dx;
                int dz;
                int cause = FIELD_CHANGE_GAME;

                if (items[ut] == prev) {
                    continue;
                }
                l_field.items[bz][bx][ut] = items[ut];
                ut_x = ut % UT_X_NUM;
                ut_z = ut / UT_X_NUM;
                dx = bx * UT_X_NUM + ut_x - player_x;
                dz = bz * UT_Z_NUM + ut_z - player_z;
                if (l_field.action_age < FIELD_ACTION_FRAMES && ABS(dx) <= PortEvents_FieldReach(l_field.action) &&
                    ABS(dz) <= PortEvents_FieldReach(l_field.action)) {
                    cause = l_field.action;
                }
                CALL_EVENT(FieldItemChanged, bx, bz, ut_x, ut_z, prev, items[ut], cause);
            }
        }
    }
}

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
    l_field.valid = FALSE;
    l_field.action = FIELD_CHANGE_GAME;
    l_field.action_age = FIELD_ACTION_FRAMES;
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
    PortEvents_WatchField(play);

    if (Save_Get(save_exist) || priv != NULL) {
        if (!l_town.valid || l_town.land_id != Save_Get(land_info).id) {
            PortEvents_WatchTownReset();
        } else {
            PortEvents_TownSnapshot(TRUE);
        }
    }
}
