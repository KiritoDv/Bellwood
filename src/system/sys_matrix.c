#include "sys_matrix.h"
#include "TwoHeadArena.h"
#include "m_skin_matrix.h"
#include "graph.h"
#include "libc64/math64.h"
#include <math.h>
#include "libforest/gbi_extensions.h"
#include "libultra/gu.h"
#include "port/interpolation/FrameInterpolation.h"

// clang-format off
Mtx Mtx_clear = gdSPDefMtx(
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
);
// clang-format on

// clang-format off
MtxF MtxF_clear = { {
    {1.0f, 0.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f, 0.0f},
    {0.0f, 0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 0.0f, 1.0f},
} };
// clang-format on

static MtxF* Matrix_stack = NULL;
static MtxF* Matrix_now = NULL;

void new_Matrix(GAME* game) {
    Matrix_now = THA_alloc16(&game->tha, 0x500);
    Matrix_stack = Matrix_now;
}

void Matrix_push() {
    FrameInterpolation_RecordMatrixPush();
    Matrix_copy_MtxF(Matrix_now + 1, Matrix_now);
    Matrix_now++;
}

void Matrix_pull() {
    FrameInterpolation_RecordMatrixPop();
    Matrix_now--;
}

void Matrix_get(MtxF* dest) {
    Matrix_copy_MtxF(dest, Matrix_now);
}

void Matrix_put(MtxF* src) {
    FrameInterpolation_RecordMatrixPut(src);
    Matrix_copy_MtxF(Matrix_now, src);
}

MtxF* get_Matrix_now(void) {
    return (Matrix_now);
}

MtxF* Matrix_SwapStack(MtxF* stack) {
    MtxF* prev = Matrix_now;

    Matrix_now = stack;
    return prev;
}

void Matrix_mult(MtxF* m, u8 mode) {
    MtxF* curm = get_Matrix_now();

    FrameInterpolation_RecordMatrixMult(m, mode);

    if (mode == MTX_MULT) {
        Skin_Matrix_MulMatrix(curm, m, curm);
    } else {
        Matrix_copy_MtxF(Matrix_now, m);
    }
}

void Matrix_translate(f32 x, f32 y, f32 z, u8 mode) {
    MtxF* curm = Matrix_now;
    f32 tx, ty;

    FrameInterpolation_RecordMatrixTranslate(x, y, z, mode);

    if (mode == MTX_MULT) {
        tx = curm->xx;
        ty = curm->xy;
        curm->xw += tx * x + ty * y + curm->xz * z;
        tx = curm->yx;
        ty = curm->yy;
        curm->yw += tx * x + ty * y + curm->yz * z;
        tx = curm->zx;
        ty = curm->zy;
        curm->zw += tx * x + ty * y + curm->zz * z;
        tx = curm->wx;
        ty = curm->wy;
        curm->ww += tx * x + ty * y + curm->wz * z;
    } else {
        Skin_Matrix_SetTranslate(curm, x, y, z);
    }
}

void Matrix_scale(f32 x, f32 y, f32 z, u8 mode) {
    MtxF* curm = Matrix_now;

    FrameInterpolation_RecordMatrixScale(x, y, z, mode);

    if (mode == MTX_MULT) {
        curm->xx *= x;
        curm->yx *= x;
        curm->zx *= x;
        curm->xy *= y;
        curm->yy *= y;
        curm->zy *= y;
        curm->xz *= z;
        curm->yz *= z;
        curm->zz *= z;
        curm->wx *= x;
        curm->wy *= y;
        curm->wz *= z;
    } else {
        Skin_Matrix_SetScale(curm, x, y, z);
    }
}

void Matrix_RotateX(s16 x, int mode) {
    MtxF* curm;
    f32 sin;
    f32 cos;
    f32 fp, st2;

    FrameInterpolation_RecordMatrixRotate1Coord(0, x, mode);

    if (mode == MTX_MULT) {
        if (x != 0) {
            curm = Matrix_now;

            sin = sin_s(x);
            cos = cos_s(x);

            fp = curm->xy;
            st2 = curm->xz;
            curm->xy = (fp * cos) + (st2 * sin);
            curm->xz = -(fp * sin) + (st2 * cos);

            fp = curm->yy;
            st2 = curm->yz;
            curm->yy = (fp * cos) + (st2 * sin);
            curm->yz = -(fp * sin) + (st2 * cos);

            fp = curm->zy;
            st2 = curm->zz;
            curm->zy = (fp * cos) + (st2 * sin);
            curm->zz = -(fp * sin) + (st2 * cos);

            fp = curm->wy;
            st2 = curm->wz;
            curm->wy = (fp * cos) + (st2 * sin);
            curm->wz = -(fp * sin) + (st2 * cos);
        }
    } else {
        curm = Matrix_now;

        if (x != 0) {
            sin = sin_s(x);
            cos = cos_s(x);

        } else {
            sin = 0.0f;
            cos = 1.0f;
        }
        curm->yx = 0.0f;
        curm->zx = 0.0f;
        curm->wx = 0.0f;
        curm->xy = 0.0f;
        curm->wy = 0.0f;
        curm->xz = 0.0f;
        curm->wz = 0.0f;
        curm->xw = 0.0f;
        curm->yw = 0.0f;
        curm->zw = 0.0f;
        curm->xx = 1.0f;
        curm->ww = 1.0f;
        curm->yy = cos;
        curm->zz = cos;
        curm->zy = sin;
        curm->yz = -sin;
    }
}

void Matrix_RotateY(s16 y, int mode) {
    MtxF* curm;
    f32 sin;
    f32 cos;
    f32 fp, st2;

    FrameInterpolation_RecordMatrixRotate1Coord(1, y, mode);

    if (mode == MTX_MULT) {
        if (y != 0) {
            curm = Matrix_now;

            sin = sin_s(y);
            cos = cos_s(y);

            fp = curm->xx;
            st2 = curm->xz;
            curm->xx = fp * cos - st2 * sin;
            curm->xz = fp * sin + st2 * cos;

            fp = curm->yx;
            st2 = curm->yz;
            curm->yx = fp * cos - st2 * sin;
            curm->yz = fp * sin + st2 * cos;

            fp = curm->zx;
            st2 = curm->zz;
            curm->zx = fp * cos - st2 * sin;
            curm->zz = fp * sin + st2 * cos;

            fp = curm->wx;
            st2 = curm->wz;
            curm->wx = fp * cos - st2 * sin;
            curm->wz = fp * sin + st2 * cos;
        }
    } else {
        curm = Matrix_now;

        if (y != 0) {
            sin = sin_s(y);
            cos = cos_s(y);

        } else {
            sin = 0.0f;
            cos = 1.0f;
        }

        curm->yx = 0.0f;
        curm->wx = 0.0f;
        curm->xy = 0.0f;
        curm->zy = 0.0f;
        curm->wy = 0.0f;
        curm->yz = 0.0f;
        curm->wz = 0.0f;
        curm->xw = 0.0f;
        curm->yw = 0.0f;
        curm->zw = 0.0f;
        curm->yy = 1.0f;
        curm->ww = 1.0f;
        curm->xx = cos;
        curm->zz = cos;
        curm->zx = -sin;
        curm->xz = sin;
    }
}

void Matrix_RotateZ(s16 z, int mode) {
    MtxF* curm;
    f32 sin;
    f32 cos;
    f32 fp, st2;

    FrameInterpolation_RecordMatrixRotate1Coord(2, z, mode);

    if (mode == MTX_MULT) {
        if (z != 0) {
            curm = Matrix_now;

            sin = sin_s(z);
            cos = cos_s(z);

            fp = curm->xx;
            st2 = curm->xy;
            curm->xx = (fp * cos) + (st2 * sin);
            curm->xy = -(fp * sin) + (st2 * cos);

            fp = curm->yx;
            st2 = curm->yy;
            curm->yx = (fp * cos) + (st2 * sin);
            curm->yy = -(fp * sin) + (st2 * cos);

            fp = curm->zx;
            st2 = curm->zy;
            curm->zx = (fp * cos) + (st2 * sin);
            curm->zy = -(fp * sin) + (st2 * cos);

            fp = curm->wx;
            st2 = curm->wy;
            curm->wx = (fp * cos) + (st2 * sin);
            curm->wy = -(fp * sin) + (st2 * cos);
        }
    } else {
        curm = Matrix_now;

        if (z != 0) {
            sin = sin_s(z);
            cos = cos_s(z);

        } else {
            sin = 0.0f;
            cos = 1.0f;
        }

        curm->zx = 0.0f;
        curm->wx = 0.0f;
        curm->zy = 0.0f;
        curm->wy = 0.0f;
        curm->xz = 0.0f;
        curm->yz = 0.0f;
        curm->wz = 0.0f;
        curm->xw = 0.0f;
        curm->yw = 0.0f;
        curm->zw = 0.0f;
        curm->zz = 1.0f;
        curm->ww = 1.0f;
        curm->xx = cos;
        curm->yy = cos;
        curm->yx = sin;
        curm->xy = -sin;
    }
}

void Matrix_rotateXYZ(s16 x, s16 y, s16 z, int mode) {
    MtxF* curm = Matrix_now;
    f32 sin;
    f32 cos;
    f32 fp, st2;

    FrameInterpolation_RecordMatrixRotateXYZ(x, y, z, mode);

    if (mode == MTX_MULT) {
        sin = sin_s(z);
        cos = cos_s(z);

        fp = curm->xx;
        st2 = curm->xy;
        curm->xx = (fp * cos) + (st2 * sin);
        curm->xy = -(fp * sin) + (st2 * cos);

        fp = curm->yx;
        st2 = curm->yy;
        curm->yx = (fp * cos) + (st2 * sin);
        curm->yy = -(fp * sin) + (st2 * cos);

        fp = curm->zx;
        st2 = curm->zy;
        curm->zx = (fp * cos) + (st2 * sin);
        curm->zy = -(fp * sin) + (st2 * cos);

        fp = curm->wx;
        st2 = curm->wy;
        curm->wx = (fp * cos) + (st2 * sin);
        curm->wy = -(fp * sin) + (st2 * cos);

        if (y != 0) {
            sin = sin_s(y);
            cos = cos_s(y);

            fp = curm->xx;
            st2 = curm->xz;
            curm->xx = (fp * cos) - (st2 * sin);
            curm->xz = (fp * sin) + (st2 * cos);

            fp = curm->yx;
            st2 = curm->yz;
            curm->yx = (fp * cos) - (st2 * sin);
            curm->yz = (fp * sin) + (st2 * cos);

            fp = curm->zx;
            st2 = curm->zz;
            curm->zx = (fp * cos) - (st2 * sin);
            curm->zz = (fp * sin) + (st2 * cos);

            fp = curm->wx;
            st2 = curm->wz;
            curm->wx = (fp * cos) - (st2 * sin);
            curm->wz = (fp * sin) + (st2 * cos);
        }

        if (x != 0) {
            sin = sin_s(x);
            cos = cos_s(x);

            fp = curm->xy;
            st2 = curm->xz;
            curm->xy = (fp * cos) + (st2 * sin);
            curm->xz = -(fp * sin) + (st2 * cos);

            fp = curm->yy;
            st2 = curm->yz;
            curm->yy = (fp * cos) + (st2 * sin);
            curm->yz = -(fp * sin) + (st2 * cos);

            fp = curm->zy;
            st2 = curm->zz;
            curm->zy = (fp * cos) + (st2 * sin);
            curm->zz = -(fp * sin) + (st2 * cos);

            fp = curm->wy;
            st2 = curm->wz;
            curm->wy = (fp * cos) + (st2 * sin);
            curm->wz = -(fp * sin) + (st2 * cos);
        }
    } else {
        Skin_Matrix_SetRotateXyz_s(curm, x, y, z);
    }
}

void Matrix_softcv3_mult(xyz_t* pos, s_xyz* rot) {
    MtxF* curm = Matrix_now;
    f32 sin, cos;
    f32 fp, st2;

    FrameInterpolation_RecordMatrixTranslateRotateZYX(pos, rot);

    sin = sin_s(rot->z);
    cos = cos_s(rot->z);

    fp = curm->xx;
    st2 = curm->xy;

    curm->xw = curm->xw + (fp * pos->x + st2 * pos->y + curm->xz * pos->z);
    curm->xx = (fp * cos) + (st2 * sin);
    curm->xy = -(fp * sin) + (st2 * cos);

    fp = curm->yx;
    st2 = curm->yy;

    curm->yw = curm->yw + (fp * pos->x + st2 * pos->y + curm->yz * pos->z);
    curm->yx = (fp * cos) + (st2 * sin);
    curm->yy = -(fp * sin) + (st2 * cos);

    fp = curm->zx;
    st2 = curm->zy;

    curm->zw = curm->zw + (curm->zx * pos->x + curm->zy * pos->y + curm->zz * pos->z);
    curm->zx = (fp * cos) + (st2 * sin);
    curm->zy = -(fp * sin) + (st2 * cos);

    fp = curm->wx;
    st2 = curm->wy;
    curm->ww = curm->ww + (curm->wx * pos->x + curm->wy * pos->y + curm->wz * pos->z);
    curm->wx = (fp * cos) + (st2 * sin);
    curm->wy = -(fp * sin) + (st2 * cos);

    if (rot->y != 0) {

        sin = sin_s(rot->y);
        cos = cos_s(rot->y);

        fp = curm->xx;
        st2 = curm->xz;
        curm->xx = (fp * cos) - (st2 * sin);
        curm->xz = (fp * sin) + (st2 * cos);

        fp = curm->yx;
        st2 = curm->yz;
        curm->yx = (fp * cos) - (st2 * sin);
        curm->yz = (fp * sin) + (st2 * cos);

        fp = curm->zx;
        st2 = curm->zz;
        curm->zx = (fp * cos) - (st2 * sin);
        curm->zz = (fp * sin) + (st2 * cos);

        fp = curm->wx;
        st2 = curm->wz;
        curm->wx = (fp * cos) - (st2 * sin);
        curm->wz = (fp * sin) + (st2 * cos);
    }

    if (rot->x != 0) {
        sin = sin_s(rot->x);
        cos = cos_s(rot->x);

        fp = curm->xy;
        st2 = curm->xz;
        curm->xy = (fp * cos) + (st2 * sin);
        curm->xz = -(fp * sin) + (st2 * cos);

        fp = curm->yy;
        st2 = curm->yz;
        curm->yy = (fp * cos) + (st2 * sin);
        curm->yz = -(fp * sin) + (st2 * cos);

        fp = curm->zy;
        st2 = curm->zz;
        curm->zy = (fp * cos) + (st2 * sin);
        curm->zz = -(fp * sin) + (st2 * cos);

        fp = curm->wy;
        st2 = curm->wz;
        curm->wy = (fp * cos) + (st2 * sin);
        curm->wz = -(fp * sin) + (st2 * cos);
    }
}

void Matrix_softcv3_load(s_xyz* src, f32 x, f32 y, f32 z) {
    MtxF* curm = Matrix_now;
    f32 sin, cos;
    f32 ss, sc;

    sin = sin_s(src->y);
    cos = cos_s(src->y);

    curm->xx = cos;
    curm->zx = -sin;
    curm->xw = x;
    curm->yw = y;
    curm->zw = z;
    curm->wz = 0.0f;
    curm->wy = 0.0f;
    curm->wx = 0.0f;
    curm->ww = 1.0f;

    if (src->x != 0) {
        ss = sin_s(src->x);
        sc = cos_s(src->x);

        curm->zz = cos * sc;
        curm->zy = cos * ss;

        curm->xz = sin * sc;
        curm->xy = sin * ss;

        curm->yz = -ss;
        curm->yy = sc;
    } else {
        curm->zz = cos;
        curm->xz = sin;
        curm->yz = 0.0f;
        curm->zy = 0.0f;
        curm->xy = 0.0f;
        curm->yy = 1.0f;
    }
    if (src->z != 0) {
        sin = sin_s(src->z);
        cos = cos_s(src->z);

        ss = curm->xy;
        sc = curm->xx;

        curm->xx = (sc * cos) + (ss * sin);
        curm->xy = -(sc * sin) + (ss * cos);

        ss = curm->zy;
        sc = curm->zx;

        curm->zx = (sc * cos) + (ss * sin);
        curm->zy = -(sc * sin) + (ss * cos);

        ss = curm->yy;
        curm->yx = curm->yy * sin;
        curm->yy = ss * cos;
    } else {
        curm->yx = 0.0f;
    }
    FrameInterpolation_RecordMatrixSetTranslateRotateYXZ(x, y, z, src);
}

Mtx* _MtxF_to_Mtx(MtxF* src, Mtx* dest) {
    FrameInterpolation_RecordMatrixMtxFToMtx(src, dest);
    guMtxF2L(src->mf, dest);
    return dest;
}

Mtx* _Matrix_to_Mtx(Mtx* dest) {
    FrameInterpolation_RecordMatrixToMtx(dest);
    guMtxF2L(Matrix_now->mf, dest);
    return dest;
}

Mtx* _Matrix_to_Mtx_new(GRAPH* graph) {
    return _Matrix_to_Mtx(GRAPH_ALLOC(graph, sizeof(Mtx)));
}

void Matrix_Position(xyz_t* input_position, xyz_t* output_position) {
    MtxF* current_matrix = Matrix_now;

    output_position->x = (current_matrix->xx * input_position->x) +
                         (current_matrix->xy * input_position->y) +
                         (current_matrix->xz * input_position->z) +
                         current_matrix->xw;

    output_position->y = (current_matrix->yx * input_position->x) +
                         (current_matrix->yy * input_position->y) +
                         (current_matrix->yz * input_position->z) +
                         current_matrix->yw;

    output_position->z = (current_matrix->zx * input_position->x) +
                         (current_matrix->zy * input_position->y) +
                         (current_matrix->zz * input_position->z) +
                         current_matrix->zw;
}


void Matrix_Position_Zero(xyz_t* v) {
    MtxF* curm = Matrix_now;

    v->x = curm->xw;
    v->y = curm->yw;
    v->z = curm->zw;
}

void Matrix_Position_VecX(f32 x, xyz_t* v) {
    MtxF* curm = Matrix_now;

    v->x = curm->xw + (curm->xx * x);
    v->y = curm->yw + (curm->yx * x);
    v->z = curm->zw + (curm->zx * x);
}
void Matrix_Position_VecZ(f32 z, xyz_t* v) {
    MtxF* curm = Matrix_now;

    v->x = curm->xw + (curm->xz * z);
    v->y = curm->yw + (curm->yz * z);
    v->z = curm->zw + (curm->zz * z);
}

void Matrix_copy_MtxF(MtxF* dest, MtxF* src) {
    dest->xx = src->xx;
    dest->yx = src->yx;
    dest->zx = src->zx;
    dest->wx = src->wx;
    dest->xy = src->xy;
    dest->yy = src->yy;
    dest->zy = src->zy;
    dest->wy = src->wy;
    dest->xz = src->xz;
    dest->yz = src->yz;
    dest->zz = src->zz;
    dest->wz = src->wz;
    dest->xw = src->xw;
    dest->yw = src->yw;
    dest->zw = src->zw;
    dest->ww = src->ww;
}

void Matrix_MtxtoMtxF(Mtx* src, MtxF* dest) {
    guMtxL2F(dest->mf, src);
}

void Matrix_reverse(MtxF* curm) {
    f32 fp;

    fp = curm->yx;
    curm->yx = curm->xy;
    curm->xy = fp;

    fp = curm->zx;
    curm->zx = curm->xz;
    curm->xz = fp;

    fp = curm->zy;
    curm->zy = curm->yz;
    curm->yz = fp;
}

void Matrix_to_rotate_new(MtxF* curm, s_xyz* vec, int flag) {
    f32 temp;
    f32 temp2;
    f32 temp3;
    f32 temp4;

    temp = curm->xz;
    temp *= temp;
    temp += (curm->zz * curm->zz);
    vec->x = RAD2SHORT_ANGLE2(fatan2(-curm->yz, sqrtf(temp)));

    if ((vec->x == DEG2SHORT_ANGLE2(90.0f)) || (vec->x == DEG2SHORT_ANGLE2(-90.0f))) {
        vec->z = 0;

        vec->y = RAD2SHORT_ANGLE2(fatan2(-curm->zx, curm->xx));
    } else {
        vec->y = RAD2SHORT_ANGLE2(fatan2(curm->xz, curm->zz));

        if (flag == MTX_LOAD) {
            vec->z = RAD2SHORT_ANGLE2(fatan2(curm->yx, curm->yy));
        } else {
            temp = curm->xx;

            temp2 = curm->yx;
            temp *= temp;
            temp += curm->zx * curm->zx;
            temp += (temp2 * temp2);
            /* temp = xx^2+zx^2+yx^2 == 1 for a rotation matrix */
            temp = temp2 / sqrtf(temp);

            temp2 = curm->xy;
            temp3 = curm->yy;
            temp2 *= temp2;
            temp2 += curm->zy * curm->zy;
            temp2 += (temp3 * temp3);
            /* temp2 = xy^2+zy^2+yy^2 == 1 for a rotation matrix */
            temp2 = temp3 / sqrtf(temp2);

            /* for a rotation matrix, temp == yx and temp2 == yy
             * which is the same as in the !flag branch */
            vec->z = RAD2SHORT_ANGLE2(fatan2(temp, temp2));
        }
    }
}

void Matrix_to_rotate2_new(MtxF* curm, s_xyz* v, int flag) {
    f32 temp;
    f32 temp2;
    f32 temp3;
    f32 temp4;

    temp = curm->xx;
    temp *= temp;
    temp += (curm->yx * curm->yx);
    v->y = RAD2SHORT_ANGLE2(fatan2(-curm->zx, sqrtf(temp)));

    if ((v->y == DEG2SHORT_ANGLE2(90.0f)) || (v->y == DEG2SHORT_ANGLE2(-90.0f))) {
        v->x = 0;
        v->z = RAD2SHORT_ANGLE2(fatan2(-curm->xy, curm->yy));
    } else {
        v->z = RAD2SHORT_ANGLE2(fatan2(curm->yx, curm->xx));

        if (flag == MTX_LOAD) {
            v->x = RAD2SHORT_ANGLE2(fatan2(curm->zy, curm->zz));
        } else {
            temp = curm->xy;

            temp2 = curm->zy;
            temp *= temp;
            temp += curm->yy * curm->yy;
            temp += (temp2 * temp2);
            /* temp = zx^2+yy^2+zy^2 == 1 for a rotation matrix */
            temp = temp2 / sqrtf(temp);

            temp2 = curm->xz;
            temp3 = curm->zz;
            temp2 *= temp2;
            temp2 += curm->yz * curm->yz;
            temp2 += (temp3 * temp3);
            /* temp2 = xz^2+yz^2+zz^2 == 1 for a rotation matrix */
            temp2 = temp3 / sqrtf(temp2);

            v->x = RAD2SHORT_ANGLE2(fatan2(temp, temp2));
        }
    }
}

void Matrix_RotateVector(s16 angle, xyz_t* axis, u8 mode) {
    MtxF* curm;
    f32 sin;
    f32 cos;
    f32 temp1; // component x
    f32 temp2; // component y
    f32 temp3; // component z
    f32 temp4; // component q?

    FrameInterpolation_RecordMatrixRotateAxis(angle, axis, mode);

    if (mode == MTX_MULT) {
        if (angle != 0) {
            curm = Matrix_now;

            sin = sin_s(angle);
            cos = cos_s(angle);

            temp1 = curm->xx;
            temp2 = curm->xy;
            temp3 = curm->xz;
            temp4 = (axis->x * temp1 + axis->y * temp2 + axis->z * temp3) * (1.0f - cos);
            curm->xx = temp1 * cos + axis->x * temp4 + sin * (temp2 * axis->z - temp3 * axis->y);
            curm->xy = temp2 * cos + axis->y * temp4 + sin * (temp3 * axis->x - temp1 * axis->z);
            curm->xz = temp3 * cos + axis->z * temp4 + sin * (temp1 * axis->y - temp2 * axis->x);

            temp1 = curm->yx;
            temp2 = curm->yy;
            temp3 = curm->yz;
            temp4 = (axis->x * temp1 + axis->y * temp2 + axis->z * temp3) * (1.0f - cos);
            curm->yx = temp1 * cos + axis->x * temp4 + sin * (temp2 * axis->z - temp3 * axis->y);
            curm->yy = temp2 * cos + axis->y * temp4 + sin * (temp3 * axis->x - temp1 * axis->z);
            curm->yz = temp3 * cos + axis->z * temp4 + sin * (temp1 * axis->y - temp2 * axis->x);

            temp1 = curm->zx;
            temp2 = curm->zy;
            temp3 = curm->zz;
            temp4 = (axis->x * temp1 + axis->y * temp2 + axis->z * temp3) * (1.0f - cos);
            curm->zx = temp1 * cos + axis->x * temp4 + sin * (temp2 * axis->z - temp3 * axis->y);
            curm->zy = temp2 * cos + axis->y * temp4 + sin * (temp3 * axis->x - temp1 * axis->z);
            curm->zz = temp3 * cos + axis->z * temp4 + sin * (temp1 * axis->y - temp2 * axis->x);
        }
    } else {
        curm = Matrix_now;

        if (angle != 0) {
            sin = sin_s(angle);
            cos = cos_s(angle);

            curm->xx = axis->x * axis->x * (1.0f - cos) + cos;
            curm->yy = axis->y * axis->y * (1.0f - cos) + cos;
            curm->zz = axis->z * axis->z * (1.0f - cos) + cos;

            temp2 = axis->y * ((1.0f - cos) * axis->x);
            temp3 = axis->z * sin;
            curm->yx = temp2 + temp3;
            curm->xy = temp2 - temp3;

            temp2 = axis->z * ((1.0f - cos) * axis->x);
            temp3 = axis->y * sin;
            curm->zx = temp2 - temp3;
            curm->xz = temp2 + temp3;

            temp2 = axis->z * ((1.0f - cos) * axis->y);
            temp3 = axis->x * sin;
            curm->zy = temp2 + temp3;
            curm->yz = temp2 - temp3;

            curm->wx = 0.0f;
            curm->wy = 0.0f;
            curm->wz = 0.0f;
            curm->xw = 0.0f;
            curm->yw = 0.0f;
            curm->zw = 0.0f;
            curm->ww = 1.0f;
        } else {
            curm->yx = 0.0f;
            curm->zx = 0.0f;
            curm->wx = 0.0f;
            curm->xy = 0.0f;
            curm->zy = 0.0f;
            curm->wy = 0.0f;
            curm->xz = 0.0f;
            curm->yz = 0.0f;
            curm->wz = 0.0f;
            curm->xw = 0.0f;
            curm->yw = 0.0f;
            curm->zw = 0.0f;
            curm->xx = 1.0f;
            curm->yy = 1.0f;
            curm->zz = 1.0f;
            curm->ww = 1.0f;
        }
    }
}

void suMtxMakeTS(Mtx* mtx, f32 scaleX, f32 scaleY, f32 scaleZ, f32 translateX, f32 translateY, f32 translateZ) {
    f32 mf[4][4];

    mf[0][0] = scaleX;
    mf[0][1] = 0.0f;
    mf[0][2] = 0.0f;
    mf[0][3] = 0.0f;
    mf[1][0] = 0.0f;
    mf[1][1] = scaleY;
    mf[1][2] = 0.0f;
    mf[1][3] = 0.0f;
    mf[2][0] = 0.0f;
    mf[2][1] = 0.0f;
    mf[2][2] = scaleZ;
    mf[2][3] = 0.0f;
    mf[3][0] = translateX;
    mf[3][1] = translateY;
    mf[3][2] = translateZ;
    mf[3][3] = 1.0f;
    FrameInterpolation_RecordMatrixF2L(mf, mtx);
    guMtxF2L(mf, mtx);
}

// S(RxRyRz)T where S is a scale matrix, Rx/Ry/Rz are rotations about the x/y/z axes, and T is a translation
void suMtxMakeSRT(Mtx* mtx, f32 scaleX, f32 scaleY, f32 scaleZ, s16 rotX, s16 rotY, s16 rotZ, f32 translateX,
                  f32 translateY, f32 translateZ) {
    f32 mf[4][4];
    f32 sinX = sin_s(rotX);
    f32 sinY = sin_s(rotY); // sp+38
    f32 sinZ = sin_s(rotZ); // sp+34
    f32 cosX = cos_s(rotX); // sp+30
    f32 cosY = cos_s(rotY); // sp+2C
    f32 cosZ = cos_s(rotZ);

    mf[0][0] = cosY * cosZ * scaleX;

    mf[0][1] = cosY * sinZ * scaleX;

    mf[0][2] = -sinY * scaleX;

    mf[1][0] = ((sinX * sinY * cosZ) - (cosX * sinZ)) * scaleY;

    mf[1][1] = ((sinX * sinY * sinZ) + (cosX * cosZ)) * scaleY;

    mf[1][2] = sinX * cosY * scaleY;

    mf[2][0] = ((cosX * sinY * cosZ) + (sinX * sinZ)) * scaleZ;

    mf[2][1] = ((cosX * sinY * sinZ) - (sinX * cosZ)) * scaleZ;

    mf[2][2] = cosX * cosY * scaleZ;

    mf[3][0] = translateX;

    mf[3][1] = translateY;

    mf[3][2] = translateZ;

    mf[0][3] = mf[1][3] = mf[2][3] = 0.0f;
    mf[3][3] = 1.0f;
    FrameInterpolation_RecordMatrixF2L(mf, mtx);
    guMtxF2L(mf, mtx);
}

// S(RzRxRy)T where S is a scale matrix, Rx/Ry/Rz are rotations, and T is a translation
void suMtxMakeSRT_ZXY(Mtx* mtx, f32 scaleX, f32 scaleY, f32 scaleZ, s16 rotX, s16 rotY, s16 rotZ, f32 translateX,
                      f32 translateY, f32 translateZ) {
    f32 mf[4][4];
    f32 sinX = sin_s(rotX); // sp+4C
    f32 sinY = sin_s(rotY); // likely sp+48
    f32 sinZ = sin_s(rotZ); // sp+44
    f32 cosX = cos_s(rotX); // sp+40
    f32 cosY = cos_s(rotY); // sp+3C
    f32 cosZ = cos_s(rotZ); // likely sp+38

    mf[0][0] = ((cosY * cosZ) + (sinX * sinY * sinZ)) * scaleX;

    mf[0][1] = cosX * sinZ * scaleX;

    mf[0][2] = (-(sinY * cosZ) + (sinX * cosY * sinZ)) * scaleX;

    mf[1][0] = (-(cosY * sinZ) + (sinX * sinY * cosZ)) * scaleY;

    mf[1][1] = cosX * cosZ * scaleY;

    mf[1][2] = ((sinY * sinZ) + (sinX * cosY * cosZ)) * scaleY;

    mf[2][0] = cosX * sinY * scaleZ;

    mf[2][1] = -sinX * scaleZ;

    mf[2][2] = cosX * cosY * scaleZ;

    mf[3][0] = translateX;

    mf[3][1] = translateY;

    mf[3][2] = translateZ;

    mf[0][3] = mf[1][3] = mf[2][3] = 0.0f;
    mf[3][3] = 1.0f;
    FrameInterpolation_RecordMatrixF2L(mf, mtx);
    guMtxF2L(mf, mtx);
}
