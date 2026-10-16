#include "ac_port.h"

typedef enum {
    AC_FADE_NONE,
    AC_FADE_IN,
    AC_FADE_OUT,
} ACFadeMode;

static ACFadeMode sFadeMode = AC_FADE_NONE;
static int sFadeFrames;

int AC_FadeIn(int frames) {
    if (sFadeMode != AC_FADE_NONE && sFadeFrames > 0) {
        return FALSE;
    }
    sFadeMode = AC_FADE_IN;
    sFadeFrames = frames;
    return TRUE;
}

int AC_FadeOut(int frames) {
    if (sFadeMode != AC_FADE_NONE && sFadeFrames > 0) {
        return FALSE;
    }
    sFadeMode = AC_FADE_OUT;
    sFadeFrames = frames;
    return TRUE;
}

void AC_FadeUpdate(void) {
    if (sFadeFrames > 0) {
        sFadeFrames--;
    }
    if (sFadeFrames == 0) {
        sFadeMode = AC_FADE_NONE;
    }
}
