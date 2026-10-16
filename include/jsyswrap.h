#ifndef JSYSWRAP_H
#define JSYSWRAP_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CResTIMG {
	u8 mTextureFormat;              // _00
	u8 mTransparency; // _01
	u16 mSizeX;                     // _02
	u16 mSizeY;                     // _04
	u8 mWrapS;                      // _06
	u8 mWrapT;                      // _07
	u8 mPaletteFormat;              // _08
	u8 mColorFormat;                // _09
	u16 mPaletteEntryCount;         // _0A
	u32 mPaletteOffset;             // _0C
	u8 mIsMIPmapEnabled;            // _10
	u8 mDoEdgeLOD;                  // _11
	u8 mIsBiasClamp;                // _12
	u8 mIsMaxAnisotropy;            // _13
	u8 mMinFilterType;              // _14
	u8 mMagFilterType;              // _15
	s8 mMinLOD;                     // _16
	s8 mMaxLOD;                     // _17
	u8 mTotalImageCount;            // _18
	u8 _19;                         // _19, unknown
	s16 mLODBias;                   // _1A
	u32 mImageDataOffset;           // _1C
} CResTIMG;

enum resource_index {
    RESOURCE_FGDATA,
    RESOURCE_MAIL,
    RESOURCE_MAIL_TABLE,
    RESOURCE_MAILA,
    RESOURCE_MAILA_TABLE,
    RESOURCE_MAILB,
    RESOURCE_MAILB_TABLE,
    RESOURCE_MAILC,
    RESOURCE_MAILC_TABLE,
    RESOURCE_PALLET_BOY,
    RESOURCE_PS,
    RESOURCE_PS_TABLE,
    RESOURCE_PSZ,
    RESOURCE_PSZ_TABLE,
    RESOURCE_SELECT,
    RESOURCE_SELECT_TABLE,
    RESOURCE_STRING,
    RESOURCE_STRING_TABLE,
    RESOURCE_SUPERZ,
    RESOURCE_SUPERZ_TABLE,
    RESOURCE_SUPER,
    RESOURCE_SUPER_TABLE,
    RESOURCE_TEX_BOY,
    RESOURCE_FACE_BOY,
    RESOURCE_FGNPCDATA,
    RESOURCE_MESSAGE,
    RESOURCE_MESSAGE_TABLE,
    RESOURCE_MY_ORIGINAL,
    RESOURCE_NEEDLEWORK_JOYBOOT,
    RESOURCE_PLAYER_ROOM_FLOOR,
    RESOURCE_PLAYER_ROOM_WALL,
    RESOURCE_NPC_NAME_STR_TABLE,
    RESOURCE_D_OBJ_NPC_STOCK_SCH,
    RESOURCE_D_OBJ_NPC_STOCK_SCL,
    RESOURCE_TITLE,
    RESOURCE_MURA_SPRING,
    RESOURCE_MURA_SUMMER,
    RESOURCE_MURA_FALL,
    RESOURCE_MURA_WINTER,
    RESOURCE_ODEKAKE,
    RESOURCE_OMAKE,
    RESOURCE_EKI1,
    RESOURCE_EKI1_2,
    RESOURCE_EKI1_3,
    RESOURCE_EKI1_4,
    RESOURCE_EKI1_5,
    RESOURCE_EKI2,
    RESOURCE_EKI2_2,
    RESOURCE_EKI2_3,
    RESOURCE_EKI2_4,
    RESOURCE_EKI2_5,
    RESOURCE_EKI3,
    RESOURCE_EKI3_2,
    RESOURCE_EKI3_3,
    RESOURCE_EKI3_4,
    RESOURCE_EKI3_5,
    RESOURCE_TEGAMI,
    RESOURCE_TEGAMI2,
    RESOURCE_FAMIKON,
    RESOURCE_BOY1,
    RESOURCE_BOY2,
    RESOURCE_BOY3,
    RESOURCE_BOY4,
    RESOURCE_BOY5,
    RESOURCE_BOY6,
    RESOURCE_BOY7,
    RESOURCE_BOY8,
    RESOURCE_GIRL1,
    RESOURCE_GIRL2,
    RESOURCE_GIRL3,
    RESOURCE_GIRL4,
    RESOURCE_GIRL5,
    RESOURCE_GIRL6,
    RESOURCE_GIRL7,
    RESOURCE_GIRL8,
    RESOURCE_D_BG_ISLAND_SCH,

    RESOURCE_NUM
};

extern u32 JW_GetAramAddress(int res_no);
extern u8* _JW_GetResourceAram(u32 aram_addr, u8* dst, u32 size);
extern u32 JW_GetResSizeFileNo(int res_no);

#ifdef __cplusplus
}
#endif

#endif
