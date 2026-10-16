#ifndef FAMICOM_PRIV_H
#define FAMICOM_PRIV_H

#include "types.h"
#include "Famicom/famicom.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FAMICOM_SAVE_SINGLE_SIZE 0x660
#define FAMICOM_SAVE_TOTAL_SIZE (FAMICOM_SAVE_HEADER_SIZE + FAMICOM_SAVE_SINGLE_SIZE * 4)

typedef struct FamicomNesState {
    u8* wram;
} FamicomNesState;

typedef struct FamicomCommon {
    FamicomNesState* sp;
    u8* nesromp;
    size_t nesrom_size;
    u8* bbramp;
    u8* highscore_flagsp;
    u8 rom_no;
    s8 save_pl_no;
    FamicomSaveDataHeader* save_data_header;
    u8* internal_save_datap;
    size_t save_data_total_size;
    size_t save_data_single_size;
    const u8* save_data_name;
} FamicomCommon;

extern FamicomCommon famicomCommon;
extern u8 famicomCommonSave[FAMICOM_SAVE_TOTAL_SIZE];

extern u32 nesinfo_tags_size;
extern u8* nesinfo_tags_start;
extern u8* nesinfo_tags_end;

extern u32 nesinfo_data_size;
extern u8* nesinfo_data_start;
extern u8* nesinfo_data_end;

extern u8 tcs_bad;
extern u8 ics_bad;

int FamicomPort_ReadSave(void* data, size_t size);
int FamicomPort_WriteSave(const void* data, size_t size);

#ifdef __cplusplus
}
#endif

#endif
