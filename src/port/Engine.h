#pragma once

#define LOAD_ASSET(path) (path == NULL ? NULL : (GameEngine_OTRSigCheck((const char*) path) ? ResourceGetDataByName((const char*) path) : path))
#define LOAD_ASSET_RAW(path) ResourceGetDataByName((const char*) path)

#ifdef __cplusplus
#include <vector>
#include <ship/controller/controldeck/ControlDeck.h>
#include <ship/Context.h>
#include <fast/interpreter.h>
#include <libultraship.h>
#include "interpolation/FrameInterpolation.h"

#ifndef IDYES
#define IDYES 6
#endif
#ifndef IDNO
#define IDNO 7
#endif

#define SAMPLES_HIGH 544
#define SAMPLES_LOW 528
#define AUDIO_FRAMES_PER_UPDATE 2
#define NUM_AUDIO_CHANNELS 2
#define SAMPLES_PER_FRAME (SAMPLES_HIGH * NUM_AUDIO_CHANNELS * 2)

enum FrameRateMode {
    FRAME_RATE_ORIGINAL,
    FRAME_RATE_MATCH_REFRESH_RATE,
    FRAME_RATE_60,
    FRAME_RATE_120,
    FRAME_RATE_144,
    FRAME_RATE_UNCAPPED,
    FRAME_RATE_CUSTOM,
};

struct AllocationEntry {
  uint8_t* addr;
  size_t size;
};

class GameEngine {
  public:
    static GameEngine* Instance;

    ImFont *fontStandard;
    ImFont *fontStandardLarger;
    ImFont *fontStandardLargest;
    ImFont *fontMono;
    ImFont *fontMonoLarger;
    ImFont *fontMonoLargest;

    std::shared_ptr<Ship::Context> context;

    std::vector<AllocationEntry> memoryPool;
    int totalScripts = 0;

    GameEngine();
    static void Create(int argc, char* argv[]);
    void LoadResourceFiles();
    void LoadScripts();
    void RunExtract(int argc, char* argv[]);
    void AudioInit();
    void FinishInit();
    void StartFrame() const;
    static void RunCommands(Gfx* Commands, const std::vector<FrameInterpolationResult>& replacements);
    static void RunCommandsUncapped(Gfx* Commands, uint32_t originalFps);
    static int32_t GetFrameRateMode();
    static uint32_t GetOriginalFPS();
    static uint32_t GetInterpolationFPS();
    static void HandleAudioThread();
    static void StartAudioFrame();
    static void EndAudioFrame();
    static void EndWebFrame();
    static void AudioExit();
    static void ProcessGfxCommands(Gfx* commands);
    static ImFont *CreateFontWithSize(float size, std::string fontPath);
    static void ScaleImGui();
    static bool IsAltAssetsEnabled();
    static bool IsGameArchiveLoaded();
    static uint32_t GetGameVersion();
    static void Destroy();
};

Fast::Interpreter* GameEngine_GetInterpreter();
bool GameEngine_IsGameArchiveCurrent(const std::string& path);

extern "C" {
#endif
// HUD and Rendering related
float GameEngine_GetAspectRatio();
uint32_t OTRGetCurrentWidth(void);
uint32_t OTRGetCurrentHeight(void);
float OTRGetHUDAspectRatio();
int32_t OTRConvertHUDXToScreenX(int32_t v);
float OTRGetDimensionFromLeftEdge(float v);
float OTRGetDimensionFromRightEdge(float v);
int16_t OTRGetRectDimensionFromLeftEdge(float v);
int16_t OTRGetRectDimensionFromRightEdge(float v);
float OTRGetDimensionFromLeftEdgeForcedAspect(float v, float aspectRatio);
float OTRGetDimensionFromRightEdgeForcedAspect(float v, float aspectRatio);
int16_t OTRGetRectDimensionFromLeftEdgeForcedAspect(float v, float aspectRatio);
int16_t OTRGetRectDimensionFromRightEdgeForcedAspect(float v, float aspectRatio);
float OTRGetDimensionFromLeftEdgeOverride(float v);
float OTRGetDimensionFromRightEdgeOverride(float v);
int16_t OTRGetRectDimensionFromLeftEdgeOverride(float v);
int16_t OTRGetRectDimensionFromRightEdgeOverride(float v);
uint32_t OTRGetGameRenderWidth();
uint32_t OTRGetGameRenderHeight();

// Engine related
void GameEngine_ProcessGfxCommands(Gfx* commands);
void GameEngine_SetFrameDivisor(uint32_t divisor);
uint32_t GameEngine_GetInterpolatedFPS();
uint32_t GameEngine_GetSampleRate();
uint32_t GameEngine_GetSamplesPerFrame();
uint32_t GameEngine_GetGameVersion();
bool GameEngine_IsGameArchiveLoaded();
bool GameEngine_OTRSigCheck(const char* imgData);
void* GameEngine_GetExactDataByName(const char* path);
void* GameEngine_Malloc(size_t size);
void GameEngine_Free(void* ptr);

#ifdef __cplusplus
}
#endif
