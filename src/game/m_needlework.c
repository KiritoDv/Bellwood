#include "ac_asset.h"
#include "m_needlework.h"
#include "ac_port.h"

#include "m_string.h"
#include "m_private.h"
#include "m_common_data.h"
#include "jsyswrap.h"
#include "libultra/libultra.h"
#include "m_lib.h"

/**
 * @brief Initializes all designs' palette indexes for a player.
 *
 * @param player_no The index of the player whose designs will be updated
 **/
static void mNW_InitMyOriginalPallet(int player_no) {
    static u8 pal_table[mPr_ORIGINAL_DESIGN_COUNT] = { 0, 8, 7, 7, 0, 0, 0, 0 };

    int i;

    for (i = 0; i < mPr_ORIGINAL_DESIGN_COUNT; i++) {
        Save_Set(private_data[player_no & 3].my_org[i & 7].palette, pal_table[i]);
    }
}

/**
 * @brief Initializes all designs' names for a player.
 *
 * @param player_no The index of the player whose designs will be renamed
 **/
static void mNW_InitMyOriginalName(int player_no) {
    int i;

    player_no &= 3;
    for (i = 0; i < mPr_ORIGINAL_DESIGN_COUNT; i++) {
        /* TODO: define for the string index? */
        mString_Load_StringFromRom(Save_Get(private_data[player_no].my_org[i & 7].name), mNW_ORIGINAL_DESIGN_NAME_LEN,
                                   0x6DF + i);
    }
}

/**
 * @brief Initializes all designs' textures for a player.
 *
 * @param player_no The index of the player whose designs will be updated
 **/
static void mNW_InitMyOriginalTexture(int player_no) {
    int i;

    player_no &= 3;
    for (i = 0; i < mNW_DEFAULT_ORIGINAL_TEX_NUM; i++) {
        _JW_GetResourceAram(JW_GetAramAddress(27) + i * mNW_DESIGN_TEX_SIZE,
                            Save_Get(private_data[player_no].my_org[i & 7].design.data), mNW_DESIGN_TEX_SIZE);
    }
}

/**
 * @brief Initializes all designs for all players.
 **/
extern void mNW_InitMyOriginal() {
    int i;

    for (i = 0; i < PLAYER_NUM; i++) {
        mNW_InitOneMyOriginal(i);
    }
}

/**
 * @brief Initializes a single player's designs.
 *
 * @param player_no The index of the player whose designs will be initialized.
 **/
extern void mNW_InitOneMyOriginal(int player_no) {
    int i;

    mNW_InitMyOriginalPallet(player_no & 3);
    mNW_InitMyOriginalName(player_no & 3);
    mNW_InitMyOriginalTexture(player_no & 3);

    for (i = mNW_DEFAULT_ORIGINAL_TEX_NUM; i < mPr_ORIGINAL_DESIGN_COUNT; i++) {
        mNW_InitOriginalData(Save_Get(private_data[player_no].my_org + (i & 7)));
    }
}

/**
 * @brief Copies a default design texture into the supplied buffer.
 *
 * @param dst The destination design texture buffer
 * @param tex_no The index of the texture to retrieve (0-7)
 **/
static void mNW_CopyNeedleworkDefaultTexture(u8* dst, int tex_no) {
    u32 addr = JW_GetAramAddress(28);
    _JW_GetResourceAram(addr + (tex_no & 7) * mNW_DESIGN_TEX_SIZE, dst, mNW_DESIGN_TEX_SIZE);
}

/**
 * @brief Initializes all Able Sisters' designs to their default textures.
 **/
static void mNW_InitNeedleworkTexture() {
    int i;

    for (i = 0; i < mNW_TOTAL_DESIGN_NUM; i++) {
        mNW_CopyNeedleworkDefaultTexture(Save_Get(needlework.original_design[i & 7].design.data), i);
    }
}

/**
 * @brief Initializes all Able Sisters' designs to their default palettes.
 **/
static void mNW_InitNeedleworkPelatteNo() {
    static u8 pal_table[mNW_TOTAL_DESIGN_NUM] = { 7, 1, 10, 3, 6, 0, 6, 7 };

    int i;

    for (i = 0; i < mNW_TOTAL_DESIGN_NUM; i++) {
        Save_Set(needlework.original_design[i & 7].palette, pal_table[i]);
    }
}

/**
 * @brief Initializes all Able Sisters' designs to their default names.
 **/
static void mNW_InitNeedleworkTextureName() {
    int i;

    for (i = 0; i < mNW_TOTAL_DESIGN_NUM; i++) {
        mString_Load_StringFromRom(Save_Get(needlework.original_design[i & 7].name), mNW_ORIGINAL_DESIGN_NAME_LEN,
                                   0x6E7 + i);
    }
}

/**
 * @brief Initializes all Able Sisters' designs to their default data.
 **/
extern void mNW_InitNeedleworkData() {
    mNW_InitNeedleworkTexture();
    mNW_InitNeedleworkPelatteNo();
    mNW_InitNeedleworkTextureName();
}

/**
 * @brief Converts a design texture from GBA format to GC format.
 *
 * @param agb Pointer to the source GBA texture
 * @param gc Pointer to the destination GC texture
 **/
extern void mNW_AGB_to_GC_texture(u8* agb, u8* gc) {
    int i;

    for (i = 0; i < mNW_DESIGN_TEX_SIZE; i++) {
        u8 src = *agb++;
        *gc++ = (((src & 0xF0) >> 4) | ((src & 0xF) << 4));
    }
}

/**
 * @brief Converts a design texture from GC format to GBA format.
 *
 * @param gc Pointer to the source GC texture
 * @param agb Pointer to the destination GBA texture
 **/
extern void mNW_GC_to_Agb_texture(u8* gc, u8* agb) {
    mNW_AGB_to_GC_texture(gc, agb);
}

#define mNW_PALETTE_COLOR(argb8) \
    AC_TLUT_HOST16(GPACK_RGBA5551(((argb8) >> 16) & 0xFF, ((argb8) >> 8) & 0xFF, (argb8) & 0xFF, 1))

/* These are aligned to 32 bytes for transfering to-and-from ARAM */

static u16 needlework0_pal[16] ATTRIBUTE_ALIGN(32) = {
    mNW_PALETTE_COLOR(0xFF525252), mNW_PALETTE_COLOR(0xFFCE4A4A), mNW_PALETTE_COLOR(0xFFDE8442), mNW_PALETTE_COLOR(0xFFE7AD18),
    mNW_PALETTE_COLOR(0xFFE7C621), mNW_PALETTE_COLOR(0xFFD6DE18), mNW_PALETTE_COLOR(0xFFB5E718), mNW_PALETTE_COLOR(0xFF84D652),
    mNW_PALETTE_COLOR(0xFF39C66B), mNW_PALETTE_COLOR(0xFF29ADC6), mNW_PALETTE_COLOR(0xFF427BEF), mNW_PALETTE_COLOR(0xFF6B4AE7),
    mNW_PALETTE_COLOR(0xFF945ACE), mNW_PALETTE_COLOR(0xFFBD42B5), mNW_PALETTE_COLOR(0xFF000000), mNW_PALETTE_COLOR(0xFFFFFFFF),
};

static u16 needlework1_pal[16] ATTRIBUTE_ALIGN(32) = {
    mNW_PALETTE_COLOR(0xFF525252), mNW_PALETTE_COLOR(0xFFFF8C8C), mNW_PALETTE_COLOR(0xFFFFCE84), mNW_PALETTE_COLOR(0xFFFFE75A),
    mNW_PALETTE_COLOR(0xFFFFF763), mNW_PALETTE_COLOR(0xFFFFFF84), mNW_PALETTE_COLOR(0xFFDEFF52), mNW_PALETTE_COLOR(0xFFB5FF84),
    mNW_PALETTE_COLOR(0xFF7BF7AD), mNW_PALETTE_COLOR(0xFF63E7F7), mNW_PALETTE_COLOR(0xFF84C6FF), mNW_PALETTE_COLOR(0xFFA59CFF),
    mNW_PALETTE_COLOR(0xFFD69CFF), mNW_PALETTE_COLOR(0xFFFF9CF7), mNW_PALETTE_COLOR(0xFF8C8C8C), mNW_PALETTE_COLOR(0xFFFFFFFF),
};

static u16 needlework2_pal[16] ATTRIBUTE_ALIGN(32) = {
    mNW_PALETTE_COLOR(0xFF525252), mNW_PALETTE_COLOR(0xFF9C1818), mNW_PALETTE_COLOR(0xFFAD5208), mNW_PALETTE_COLOR(0xFFB57B00),
    mNW_PALETTE_COLOR(0xFFB59400), mNW_PALETTE_COLOR(0xFFA5AD00), mNW_PALETTE_COLOR(0xFF84B500), mNW_PALETTE_COLOR(0xFF52A531),
    mNW_PALETTE_COLOR(0xFF089439), mNW_PALETTE_COLOR(0xFF007B94), mNW_PALETTE_COLOR(0xFF104ABD), mNW_PALETTE_COLOR(0xFF3918AD),
    mNW_PALETTE_COLOR(0xFF5A2994), mNW_PALETTE_COLOR(0xFF8C087B), mNW_PALETTE_COLOR(0xFF080808), mNW_PALETTE_COLOR(0xFFFFFFFF),
};

static u16 needlework3_pal[16] ATTRIBUTE_ALIGN(32) = {
    mNW_PALETTE_COLOR(0xFF525252), mNW_PALETTE_COLOR(0xFF42945A), mNW_PALETTE_COLOR(0xFF73C68C), mNW_PALETTE_COLOR(0xFF94E7AD),
    mNW_PALETTE_COLOR(0xFF008C7B), mNW_PALETTE_COLOR(0xFF5AB5AD), mNW_PALETTE_COLOR(0xFF84C6C6), mNW_PALETTE_COLOR(0xFF2173A5),
    mNW_PALETTE_COLOR(0xFF4A9CCE), mNW_PALETTE_COLOR(0xFF6BADDE), mNW_PALETTE_COLOR(0xFF7384BD), mNW_PALETTE_COLOR(0xFF6B73AD),
    mNW_PALETTE_COLOR(0xFF525294), mNW_PALETTE_COLOR(0xFF39397B), mNW_PALETTE_COLOR(0xFF181863), mNW_PALETTE_COLOR(0xFFFFFFFF),
};

static u16 needlework4_pal[16] ATTRIBUTE_ALIGN(32) = {
    mNW_PALETTE_COLOR(0xFF525252), mNW_PALETTE_COLOR(0xFF9C8452), mNW_PALETTE_COLOR(0xFFBD945A), mNW_PALETTE_COLOR(0xFFD6BD84),
    mNW_PALETTE_COLOR(0xFF9C5252), mNW_PALETTE_COLOR(0xFFCE7363), mNW_PALETTE_COLOR(0xFFEF9C8C), mNW_PALETTE_COLOR(0xFF8C6384),
    mNW_PALETTE_COLOR(0xFFA584B5), mNW_PALETTE_COLOR(0xFFDEB5DE), mNW_PALETTE_COLOR(0xFFBD8484), mNW_PALETTE_COLOR(0xFFAD736B),
    mNW_PALETTE_COLOR(0xFF945252), mNW_PALETTE_COLOR(0xFF7B3939), mNW_PALETTE_COLOR(0xFF631810), mNW_PALETTE_COLOR(0xFFFFFFFF),
};

static u16 needlework5_pal[16] ATTRIBUTE_ALIGN(32) = {
    mNW_PALETTE_COLOR(0xFF525252), mNW_PALETTE_COLOR(0xFFEF5A00), mNW_PALETTE_COLOR(0xFFFF9C42), mNW_PALETTE_COLOR(0xFFFFCE84),
    mNW_PALETTE_COLOR(0xFFFFEFA5), mNW_PALETTE_COLOR(0xFF8C4A29), mNW_PALETTE_COLOR(0xFFB57B5A), mNW_PALETTE_COLOR(0xFFE7AD8C),
    mNW_PALETTE_COLOR(0xFFFFDEBD), mNW_PALETTE_COLOR(0xFF318CFF), mNW_PALETTE_COLOR(0xFF63B5FF), mNW_PALETTE_COLOR(0xFF9CDEFF),
    mNW_PALETTE_COLOR(0xFFC6E7FF), mNW_PALETTE_COLOR(0xFF6B6B6B), mNW_PALETTE_COLOR(0xFF000000), mNW_PALETTE_COLOR(0xFFFFFFFF),
};

static u16 needlework6_pal[16] ATTRIBUTE_ALIGN(32) = {
    mNW_PALETTE_COLOR(0xFF525252), mNW_PALETTE_COLOR(0xFF39B542), mNW_PALETTE_COLOR(0xFF63DE5A), mNW_PALETTE_COLOR(0xFF8CEF84),
    mNW_PALETTE_COLOR(0xFFB5FFAD), mNW_PALETTE_COLOR(0xFF2121C6), mNW_PALETTE_COLOR(0xFF5252F7), mNW_PALETTE_COLOR(0xFF8484FF),
    mNW_PALETTE_COLOR(0xFFB5B5FF), mNW_PALETTE_COLOR(0xFFCE3939), mNW_PALETTE_COLOR(0xFFDE6B6B), mNW_PALETTE_COLOR(0xFFE78C9C),
    mNW_PALETTE_COLOR(0xFFEFBDBD), mNW_PALETTE_COLOR(0xFF6B6B6B), mNW_PALETTE_COLOR(0xFF000000), mNW_PALETTE_COLOR(0xFFFFFFFF),
};

static u16 needlework7_pal[16] ATTRIBUTE_ALIGN(32) = {
    mNW_PALETTE_COLOR(0xFF525252), mNW_PALETTE_COLOR(0xFF082100), mNW_PALETTE_COLOR(0xFF425A39), mNW_PALETTE_COLOR(0xFF6B8463),
    mNW_PALETTE_COLOR(0xFF9CB594), mNW_PALETTE_COLOR(0xFF5A2900), mNW_PALETTE_COLOR(0xFF7B4A21), mNW_PALETTE_COLOR(0xFFA5734A),
    mNW_PALETTE_COLOR(0xFFD6A57B), mNW_PALETTE_COLOR(0xFF947B00), mNW_PALETTE_COLOR(0xFFB59439), mNW_PALETTE_COLOR(0xFFCEB56B),
    mNW_PALETTE_COLOR(0xFFDED69C), mNW_PALETTE_COLOR(0xFF6B6B6B), mNW_PALETTE_COLOR(0xFF000000), mNW_PALETTE_COLOR(0xFFFFFFFF),
};

static u16 needlework8_pal[16] ATTRIBUTE_ALIGN(32) = {
    mNW_PALETTE_COLOR(0xFF525252), mNW_PALETTE_COLOR(0xFF2121FF), mNW_PALETTE_COLOR(0xFFFF2121), mNW_PALETTE_COLOR(0xFFD6D600),
    mNW_PALETTE_COLOR(0xFF6363FF), mNW_PALETTE_COLOR(0xFFFF6363), mNW_PALETTE_COLOR(0xFFD6D663), mNW_PALETTE_COLOR(0xFF9494FF),
    mNW_PALETTE_COLOR(0xFFFF9494), mNW_PALETTE_COLOR(0xFFD6D694), mNW_PALETTE_COLOR(0xFFADADFF), mNW_PALETTE_COLOR(0xFFFFADAD),
    mNW_PALETTE_COLOR(0xFFE7E7AD), mNW_PALETTE_COLOR(0xFF6B6B6B), mNW_PALETTE_COLOR(0xFF000000), mNW_PALETTE_COLOR(0xFFFFFFFF),
};

static u16 needlework9_pal[16] ATTRIBUTE_ALIGN(32) = {
    mNW_PALETTE_COLOR(0xFF525252), mNW_PALETTE_COLOR(0xFF21A521), mNW_PALETTE_COLOR(0xFF39ADFF), mNW_PALETTE_COLOR(0xFF9C52EF),
    mNW_PALETTE_COLOR(0xFF52BD52), mNW_PALETTE_COLOR(0xFF5AC6FF), mNW_PALETTE_COLOR(0xFFB59CFF), mNW_PALETTE_COLOR(0xFF6BD673),
    mNW_PALETTE_COLOR(0xFF8CE7FF), mNW_PALETTE_COLOR(0xFFCEB5FF), mNW_PALETTE_COLOR(0xFF94DEAD), mNW_PALETTE_COLOR(0xFFBDF7FF),
    mNW_PALETTE_COLOR(0xFFD6CEFF), mNW_PALETTE_COLOR(0xFF6B6B6B), mNW_PALETTE_COLOR(0xFF000000), mNW_PALETTE_COLOR(0xFFFFFFFF),
};

static u16 needlework10_pal[16] ATTRIBUTE_ALIGN(32) = {
    mNW_PALETTE_COLOR(0xFF525252), mNW_PALETTE_COLOR(0xFFD60000), mNW_PALETTE_COLOR(0xFFFFBD00), mNW_PALETTE_COLOR(0xFFEFF731),
    mNW_PALETTE_COLOR(0xFF4ACE42), mNW_PALETTE_COLOR(0xFF299C29), mNW_PALETTE_COLOR(0xFF528CBD), mNW_PALETTE_COLOR(0xFF424AAD),
    mNW_PALETTE_COLOR(0xFF9452D6), mNW_PALETTE_COLOR(0xFFF77BDE), mNW_PALETTE_COLOR(0xFFA59439), mNW_PALETTE_COLOR(0xFF9C4242),
    mNW_PALETTE_COLOR(0xFF5A3139), mNW_PALETTE_COLOR(0xFF6B6B6B), mNW_PALETTE_COLOR(0xFF000000), mNW_PALETTE_COLOR(0xFFFFFFFF),
};

static u16 needlework11_pal[16] ATTRIBUTE_ALIGN(32) = {
    mNW_PALETTE_COLOR(0xFF525252), mNW_PALETTE_COLOR(0xFFE7CE18), mNW_PALETTE_COLOR(0xFF21C618), mNW_PALETTE_COLOR(0xFFFF6B00),
    mNW_PALETTE_COLOR(0xFF0000FF), mNW_PALETTE_COLOR(0xFF9400BD), mNW_PALETTE_COLOR(0xFFE7CE18), mNW_PALETTE_COLOR(0xFF00A500),
    mNW_PALETTE_COLOR(0xFFCE4200), mNW_PALETTE_COLOR(0xFF0000D6), mNW_PALETTE_COLOR(0xFF5A008C), mNW_PALETTE_COLOR(0xFF9C8C18),
    mNW_PALETTE_COLOR(0xFF008400), mNW_PALETTE_COLOR(0xFFA52100), mNW_PALETTE_COLOR(0xFF0000A5), mNW_PALETTE_COLOR(0xFF4A005A),
};

static u16 needlework12_pal[16] ATTRIBUTE_ALIGN(32) = {
    mNW_PALETTE_COLOR(0xFF525252), mNW_PALETTE_COLOR(0xFFFF2121), mNW_PALETTE_COLOR(0xFFE7D600), mNW_PALETTE_COLOR(0xFFF739BD),
    mNW_PALETTE_COLOR(0xFF00D69C), mNW_PALETTE_COLOR(0xFF107310), mNW_PALETTE_COLOR(0xFFC62121), mNW_PALETTE_COLOR(0xFFBDA500),
    mNW_PALETTE_COLOR(0xFFCE3994), mNW_PALETTE_COLOR(0xFF009C6B), mNW_PALETTE_COLOR(0xFF214A21), mNW_PALETTE_COLOR(0xFF8C2121),
    mNW_PALETTE_COLOR(0xFF846B00), mNW_PALETTE_COLOR(0xFF941863), mNW_PALETTE_COLOR(0xFF00734A), mNW_PALETTE_COLOR(0xFF183918),
};

static u16 needlework13_pal[16] ATTRIBUTE_ALIGN(32) = {
    mNW_PALETTE_COLOR(0xFF525252), mNW_PALETTE_COLOR(0xFFEFD6D6), mNW_PALETTE_COLOR(0xFFDEC6C6), mNW_PALETTE_COLOR(0xFFCEB5B5),
    mNW_PALETTE_COLOR(0xFFBDA5A5), mNW_PALETTE_COLOR(0xFFAD9494), mNW_PALETTE_COLOR(0xFF9C8484), mNW_PALETTE_COLOR(0xFF8C7373),
    mNW_PALETTE_COLOR(0xFF7B6363), mNW_PALETTE_COLOR(0xFF6B5252), mNW_PALETTE_COLOR(0xFF5A4242), mNW_PALETTE_COLOR(0xFF4A3131),
    mNW_PALETTE_COLOR(0xFF392121), mNW_PALETTE_COLOR(0xFF291010), mNW_PALETTE_COLOR(0xFF180000), mNW_PALETTE_COLOR(0xFF100000),
};

static u16 needlework14_pal[16] ATTRIBUTE_ALIGN(32) = {
    mNW_PALETTE_COLOR(0xFF525252), mNW_PALETTE_COLOR(0xFFEFEFEF), mNW_PALETTE_COLOR(0xFFDEDEDE), mNW_PALETTE_COLOR(0xFFCECECE),
    mNW_PALETTE_COLOR(0xFFBDBDBD), mNW_PALETTE_COLOR(0xFFADADAD), mNW_PALETTE_COLOR(0xFF9C9C9C), mNW_PALETTE_COLOR(0xFF8C8C8C),
    mNW_PALETTE_COLOR(0xFF7B7B7B), mNW_PALETTE_COLOR(0xFF6B6B6B), mNW_PALETTE_COLOR(0xFF5A5A5A), mNW_PALETTE_COLOR(0xFF4A4A4A),
    mNW_PALETTE_COLOR(0xFF393939), mNW_PALETTE_COLOR(0xFF292929), mNW_PALETTE_COLOR(0xFF181818), mNW_PALETTE_COLOR(0xFF101010),
};

static u16 needlework15_pal[16] ATTRIBUTE_ALIGN(32) = {
    mNW_PALETTE_COLOR(0xFF525252), mNW_PALETTE_COLOR(0xFFEF7B7B), mNW_PALETTE_COLOR(0xFFD61818), mNW_PALETTE_COLOR(0xFFF79418),
    mNW_PALETTE_COLOR(0xFFE7E752), mNW_PALETTE_COLOR(0xFF006B00), mNW_PALETTE_COLOR(0xFF39B539), mNW_PALETTE_COLOR(0xFF0039B5),
    mNW_PALETTE_COLOR(0xFF399CFF), mNW_PALETTE_COLOR(0xFF940094), mNW_PALETTE_COLOR(0xFFFF6BFF), mNW_PALETTE_COLOR(0xFF944208),
    mNW_PALETTE_COLOR(0xFFEF9C5A), mNW_PALETTE_COLOR(0xFFFFC694), mNW_PALETTE_COLOR(0xFF000000), mNW_PALETTE_COLOR(0xFFFFFFFF),
};

static u16* mNW_needlework_pallet_table[mNW_PALETTE_COUNT] = {
    needlework0_pal,  needlework1_pal,  needlework2_pal,  needlework3_pal, needlework4_pal,  needlework5_pal,
    needlework6_pal,  needlework7_pal,  needlework8_pal,  needlework9_pal, needlework10_pal, needlework11_pal,
    needlework12_pal, needlework13_pal, needlework14_pal, needlework15_pal
};

/**
 * @brief Gets a pointer to the requested palette color data.
 *
 * @param palette_idx Index of the palette to retrieve (0-15)
 * @return Palette color data pointer
 **/
extern u16* mNW_PaletteIdx2Palette(int palette_idx) {
    return mNW_needlework_pallet_table[palette_idx & 15];
}

/**
 * @brief Copies one design class to another and updates data cache.
 *
 * @param dst Destination design class
 * @param src Source design class
 **/
extern void mNW_CopyOriginalTextureClass(mNW_original_design_c* dst, mNW_original_design_c* src) {
    bcopy(src, dst, sizeof(mNW_original_design_c));
    AC_TexInvalidateRange(dst->design.data, mNW_DESIGN_TEX_SIZE);
}

/**
 * @brief Copies a design's texture data to a block of memory and updates data cache.
 *
 * @param dst Destination pointer
 * @param src Source design class
 **/
extern void mNW_CopyOriginalTexture(void* dst, mNW_original_design_c* src) {
    if (dst != NULL) {
        bcopy(src->design.data, dst, mNW_DESIGN_TEX_SIZE);
        AC_TexInvalidateRange(dst, mNW_DESIGN_TEX_SIZE);
    }
}

/**
 * @brief Copies a design's palette colors to a block of memory and updates data cache.
 *
 * @param dst Destination pointer
 * @param src Source design class
 **/
extern void mNW_CopyOriginalPalette(void* dst, mNW_original_design_c* src) {
    if (dst != NULL) {
        bcopy(mNW_PaletteIdx2Palette(src->palette), dst, 16 * sizeof(u16));
        AC_TexInvalidateRange(dst, 16 * sizeof(u16));
    }
}

/**
 * @brief Copies a block of memory to a design's texture data and updates data cache.
 *
 * @param dst The destination design class
 * @param src Source data pointer
 **/
extern void mNW_OverWriteOriginalTexture(mNW_original_design_c* dst, u8* src) {
    if (src != NULL) {
        u8* tex = dst->design.data;
        bcopy(src, tex, mNW_DESIGN_TEX_SIZE);
        AC_TexInvalidateRange(tex, mNW_DESIGN_TEX_SIZE);
    }
}

/**
 * @brief Copies a block of memory to a design's name.
 *
 * @param dst The destination design class
 * @param src Source name pointer
 **/
extern void mNW_OverWriteOriginalName(mNW_original_design_c* dst, u8* src) {
    if (src != NULL) {
        bcopy(src, dst->name, mNW_ORIGINAL_DESIGN_NAME_LEN);
    }
}

/* swap buffer for original design */
static mNW_original_design_c mNW_swap_work;

/**
 * @brief Swaps two design classes' data in memory and updates data cache.
 *
 * @param org0 First design class
 * @param org1 Second design class
 **/
extern void mNW_SwapOriginalData(mNW_original_design_c* org0, mNW_original_design_c* org1) {
    bcopy(org0, &mNW_swap_work, sizeof(mNW_original_design_c));
    bcopy(org1, org0, sizeof(mNW_original_design_c));
    bcopy(&mNW_swap_work, org1, sizeof(mNW_original_design_c));
    AC_TexInvalidateRange(org0->design.data, mNW_DESIGN_TEX_SIZE);
    AC_TexInvalidateRange(org1->design.data, mNW_DESIGN_TEX_SIZE);
}

/**
 * @brief Initializes a design class to 'blank' data.
 *
 * @param design The design which will be initialized
 **/
extern void mNW_InitOriginalData(mNW_original_design_c* design) {
    static u8 name[mNW_ORIGINAL_DESIGN_NAME_LEN] = "blank           ";

    u8* tex;
    mNW_OverWriteOriginalName(design, name);
    tex = design->design.data;
    mem_clear(tex, mNW_DESIGN_TEX_SIZE, 0xFF);
    AC_TexInvalidateRange(tex, mNW_DESIGN_TEX_SIZE);
    design->palette = 0;
}

/**
 * @brief Initializes a player's design related data to the default state.
 *
 * @param player_no The index of the player whose design data will be initialized
 **/
extern void mNW_InitPrivateOriginalData(int player_no) {
    int i;
    Private_c* private = Save_Get(private_data + player_no);

    for (i = 0; i < mPr_ORIGINAL_DESIGN_COUNT; i++) {
        private->my_org_no_table[i] = i;
    }

    for (i = 0; i < mPr_ORIGINAL_DESIGN_COUNT; i++) {
        mNW_InitOriginalData(private->my_org + i);
    }
}
