#ifndef M_CARD_H
#define M_CARD_H

#include "gc_compat.h"
#include "types.h"
#include "libu64/gfxprint.h"
#include "m_personal_id.h"
#include "m_land_h.h"
#include "m_private.h"
#include "m_diary.h"

#ifdef __cplusplus
extern "C" {
#endif

#define mCD_LAND_PROTECT_CODE_MAGIC0 0x3C1C

enum {
    mCD_SLOT_A,
    mCD_SLOT_B,

    mCD_SLOT_NUM
};

enum {
    mCD_START_COND_0,
    mCD_START_COND_1,
    mCD_START_COND_2,
    mCD_START_COND_INCOMING_FOREIGNER,
    mCD_START_COND_OUTGOING_FOREIGNER,

    mCD_START_COND_NUM
};

enum {
    mCD_ARAM_DATA_ORIGINAL,
    mCD_ARAM_DATA_MAIL,
    mCD_ARAM_DATA_DIARY,

    mCD_ARAM_DATA_NUM
};

/* This is also a priority table where lower = higher priority */
enum {
    mCD_TRANS_ERR_NONE,
    mCD_TRANS_ERR_NONE_NEXTLAND, // leave town
    mCD_TRANS_ERR_IOERROR,
    mCD_TRANS_ERR_DAMAGED,
    mCD_TRANS_ERR_BROKEN_WRONGENCODING,
    mCD_TRANS_ERR_REPAIR,
    mCD_TRANS_ERR_NOT_MEMCARD,
    mCD_TRANS_ERR_WRONG_LAND,
    mCD_TRANS_ERR_INVALID_NOLAND_CODE,
    mCD_TRANS_ERR_LAND_EXIST,
    mCD_TRANS_ERR_PASSPORT_EXIST,
    mCD_TRANS_ERR_NO_FILES,
    mCD_TRANS_ERR_NO_SPACE,
    mCD_TRANS_ERR_TOWN_INVALID,
    mCD_TRANS_ERR_OTHER_TOWN,
    mCD_TRANS_ERR_15,
    mCD_TRANS_ERR_16,
    mCD_TRANS_ERR_CORRUPT,
    mCD_TRANS_ERR_18,
    mCD_TRANS_ERR_TRAVEL_DATA_MISSING,
    mCD_TRANS_ERR_TRAVEL_DATA_EXISTS,
    mCD_TRANS_ERR_WRONGDEVICE,
    mCD_TRANS_ERR_NOCARD,
    mCD_TRANS_ERR_NO_TOWN_DATA,
    mCD_TRANS_ERR_GENERIC,
    mCD_TRANS_ERR_BUSY,

    mCD_TRANS_ERR_NUM
};

#define mCD_RESULT_ERROR -1
#define mCD_RESULT_BUSY 0
#define mCD_RESULT_SUCCESS 1

#define mCD_MEMCARD_SECTORSIZE 0x2000
#define mCD_ALIGN_SECTORSIZE(s) ALIGN_NEXT(s, mCD_MEMCARD_SECTORSIZE)

typedef struct {
    mLd_land_info_c land;
    PersonalID_c pid[PLAYER_NUM];
} mCD_persistent_data_c;

#define mCD_KEEP_ORIGINAL_PAGE_COUNT 8
#define mCD_KEEP_ORIGINAL_COUNT 12
#define mCD_KEEP_ORIGINAL_FOLDER_NAME_LEN 12

typedef struct {
    u16 checksum;
    u16 landid;
    u8 folder_names[mCD_KEEP_ORIGINAL_PAGE_COUNT][mCD_KEEP_ORIGINAL_FOLDER_NAME_LEN];
    mNW_original_design_c original[mCD_KEEP_ORIGINAL_PAGE_COUNT][mCD_KEEP_ORIGINAL_COUNT];
    int _CC80; // force size to 0xCCA0
} mCD_keep_original_c ATTRIBUTE_ALIGN(32);

#define mCD_KEEP_ORIGINAL_SIZE ALIGN_NEXT(sizeof(mCD_keep_original_c), 32)

#define mCD_KEEP_MAIL_PAGE_COUNT 8
#define mCD_KEEP_MAIL_COUNT 20
#define mCD_KEEP_MAIL_FOLDER_NAME_LEN 12

typedef struct {
    u16 checksum;
    u16 landid;
    u8 folder_names[mCD_KEEP_MAIL_PAGE_COUNT][mCD_KEEP_MAIL_FOLDER_NAME_LEN];
    Mail_c mail[mCD_KEEP_MAIL_PAGE_COUNT][mCD_KEEP_MAIL_COUNT];
} mCD_keep_mail_c ATTRIBUTE_ALIGN(32);

#define mCD_KEEP_MAIL_SIZE ALIGN_NEXT(sizeof(mCD_keep_mail_c), 32)

#define mCD_KEEP_DIARY_COUNT PLAYER_NUM
#define mCD_KEEP_DIARY_ENTRY_COUNT lbRTC_MONTHS_MAX

typedef struct {
    u16 checksum;
    mDi_entry_c entries[mCD_KEEP_DIARY_COUNT][mCD_KEEP_DIARY_ENTRY_COUNT];
} mCD_keep_diary_c ATTRIBUTE_ALIGN(32);

#define mCD_KEEP_DIARY_SIZE ALIGN_NEXT(sizeof(mCD_keep_diary_c), 32)

typedef struct {
    u16 checksum;
    Private_c priv;
    Animal_c remove_animal;
    u16 copy_protect;
    u8 _2DEA[54];
} mCD_foreigner_c;

typedef struct {
    u16 code[4];
} mCD_LandProtectCode_c;

typedef struct private_item_keep_s {
    mActor_name_t items[mPr_POCKETS_SLOT_COUNT];
    u8 ticket_expiry_month;
    u8 ticket_storage;
    u32 item_cond;
    u32 wallet;
    mQst_delivery_c delivery[mPr_DELIVERY_QUEST_NUM];
    mQst_errand_c errand[mPr_ERRAND_QUEST_NUM];
    Mail_c mail[mPr_INVENTORY_MAIL_COUNT];
    mPr_catalog_order_c catalog_order[mPr_CATALOG_ORDER_NUM];
    u8 _0FF0;
    Anmremail_c remail;
    mPr_animal_memory_c animal_memory;
} mCD_PrivateItem_c;

extern int mCD_GetThisLandSlotNo_code(int* player_no, s32* slot_card_results);
extern int mCD_GetThisLandSlotNo(void);
extern int mCD_GetSaveHomeSlotNo(void);
extern void mCD_save_data_aram_malloc(void);
extern void mCD_set_aram_save_data();
extern void mCD_init_card();
extern int mCD_InitGameStart_bg(int player_no, int card_private_idx, int start_cond, s32* mounted_chan);
extern int mCD_GetCardPrivateNameCopy(u8* name, int idx);
extern int mCD_CheckCardPlayerNative(int idx);
extern int mCD_CheckPassportFile(void);
extern int mCD_CheckBrokenPassportFile(int slot);
extern int mCD_GetPlayerNum(void);

extern int mCD_CheckStation_bg(s32* chan);
extern int mCD_SaveStation_NextLand_bg(s32* chan);
extern int mCD_SaveStation_Passport_bg(s32* chan);

extern void mCD_PrintErrInfo(gfxprint_t* gfxprint);
extern void mCD_InitAll();
extern void mCD_LoadLand(void);
extern void mCD_toNextLand();

extern int mCD_EraseBrokenLand_bg(int* slot);
extern int mCD_EraseLand_bg(int* slot);
extern int mCD_ErasePassportFile_bg(int slot);
extern int mCD_SaveErasePlayer_bg(int* slot);
extern int mCD_card_format_bg(s32 chan);
extern void mCD_ReCheckLoadLand(GAME_PLAY* play);
extern int mCD_SaveHome_bg(int param_1, int* chan);

extern int mCD_save_data_aram_to_main(void* dst, u32 size, u32 idx);
extern int mCD_save_data_main_to_aram(void* src, u32 size, u32 idx);

struct Save_s;
union save_u;

extern void mCD_InitKeepOriginal(mCD_keep_original_c* keep);
extern void mCD_InitKeepMail(mCD_keep_mail_c* keep);
extern void mCD_InitKeepDiary(mCD_keep_diary_c* keep);
extern void mCD_ClearForeignerFile(mCD_foreigner_c* foreigner);
extern void mCD_SetForeignerFile(mCD_foreigner_c* foreigner, Private_c* priv, Animal_c* animal);
extern u16 mCD_SetForeignerCopyProtect(mCD_foreigner_c* passport);
extern void mCD_MakeProtectCode(mCD_LandProtectCode_c* protect_code);
extern int mCD_CheckInitProtectCode(const mCD_LandProtectCode_c* protect_code);
extern int mCD_CheckProtectCode(const mCD_LandProtectCode_c* protect_code);
extern int mCD_CompNoLandCode(const mCD_LandProtectCode_c* code0, const mCD_LandProtectCode_c* code1);
extern u16 mCD_GetLandCopyProtect(void);
extern void mCD_SealSave(union save_u* save);
extern int mCD_CheckResetCode(Private_c* priv);
extern void mCD_SetResetCode(Private_c* priv);
extern void mCD_SaveHome_SetData(int mode);
extern void mCD_SaveHome_Written(int mode);
extern void mCD_GameStart_SetDecoy(void);
extern int mCD_GameStart_MakeData(int player_no, int start_cond, mCD_foreigner_c* foreigner,
                                  const mCD_foreigner_c* card_passport, int* passport_mode);
extern void mCD_Travel_SetData(mCD_foreigner_c* foreigner, mCD_PrivateItem_c* keep, int* stashed, int record_land);
extern void mCD_Travel_Undo(mCD_PrivateItem_c* keep, int stashed);
extern void mCD_ApplyNextLand(const union save_u* save, mCD_foreigner_c* foreigner);

#ifdef __cplusplus
}
#endif

#endif
