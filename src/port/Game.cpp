#include <libultraship.h>

#include <fast/interpreter.h>
#if __has_include(<SDL2/SDL.h>)
#include <SDL2/SDL.h>
#else
#include <SDL.h>
#endif

#include <cstdio>
#include <cstdlib>
#include "Engine.h"
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef ACPORT_AUDIO
#include "port/audio/ACAudio.h"
#endif

extern "C" {
#include "FrameStub.h"
void AC_BootInit(void);
int AC_RunFrame(void);
extern uint8_t game_GameFrame;
}

static bool sGameRunning = false;

void push_frame() {
    GameEngine::StartAudioFrame();
    GameEngine::Instance->StartFrame();
    if (sGameRunning) {
        sGameRunning = AC_RunFrame() != 0;
#ifdef ACPORT_AUDIO
        AC_AudioFrame(game_GameFrame);
#endif
    } else {
        FrameStub_Run();
    }
    GameEngine::EndAudioFrame();
}

#if defined(__APPLE__) && !defined(__IOS__)
#include <execinfo.h>
#include <csignal>
static void PortCrashTrace(int sig) {
    void* frames[64];
    int n = backtrace(frames, 64);
    backtrace_symbols_fd(frames, n, 2);
    _exit(128 + sig);
}
#endif

#ifdef _WIN32
int SDL_main(int argc, char** argv) {
#else
int main(int argc, char* argv[]) {
#endif
#if defined(__APPLE__) && !defined(__IOS__)
    // Disable the macOS "press and hold" accent/diacritic popup for this app. SDL keeps a Cocoa text
    // input context active, so holding a movement key is interpreted as holding a letter key in a text
    // field, and macOS shows the accent picker instead of repeating it. Per-app equivalent of
    // `defaults write -app <App> ApplePressAndHoldEnabled -bool false`; key repeat still works.
    CFPreferencesSetAppValue(CFSTR("ApplePressAndHoldEnabled"), kCFBooleanFalse, kCFPreferencesCurrentApplication);
    CFPreferencesAppSynchronize(kCFPreferencesCurrentApplication);
    signal(SIGSEGV, PortCrashTrace);
    signal(SIGBUS, PortCrashTrace);
    signal(SIGTRAP, PortCrashTrace);
    signal(SIGILL, PortCrashTrace);
    signal(SIGABRT, PortCrashTrace);
    signal(SIGFPE, PortCrashTrace);
#endif
    GameEngine::Create(argc, argv);
    FrameStub_Init();
    if (GameEngine::IsGameArchiveLoaded()) {
        AC_BootInit();
        sGameRunning = true;
    }
    while (WindowIsRunning()) {
        push_frame();
    }
#ifdef ACPORT_AUDIO
    AC_AudioShutdown();
#endif
    GameEngine::Instance->Destroy();
    return 0;
}
