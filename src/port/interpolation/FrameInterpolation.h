#pragma once

#include <libultraship/libultra/types.h>

#ifdef __cplusplus
#include <unordered_map>

union Gfx;

struct FrameInterpolationResult {
    std::unordered_map<Mtx*, MtxF> mtx;
    std::unordered_map<Gfx*, Gfx*> dl;
};

FrameInterpolationResult FrameInterpolation_Interpolate(float step);
bool FrameInterpolation_IsActive(void);

extern "C" {
#endif

struct xyz_s;
struct s_xyz_s;

void FrameInterpolation_ShouldInterpolateFrame(bool shouldInterpolate);

void FrameInterpolation_StartRecord(void);

void FrameInterpolation_StopRecord(void);

void FrameInterpolation_RecordOpenChild(const void* a, int b);

void FrameInterpolation_RecordCloseChild(void);

int FrameInterpolation_RecordOpenChildScope(const void* a, int b);

void FrameInterpolation_RecordCloseChildScope(int depth);

void FrameInterpolation_PauseRecord(void);

void FrameInterpolation_ResumeRecord(void);

void FrameInterpolation_DontInterpolateCamera(void);

int FrameInterpolation_GetCameraEpoch(void);

void FrameInterpolation_CameraUpdated(const struct xyz_s* eye, const struct xyz_s* center);

void FrameInterpolation_ResetObject(const void* object);

int FrameInterpolation_GetObjectEpoch(const void* object);

void FrameInterpolation_RecordActorPosRotMatrix(void);

void FrameInterpolation_RecordMatrixPush(void);

void FrameInterpolation_RecordMatrixPop(void);

void FrameInterpolation_RecordMatrixPut(MtxF* src);

void FrameInterpolation_RecordMatrixMult(MtxF* mf, u8 mode);

void FrameInterpolation_RecordMatrixTranslate(f32 x, f32 y, f32 z, u8 mode);

void FrameInterpolation_RecordMatrixScale(f32 x, f32 y, f32 z, u8 mode);

void FrameInterpolation_RecordMatrixRotate1Coord(u32 coord, s16 value, u8 mode);

void FrameInterpolation_RecordMatrixRotateXYZ(s16 x, s16 y, s16 z, u8 mode);

void FrameInterpolation_RecordMatrixTranslateRotateZYX(const struct xyz_s* translation,
                                                       const struct s_xyz_s* rotation);

void FrameInterpolation_RecordMatrixSetTranslateRotateYXZ(f32 translateX, f32 translateY, f32 translateZ,
                                                          const struct s_xyz_s* rot);

void FrameInterpolation_RecordMatrixMtxFToMtx(MtxF* src, Mtx* dest);

void FrameInterpolation_RecordMatrixF2L(float mf[4][4], Mtx* dest);

void FrameInterpolation_RecordMatrixToMtx(Mtx* dest);

void FrameInterpolation_RecordMatrixRotateAxis(s16 angle, const struct xyz_s* axis, u8 mode);

void FrameInterpolation_RecordTexScroll(union Gfx* dl, u32 wrap);

#ifdef __cplusplus
}
#endif
