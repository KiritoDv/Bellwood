#include "m_card.h"

#include "ac_port.h"
#include "game.h"
#include "lb_rtc.h"
#include "libultra/libultra.h"
#include "m_actor_type.h"
#include "m_all_grow_ovl.h"
#include "m_cockroach.h"
#include "m_common_data.h"
#include "m_diary.h"
#include "m_event.h"
#include "m_flashrom.h"
#include "m_font.h"
#include "m_home.h"
#include "m_island.h"
#include "m_land.h"
#include "m_lib.h"
#include "m_mail.h"
#include "m_name_table.h"
#include "m_needlework.h"
#include "m_npc.h"
#include "m_private.h"
#include "m_quest.h"
#include "m_room_type.h"
#include "m_start_data_init.h"
#include "m_time.h"
#include "sys_math.h"
#include "sys_math3d.h"
#include "types.h"
#include "zurumode.h"
#include <string.h>

extern void mCD_InitKeepMail(mCD_keep_mail_c* keep_mail) {
    Mail_c* mail = (Mail_c*)keep_mail->mail;
    int i;
    int j;

    for (i = 0; i < mCD_KEEP_MAIL_PAGE_COUNT; i++) {
        mem_clear(keep_mail->folder_names[i], sizeof(keep_mail->folder_names[i]), CHAR_SPACE);
        for (j = 0; j < mCD_KEEP_MAIL_COUNT; j++) {
            mMl_clear_mail(mail);
            mail++;
        }
    }
}

extern void mCD_InitKeepOriginal(mCD_keep_original_c* keep_original) {
    int i;
    int j;

    for (i = 0; i < mCD_KEEP_ORIGINAL_PAGE_COUNT; i++) {
        mem_clear(keep_original->folder_names[i], sizeof(keep_original->folder_names[i]), CHAR_SPACE);
        for (j = 0; j < mCD_KEEP_ORIGINAL_COUNT; j++) {
            mNW_InitOriginalData(&keep_original->original[i][j]);
        }
    }
}

extern void mCD_InitKeepDiary(mCD_keep_diary_c* keep_diary) {
    mDi_entry_c* entry_p;
    int i;
    int j;

    for (i = 0; i < mCD_KEEP_DIARY_COUNT; i++) {
        entry_p = keep_diary->entries[i];
        for (j = 0; j < mCD_KEEP_DIARY_ENTRY_COUNT; j++) {
            mDi_init_diary(entry_p);
            entry_p++;
        }
    }
}

extern void mCD_ClearForeignerFile(mCD_foreigner_c* foreigner) {
    bzero(foreigner, sizeof(mCD_foreigner_c));
    foreigner->checksum = 0;
    mPr_ClearPrivateInfo(&foreigner->priv);
    mNpc_ClearAnimalInfo(&foreigner->remove_animal);
    foreigner->copy_protect = 0xFFFF;
}

extern void mCD_SetForeignerFile(mCD_foreigner_c* foreigner, Private_c* priv, Animal_c* animal) {
    mPr_CopyPrivateInfo(&foreigner->priv, priv);
    bcopy(animal, &foreigner->remove_animal, sizeof(Animal_c));
    foreigner->checksum = mFRm_GetFlatCheckSum((u16*)foreigner, sizeof(mCD_foreigner_c), foreigner->checksum);
}

extern u16 mCD_SetForeignerCopyProtect(mCD_foreigner_c* passport) {
    f32 random = fqrand();
    u16 code = (u16)(random * 0xFFFE);

    passport->copy_protect = code;
    passport->checksum = mFRm_GetFlatCheckSum((u16*)passport, sizeof(mCD_foreigner_c), passport->checksum);
    return code;
}

extern int mCD_CheckInitProtectCode(const mCD_LandProtectCode_c* protect_code) {
    int i;

    for (i = 0; i < 4; i++) {
        if (protect_code->code[i] != 0) {
            return FALSE;
        }
    }

    return TRUE;
}

extern int mCD_CheckProtectCode(const mCD_LandProtectCode_c* protect_code) {
    int i;

    if (protect_code->code[0] != mCD_LAND_PROTECT_CODE_MAGIC0) {
        return FALSE;
    }

    for (i = 1; i < 4; i++) {
        if (protect_code->code[i] == 0) {
            return FALSE;
        }
    }

    return TRUE;
}

extern void mCD_MakeProtectCode(mCD_LandProtectCode_c* protect_code) {
    int i;

    protect_code->code[0] = mCD_LAND_PROTECT_CODE_MAGIC0;
    for (i = 1; i < 4; i++) {
        protect_code->code[i] = RANDOM(0xFFFE);
        protect_code->code[i] = protect_code->code[i] + 1;
    }
}

extern int mCD_CompNoLandCode(const mCD_LandProtectCode_c* code0, const mCD_LandProtectCode_c* code1) {
    return mCD_CheckProtectCode(code0) == TRUE && memcmp(code0->code, code1->code, sizeof(code0->code)) == 0;
}

extern u16 mCD_GetLandCopyProtect(void) {
    u16 code = RANDOM(0xFFF0);
    code++;
    return code;
}

extern void mCD_SealSave(Save* save) {
    save->save.save_check.version = mFRm_VERSION;
    mFRm_SetSaveCheckData(&save->save.save_check);
    save->save.save_check.checksum = mFRm_GetFlatCheckSum((u16*)save, sizeof(Save), save->save.save_check.checksum);
}

extern int mCD_CheckResetCode(Private_c* priv) {
    if ((priv->state_flags & mPr_FLAG_BIRTHDAY_ACTIVE) != 0) {
        priv->reset_code = 0;
    }

    return priv->reset_code == 0;
}

extern void mCD_SetResetCode(Private_c* priv) {
    priv->reset_code = (u32)RANDOM_F(USHT_MAX_S);
    priv->reset_code++;
}

static void mCD_SetResetInfo(Private_c* priv) {
    Common_Set(reset_flag, FALSE);
    if (mCD_CheckResetCode(priv) == FALSE) {
        if (AC_ResettiDisabled()) {
            return;
        }

        /* No reset penalty if zurumode 2 is enabled */
        if (!ZURUMODE2_ENABLED()) {
            Common_Set(reset_flag, TRUE);
        }

        priv->reset_count++;
    }
}

static void mCD_ClearWisps(Private_c* priv) {
    mActor_name_t* pockets_p = priv->inventory.pockets;
    int i;

    for (i = 0; i < mPr_POCKETS_SLOT_COUNT; i++) {
        if (ITEM_IS_WISP(*pockets_p)) {
            mPr_SetPossessionItem(priv, i, EMPTY_NO, mPr_ITEM_COND_NORMAL);
        }

        pockets_p++;
    }
}

extern void mCD_SaveHome_SetData(int mode) {
    Private_c* priv = Now_Private;
    u16 copy_protect;

    mCkRh_SavePlayTime(Common_Get(player_no));

    if (mode == 0) {
        bzero(&Now_Private->reset_code, sizeof(Now_Private->reset_code));
        mAGrw_ClearMoneyStoneShineGround();
    } else if (mCD_CheckResetCode(priv) == TRUE) {
        mCD_SetResetCode(priv);
    }

    if (mode == 0 && priv != NULL) {
        mCD_ClearWisps(priv);
    }

    Save_Set(save_exist, TRUE);
    copy_protect = mCD_GetLandCopyProtect();
    Common_Set(copy_protect, copy_protect);
    Save_Set(copy_protect, copy_protect);
    Save_Set(travel_hard_time, lbRTC_HardTime());
}

extern void mCD_SaveHome_Written(int mode) {
    Private_c* priv = Now_Private;

    if (mode == 1) {
        if (Common_Get(player_decoy_flag) == TRUE && priv != NULL) {
            priv->exists = TRUE;
        }
    } else {
        Common_Set(player_decoy_flag, FALSE);
    }
}

extern void mCD_GameStart_SetDecoy(void) {
    Private_c* priv = Now_Private;

    if (Common_Get(player_decoy_flag) == TRUE && priv != NULL && !mPr_NullCheckPersonalID(&priv->player_ID)) {
        priv->exists = TRUE;
        priv->equipment = EMPTY_NO;
    }
}

extern int mCD_GameStart_MakeData(int player_no, int start_cond, mCD_foreigner_c* foreigner,
                                  const mCD_foreigner_c* card_passport, int* passport_mode) {
    static int init_mode_table[] = { 0, 2, 1, 3, 3 };
    GAME* game = gamePT;
    Private_c* priv;
    Animal_c* in_animal = mNpc_GetInAnimalP();
    u16 copy_protect;

    *passport_mode = 0;

    switch (start_cond) {
        case mCD_START_COND_INCOMING_FOREIGNER:
            if (card_passport == NULL) {
                return FALSE;
            }

            priv = mPr_GetForeignerP();
            mPr_CopyPrivateInfo(priv, (Private_c*)&card_passport->priv);
            if (in_animal != NULL) {
                bcopy(&card_passport->remove_animal, in_animal, sizeof(Animal_c));
            }

            bcopy(card_passport, foreigner, sizeof(mCD_foreigner_c));
            mPr_LoadPak_and_SetPrivateInfo2(priv, player_no);
            break;
        case mCD_START_COND_OUTGOING_FOREIGNER:
            priv = mPr_GetForeignerP();
            mPr_CopyPrivateInfo(priv, &foreigner->priv);
            if (in_animal != NULL) {
                bcopy(&foreigner->remove_animal, in_animal, sizeof(Animal_c));
            }

            mPr_LoadPak_and_SetPrivateInfo2(priv, player_no);
            break;
    }

    mSDI_StartDataInit(game, player_no, init_mode_table[start_cond]);

    switch (start_cond) {
        case mCD_START_COND_0:
        case mCD_START_COND_2:
            Common_Set(copy_protect, Save_Get(copy_protect));
            break;
        case mCD_START_COND_1:
            priv = Save_GetPointer(private_data[player_no]);
            if (!mPr_NullCheckPersonalID(&priv->player_ID)) {
                mCD_SetResetInfo(priv);
                mCD_SetResetCode(priv);

                if (Common_Get(player_decoy_flag) == TRUE) {
                    priv->exists = FALSE;
                }
            }

            Save_Set(travel_hard_time, lbRTC_HardTime());
            copy_protect = mCD_GetLandCopyProtect();
            Common_Set(copy_protect, copy_protect);
            Save_Set(copy_protect, copy_protect);
            break;
        case mCD_START_COND_INCOMING_FOREIGNER:
        case mCD_START_COND_OUTGOING_FOREIGNER:
            if (mLd_PlayerManKindCheckNo(player_no) == FALSE) {
                priv = Now_Private;
                if (priv != NULL) {
                    mCD_SetResetInfo(priv);
                    mCD_SetResetCode(priv);
                    mRmTp_SetDefaultLightSwitchData(3);
                }

                if (start_cond == mCD_START_COND_INCOMING_FOREIGNER) {
                    *passport_mode = 1;
                }
            } else {
                mCD_SetResetInfo(&foreigner->priv);
                mCD_SetResetCode(&foreigner->priv);

                priv = Now_Private;
                if (priv != NULL) {
                    mCD_SetResetInfo(priv);
                    mCD_SetResetCode(priv);
                }

                mCD_SetForeignerCopyProtect(foreigner);
                if (start_cond == mCD_START_COND_INCOMING_FOREIGNER) {
                    *passport_mode = 2;
                }
            }

            Save_Set(travel_hard_time, lbRTC_HardTime());
            copy_protect = mCD_GetLandCopyProtect();
            Common_Set(copy_protect, copy_protect);
            Save_Set(copy_protect, copy_protect);
            break;
        default:
            return FALSE;
    }

    return TRUE;
}

static void mCD_CopyKeep(
    mActor_name_t* pockets_p, //
    u8* lotto_ticket_expiry_p, //
    u8* lotto_ticket_mail_storage_p, //
    u32* item_cond_p, //
    u32* wallet_p, //
    mQst_delivery_c* delivery_p, //
    mQst_errand_c* errand_p, //
    Mail_c* mail_p, //
    mPr_catalog_order_c* catalog_orders_p, //
    Anmremail_c* remail_p, //
    mPr_animal_memory_c* animal_memory_p, //
    mActor_name_t* pockets, //
    u8 lotto_ticket_expiry, //
    u8 lotto_ticket_mail_storage, //
    u32 item_cond, //
    u32 wallet, //
    mQst_delivery_c* delivery, //
    mQst_errand_c* errand, //
    Mail_c* mail, //
    mPr_catalog_order_c* catalog_orders, //
    Anmremail_c* remail, //
    mPr_animal_memory_c* animal_memory //
) {
    int i;

    bcopy(pockets, pockets_p, membersize(Private_c, inventory.pockets));
    *item_cond_p = item_cond;
    *lotto_ticket_expiry_p = lotto_ticket_expiry;
    *lotto_ticket_mail_storage_p = lotto_ticket_mail_storage;
    *wallet_p = wallet;

    for (i = 0; i < mPr_DELIVERY_QUEST_NUM; i++) {
        mQst_CopyDelivery(delivery_p, delivery);
        delivery_p++;
        delivery++;
    }

    for (i = 0; i < mPr_ERRAND_QUEST_NUM; i++) {
        mQst_CopyErrand(errand_p, errand);
        errand_p++;
        errand++;
    }

    for (i = 0; i < mPr_INVENTORY_MAIL_COUNT; i++) {
        mMl_copy_mail(mail_p, mail);
        mail_p++;
        mail++;
    }

    for (i = 0; i < mPr_CATALOG_ORDER_NUM; i++) {
        catalog_orders_p->item = catalog_orders->item;
        catalog_orders_p->shop_level = catalog_orders->shop_level;
        catalog_orders_p++;
        catalog_orders++;
    }

    bcopy(remail, remail_p, sizeof(Anmremail_c));
    bcopy(animal_memory, animal_memory_p, sizeof(mPr_animal_memory_c));
}

static void mCD_ClearPrivateItem(Private_c* priv, mCD_PrivateItem_c* privItem) {
    if (privItem != NULL) {
        bzero(privItem, sizeof(mCD_PrivateItem_c));
        mCD_CopyKeep(
            // dst
            privItem->items,
            &privItem->ticket_expiry_month,
            &privItem->ticket_storage,
            &privItem->item_cond,
            &privItem->wallet,
            privItem->delivery,
            privItem->errand,
            privItem->mail,
            privItem->catalog_order,
            &privItem->remail,
            &privItem->animal_memory,
            // src
            priv->inventory.pockets,
            priv->inventory.lotto_ticket_expiry_month,
            priv->inventory.lotto_ticket_mail_storage,
            priv->inventory.item_conditions,
            priv->inventory.wallet,
            priv->deliveries,
            priv->errands,
            priv->mail,
            priv->catalog_orders,
            &priv->remail,
            &priv->animal_memory
        );

        privItem->_0FF0 = TRUE;
    }

    bzero(priv->inventory.pockets, membersize(Private_c, inventory.pockets));
    priv->inventory.item_conditions = 0;
    priv->inventory.lotto_ticket_expiry_month = 0;
    priv->inventory.lotto_ticket_mail_storage = 0;
    priv->inventory.wallet = 0;
    mQst_ClearDelivery(priv->deliveries, mPr_DELIVERY_QUEST_NUM);
    mQst_ClearErrand(priv->errands, mPr_ERRAND_QUEST_NUM);
    mMl_clear_mail_box(priv->mail, mPr_INVENTORY_MAIL_COUNT);
    bzero(priv->catalog_orders, sizeof(priv->catalog_orders));
    mPr_ClearAnimalMemory(&priv->animal_memory);
    mNpc_ClearRemail(&priv->remail);
}

static void mCD_ReplaceKeep(Private_c* priv, mCD_PrivateItem_c* privItem) {
    if (privItem != NULL && privItem->_0FF0 == TRUE) {
        mCD_CopyKeep(
            // dst
            priv->inventory.pockets,
            &priv->inventory.lotto_ticket_expiry_month,
            &priv->inventory.lotto_ticket_mail_storage,
            &priv->inventory.item_conditions,
            &priv->inventory.wallet,
            priv->deliveries,
            priv->errands,
            priv->mail,
            priv->catalog_orders,
            &priv->remail,
            &priv->animal_memory,
            // src
            privItem->items,
            privItem->ticket_expiry_month,
            privItem->ticket_storage,
            privItem->item_cond,
            privItem->wallet,
            privItem->delivery,
            privItem->errand,
            privItem->mail,
            privItem->catalog_order,
            &privItem->remail,
            &privItem->animal_memory
        );
    }
}

extern void mCD_Travel_SetData(mCD_foreigner_c* foreigner, mCD_PrivateItem_c* keep, int* stashed, int record_land) {
    Private_c* priv = Now_Private;
    mCD_persistent_data_c* persistant = Common_GetPointer(travel_persistent_data);
    Animal_c* in_animal;
    u16 copy_protect;
    int i;

    *stashed = FALSE;
    mCkRh_SavePlayTime(Common_Get(player_no));
    if (priv != NULL) {
        mCD_ClearWisps(priv);
    }

    bzero(&Now_Private->reset_code, sizeof(Now_Private->reset_code));
    mHm_KeepHouseSize(Common_Get(player_no));
    in_animal = mNpc_GetInAnimalP();
    mNpc_GetRemoveAnimal(in_animal, TRUE);
    mCD_SetForeignerFile(foreigner, priv, in_animal);
    if (mLd_PlayerManKindCheckNo(Common_Get(player_no)) == FALSE) {
        priv->exists = FALSE;
        mCD_ClearPrivateItem(priv, keep);
        *stashed = TRUE;
    }

    if (record_land) {
        bcopy(Save_GetPointer(land_info), &persistant->land, sizeof(mLd_land_info_c));
        for (i = 0; i < PLAYER_NUM; i++) {
            mPr_CopyPersonalID(&persistant->pid[i], &Save_Get(private_data[i]).player_ID);
        }
    }

    mAGrw_ClearMoneyStoneShineGround();
    Save_Set(travel_hard_time, lbRTC_HardTime());
    copy_protect = mCD_GetLandCopyProtect();
    Common_Set(copy_protect, copy_protect);
    Save_Set(copy_protect, copy_protect);
}

extern void mCD_Travel_Undo(mCD_PrivateItem_c* keep, int stashed) {
    if (stashed && Now_Private != NULL) {
        Now_Private->exists = TRUE;
        mCD_ReplaceKeep(Now_Private, keep);
    }
}

extern void mCD_ApplyNextLand(const Save* save, mCD_foreigner_c* foreigner) {
    mCD_persistent_data_c persis;
    int scene_no = Save_Get(scene_no);
    int last_scene_no;
    mActor_name_t last_field_id;
    s16 demo_profile[2];
    Time_c time;
    Transition_c transition;
    int rtc_enabled;

    bcopy(Common_GetPointer(travel_persistent_data), &persis, sizeof(persis));
    last_scene_no = Common_Get(last_scene_no);
    last_field_id = Common_Get(last_field_id);
    bcopy(Common_Get(demo_profiles), demo_profile, sizeof(demo_profile));
    bcopy(Common_GetPointer(time), &time, sizeof(time));
    bcopy(Common_GetPointer(transition), &transition, sizeof(transition));
    bzero(&common_data, sizeof(common_data_t));

    // restore
    bcopy(&persis, Common_GetPointer(travel_persistent_data), sizeof(persis));
    Common_Set(last_field_id, last_field_id);
    Common_Set(last_scene_no, last_scene_no);
    bcopy(demo_profile, Common_Get(demo_profiles), sizeof(demo_profile));
    bcopy(&time, Common_GetPointer(time), sizeof(time));
    bcopy(&transition, Common_GetPointer(transition), sizeof(transition));
    bcopy(save, Common_GetPointer(save), sizeof(Save));

    Common_Set(copy_protect, Save_Get(copy_protect));
    Save_Set(scene_no, scene_no);
    rtc_enabled = Common_Get(time.rtc_enabled);
    Common_Set(time.rtc_enabled, TRUE);
    mTM_rtcTime_limit_check();
    Common_Set(time.rtc_enabled, rtc_enabled);
    lbRTC_GetTime(Common_GetPointer(time.rtc_time));
    Common_Set(now_private, &foreigner->priv);
    Common_Set(player_no, mPr_FOREIGNER);
    Common_Set(auto_nwrite_set, FALSE);
    bzero(Common_GetPointer(auto_nwrite_time), sizeof(Common_Get(auto_nwrite_time)));
    Common_Set(ball_pos, ZeroVec);
    mTM_clear_renew_is();
    mEv_ClearEventInfo();
    mEv_toland_clear_common();
    mNpc_ClearInAnimal();
    mNpc_FirstClearGoodbyMail();
    mQst_ClearGrabItemInfo();
    bzero(Common_Get(npc_schedule), sizeof(Common_Get(npc_schedule)));
    mISL_ClearKeepIsland();
    bzero(Common_GetPointer(unused_mail_26522), sizeof(Mail_c));
    Common_Set(_2664E, 0);
    Common_Set(_26650, 0);
    mNpc_ClearCacheName();

    if (mFRm_CheckSaveData_common(Save_GetPointer(save_check), Save_Get(land_info).id)) {
        if (Save_Get(save_check).version == 5) {
            bcopy(&Save_Get(save_check).time, Save_GetPointer(saved_auto_nwrite_time), sizeof(lbRTC_time_c));
        }
    }

    Common_Set(submenu_disabled, TRUE);
}
