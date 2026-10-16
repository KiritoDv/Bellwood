#pragma once

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NesCore NesCore;

#define NES_SCREEN_WIDTH 256
#define NES_SCREEN_HEIGHT 240

#define NES_SRAM_SIZE 0x8000
#define NES_FDS_BIOS_SIZE 0x2000
#define NES_QD_SIDE_SIZE 0x10000

enum {
    NES_BUTTON_A = 0x01,
    NES_BUTTON_B = 0x02,
    NES_BUTTON_SELECT = 0x04,
    NES_BUTTON_START = 0x08,
    NES_BUTTON_UP = 0x10,
    NES_BUTTON_DOWN = 0x20,
    NES_BUTTON_LEFT = 0x40,
    NES_BUTTON_RIGHT = 0x80,
};

enum {
    NES_LOAD_OK = 0,
    NES_LOAD_BAD_IMAGE = -1,
    NES_LOAD_UNSUPPORTED_MAPPER = -2,
    NES_LOAD_NO_BIOS = -3,
};

NesCore* NesCore_Create(void);
void NesCore_Destroy(NesCore* core);

int NesCore_Load(NesCore* core, uint8_t* image, size_t size, uint8_t* sram, const uint8_t* bios);
int NesCore_GetMapper(const NesCore* core);
int NesCore_IsDiskSystem(const NesCore* core);

void NesCore_Reset(NesCore* core, int hard);
void NesCore_SetButtons(NesCore* core, int port, uint8_t buttons);
void NesCore_RunFrame(NesCore* core, int audio);

const uint16_t* NesCore_GetFrame(const NesCore* core);
uint8_t* NesCore_GetRam(NesCore* core);

int NesCore_DiskBusy(const NesCore* core);
void NesCore_SyncDisk(NesCore* core);

#ifdef __cplusplus
}
#endif
