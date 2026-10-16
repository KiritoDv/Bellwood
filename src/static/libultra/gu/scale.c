#include "libultra/gu.h"
#include "port/interpolation/FrameInterpolation.h"

void guScale(Mtx *m, float x, float y, float z) {
    float mf[4][4];
    guScaleF(mf, x, y, z);
    FrameInterpolation_RecordMatrixF2L(mf, m);
    guMtxF2L(mf, m);
}
