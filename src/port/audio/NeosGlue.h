#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define NA_PORT_FRAME_SAMPLES 560

int NaPort_Boot(void);
int NaPort_RenderFrame(int16_t* out);
void NaPort_SetVolumes(float music, float sfx);
void NaPort_Shutdown(void);

#ifdef __cplusplus
}
#endif
