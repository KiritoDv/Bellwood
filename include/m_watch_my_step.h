#ifndef M_WATCH_MY_STEP_H
#define M_WATCH_MY_STEP_H

#include "types.h"
#include "m_play.h"
#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif




extern void watch_my_step_ct();
extern void watch_my_step_move(GAME_PLAY* play);
extern void watch_my_step_draw(GAME_PLAY* play);

extern void navigate_camera_ct();
extern void navigate_camera_move(GAME_PLAY* play);
extern void navigate_camera_draw(GAME_PLAY* play);

extern void mWt_mybell_confirmation_ct();
extern void mWt_mybell_confirmation_move(GAME_PLAY* play);
extern void mWt_mybell_confirmation_draw(GAME_PLAY* play);

#ifdef __cplusplus
}
#endif

#endif
