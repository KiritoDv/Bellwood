#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void AC_AudioInit(void);
void AC_AudioFrame(int gameFrame);
void AC_AudioShutdown(void);

#ifdef __cplusplus
}
#endif
