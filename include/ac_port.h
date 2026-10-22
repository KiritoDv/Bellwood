#ifndef AC_PORT_H
#define AC_PORT_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef s64 ACTime;
typedef u32 ACTick;

typedef struct ACCalendarTime {
    int sec;
    int min;
    int hour;
    int mday;
    int mon;
    int year;
    int wday;
    int yday;
    int msec;
    int usec;
} ACCalendarTime;

#define AC_BUS_CLOCK 162000000
#define AC_TIMER_CLOCK (AC_BUS_CLOCK / 4)

#define ACTicksToSeconds(ticks) ((ticks) / AC_TIMER_CLOCK)
#define ACTicksToMilliseconds(ticks) ((ticks) / (AC_TIMER_CLOCK / 1000))
#define ACTicksToMicroseconds(ticks) (((ticks) * 8) / (AC_TIMER_CLOCK / 125000))
#define ACSecondsToTicks(sec) ((sec) * AC_TIMER_CLOCK)
#define ACMillisecondsToTicks(msec) ((msec) * (AC_TIMER_CLOCK / 1000))
#define ACMicrosecondsToTicks(usec) (((usec) * (AC_TIMER_CLOCK / 125000)) / 8)

ACTime AC_GetTime(void);
ACTick AC_GetTick(void);
ACTime AC_CalendarTimeToTicks(const ACCalendarTime* td);
void AC_TicksToCalendarTime(ACTime ticks, ACCalendarTime* td);

int AC_FadeIn(int frames);
int AC_FadeOut(int frames);

void AC_DebugPrint(int x, int y, int show_count, const char* fmt, ...);

f32 AC_PadGetSubStickValue(void);
s16 AC_PadGetSubStickAngle(void);

f32 OTRGetDimensionFromLeftEdge(f32 v);
f32 OTRGetDimensionFromRightEdge(f32 v);
s16 OTRGetRectDimensionFromLeftEdge(f32 v);
s16 OTRGetRectDimensionFromRightEdge(f32 v);
f32 OTRGetDimensionFromLeftEdgeOverride(f32 v);
f32 OTRGetDimensionFromRightEdgeOverride(f32 v);
s16 OTRGetRectDimensionFromLeftEdgeOverride(f32 v);
s16 OTRGetRectDimensionFromRightEdgeOverride(f32 v);

f32 AC_WideScale(void);
u32 AC_RenderSizeKey(void);
int AC_WideCount(f32* carry);


int AC_GbaIslandWithoutGba(void);
int AC_GbaLinkAvailable(void);
int AC_EReaderConnect(void);
int AC_EReaderSendApp(u8* program, size_t size);
int AC_EReaderRead(u8* buf, size_t size, u8 type);
int AC_DesignFilesEnabled(void);
int AC_DesignExport(const u8* designs, int count);
int AC_DesignImportReady(void);
int AC_DesignImport(u8* designs, int count);

extern s32 gAC_PrerenderFb;

void AC_TexInvalidateRange(const void* addr, size_t size);

#define AC_GAME_HEAP_SIZE (96 * 1024 * 1024)

void AC_BootInit(void);
int AC_RunFrame(void);
void AC_SubmitFrame(void* dl);
int AC_SetClearColor(u8 r, u8 g, u8 b);
void AC_SetFrameDivisor(int divisor);
void AC_RequestReset(void);
void AC_ResetTick(void);
int AC_TakeRebootRequest(void);
void AC_AramWriteRecord(int res_no, u32 offset, const void* data, u32 size);

void gfx_texture_cache_clear(void);
void gfx_texture_cache_invalidate_range(const void* addr, size_t size);

#ifdef __cplusplus
}
#endif

#endif
