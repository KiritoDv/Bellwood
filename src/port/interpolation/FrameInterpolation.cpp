#include "FrameInterpolation.h"

#include <cmath>
#include <map>
#include <unordered_map>
#include <utility>
#include <vector>

struct xyz_s {
    f32 x, y, z;
};

struct s_xyz_s {
    s16 x, y, z;
};

typedef xyz_s Vec3f;
typedef s_xyz_s Vec3s;

extern "C" {
void Matrix_push(void);
void Matrix_pull(void);
void Matrix_put(MtxF* src);
MtxF* get_Matrix_now(void);
void Matrix_mult(MtxF* m, u8 mode);
void Matrix_translate(f32 x, f32 y, f32 z, u8 mode);
void Matrix_scale(f32 x, f32 y, f32 z, u8 mode);
void Matrix_RotateX(s16 x, int mode);
void Matrix_RotateY(s16 y, int mode);
void Matrix_RotateZ(s16 z, int mode);
void Matrix_rotateXYZ(s16 x, s16 y, s16 z, int mode);
void Matrix_softcv3_mult(Vec3f* pos, Vec3s* rot);
void Matrix_softcv3_load(Vec3s* src, f32 x, f32 y, f32 z);
void Matrix_RotateVector(s16 angle, Vec3f* axis, u8 mode);
MtxF* Matrix_SwapStack(MtxF* stack);
void Skin_Matrix_MulMatrix(MtxF* mfA, MtxF* mfB, MtxF* dest);
}

static bool invert_matrix(const float m[16], float invOut[16]);

using namespace std;

namespace {

enum class Op {
    OpenChild,
    CloseChild,

    MatrixPush,
    MatrixPop,
    MatrixPut,
    MatrixMult,
    MatrixTranslate,
    MatrixScale,
    MatrixRotate1Coord,
    MatrixRotateXYZ,
    MatrixTranslateRotateZYX,
    MatrixSetTranslateRotateYXZ,
    MatrixMtxFToMtx,
    MatrixToMtx,
    MatrixRotateAxis,
};

typedef pair<const void*, int> label;

union Data {
    Data() {
    }

    struct {
        MtxF src;
    } matrix_put;

    struct {
        MtxF mf;
        u8 mode;
    } matrix_mult;

    struct {
        f32 x, y, z;
        u8 mode;
    } matrix_translate, matrix_scale;

    struct {
        u32 coord;
        s16 value;
        u8 mode;
    } matrix_rotate_1_coord;

    struct {
        s16 x, y, z;
        u8 mode;
    } matrix_rotate_xyz;

    struct {
        Vec3f translation;
        Vec3s rotation;
    } matrix_translate_rotate_zyx;

    struct {
        f32 translateX, translateY, translateZ;
        Vec3s rot;
        bool has_mtx;
    } matrix_set_translate_rotate_yxz;

    struct {
        MtxF src;
        Mtx* dest;
    } matrix_mtxf_to_mtx;

    struct {
        Mtx* dest;
        MtxF src;
        bool has_adjusted;
    } matrix_to_mtx;

    struct {
        s16 angle;
        Vec3f axis;
        u8 mode;
    } matrix_rotate_axis;

    struct {
        label key;
        size_t idx;
    } open_child;
};

struct Path {
    map<label, vector<Path>> children;
    map<Op, vector<Data>> ops;
    vector<pair<Op, size_t>> items;
};

struct Recording {
    Path root_path;
};

bool is_recording;
bool discard_current;
int pause_depth;
vector<Path*> current_path;
uint32_t camera_epoch;
uint32_t previous_camera_epoch;
Recording current_recording;
Recording previous_recording;

bool next_is_actor_pos_rot_matrix;
bool has_inv_actor_mtx;
MtxF inv_actor_mtx;
size_t inv_actor_mtx_path_index;

unordered_map<const void*, int> object_epochs;

constexpr f32 kRadToDeg = 57.29577951308232f;
constexpr int kReplayStackDepth = 64;
MtxF replay_stack[kReplayStackDepth];

Data& append(Op op) {
    auto& m = current_path.back()->ops[op];
    current_path.back()->items.emplace_back(op, m.size());
    return m.emplace_back();
}

void close_one(void) {
    if (current_path.size() <= 1) {
        return;
    }
    if (has_inv_actor_mtx && current_path.size() == inv_actor_mtx_path_index) {
        has_inv_actor_mtx = false;
    }
    current_path.pop_back();
}

struct InterpolateCtx {
    float step;
    float w;
    int depth;
    unordered_map<Mtx*, MtxF> mtx_replacements;
    MtxF tmp_mtxf;
    Vec3f tmp_vec3f;
    Vec3s tmp_vec3s;
    MtxF actor_mtx;

    MtxF* new_replacement(Mtx* addr) {
        return &mtx_replacements[addr];
    }

    void interpolate_mtxf(MtxF* res, MtxF* o, MtxF* n) {
        for (size_t i = 0; i < 4; i++) {
            for (size_t j = 0; j < 4; j++) {
                res->mf[i][j] = w * o->mf[i][j] + step * n->mf[i][j];
            }
        }
    }

    float lerp(f32 o, f32 n) {
        return w * o + step * n;
    }

    void lerp_vec3f(Vec3f* res, Vec3f* o, Vec3f* n) {
        res->x = lerp(o->x, n->x);
        res->y = lerp(o->y, n->y);
        res->z = lerp(o->z, n->z);
    }

    s16 interpolate_angle(s16 os, s16 ns) {
        if (os == ns)
            return ns;
        int o = (u16)os;
        int n = (u16)ns;
        u16 res;
        int diff = o - n;
        if (-0x8000 <= diff && diff <= 0x8000) {
            if (diff < -0x4000 || diff > 0x4000) {
                return ns;
            }
            res = (u16)(w * o + step * n);
        } else {
            if (o < n) {
                o += 0x10000;
            } else {
                n += 0x10000;
            }
            diff = o - n;
            if (diff < -0x4000 || diff > 0x4000) {
                return ns;
            }
            res = (u16)(w * o + step * n);
        }
        return res;
    }

    void interpolate_angles(Vec3s* res, Vec3s* o, Vec3s* n) {
        res->x = interpolate_angle(o->x, n->x);
        res->y = interpolate_angle(o->y, n->y);
        res->z = interpolate_angle(o->z, n->z);
    }

    void interpolate_branch(Path* old_path, Path* new_path) {
        for (auto& item : new_path->items) {
            Data& new_op = new_path->ops[item.first][item.second];

            if (item.first == Op::OpenChild) {
                auto& new_children = new_path->children.find(new_op.open_child.key)->second;
                if (auto it = old_path->children.find(new_op.open_child.key);
                    it != old_path->children.end() && new_op.open_child.idx < it->second.size()) {
                    interpolate_branch(&it->second[new_op.open_child.idx], &new_children[new_op.open_child.idx]);
                } else {
                    interpolate_branch(&new_children[new_op.open_child.idx], &new_children[new_op.open_child.idx]);
                }
                continue;
            }

            if (auto it = old_path->ops.find(item.first); it != old_path->ops.end()) {
                if (item.second < it->second.size()) {
                    Data& old_op = it->second[item.second];
                    switch (item.first) {
                        case Op::OpenChild:
                        case Op::CloseChild:
                            break;

                        case Op::MatrixPush:
                            if (depth + 1 < kReplayStackDepth) {
                                Matrix_push();
                                depth++;
                            }
                            break;

                        case Op::MatrixPop:
                            if (depth > 0) {
                                Matrix_pull();
                                depth--;
                            }
                            break;

                        case Op::MatrixPut:
                            interpolate_mtxf(&tmp_mtxf, &old_op.matrix_put.src, &new_op.matrix_put.src);
                            Matrix_put(&tmp_mtxf);
                            break;

                        case Op::MatrixMult:
                            interpolate_mtxf(&tmp_mtxf, &old_op.matrix_mult.mf, &new_op.matrix_mult.mf);
                            Matrix_mult(&tmp_mtxf, new_op.matrix_mult.mode);
                            break;

                        case Op::MatrixTranslate:
                            Matrix_translate(lerp(old_op.matrix_translate.x, new_op.matrix_translate.x),
                                             lerp(old_op.matrix_translate.y, new_op.matrix_translate.y),
                                             lerp(old_op.matrix_translate.z, new_op.matrix_translate.z),
                                             new_op.matrix_translate.mode);
                            break;

                        case Op::MatrixScale:
                            Matrix_scale(lerp(old_op.matrix_scale.x, new_op.matrix_scale.x),
                                         lerp(old_op.matrix_scale.y, new_op.matrix_scale.y),
                                         lerp(old_op.matrix_scale.z, new_op.matrix_scale.z), new_op.matrix_scale.mode);
                            break;

                        case Op::MatrixRotate1Coord: {
                            s16 v = interpolate_angle(old_op.matrix_rotate_1_coord.value,
                                                      new_op.matrix_rotate_1_coord.value);
                            u8 mode = new_op.matrix_rotate_1_coord.mode;
                            switch (new_op.matrix_rotate_1_coord.coord) {
                                case 0:
                                    Matrix_RotateX(v, mode);
                                    break;

                                case 1:
                                    Matrix_RotateY(v, mode);
                                    break;

                                case 2:
                                    Matrix_RotateZ(v, mode);
                                    break;
                            }
                            break;
                        }

                        case Op::MatrixRotateXYZ:
                            Matrix_rotateXYZ(interpolate_angle(old_op.matrix_rotate_xyz.x, new_op.matrix_rotate_xyz.x),
                                             interpolate_angle(old_op.matrix_rotate_xyz.y, new_op.matrix_rotate_xyz.y),
                                             interpolate_angle(old_op.matrix_rotate_xyz.z, new_op.matrix_rotate_xyz.z),
                                             new_op.matrix_rotate_xyz.mode);
                            break;

                        case Op::MatrixTranslateRotateZYX:
                            lerp_vec3f(&tmp_vec3f, &old_op.matrix_translate_rotate_zyx.translation,
                                       &new_op.matrix_translate_rotate_zyx.translation);
                            interpolate_angles(&tmp_vec3s, &old_op.matrix_translate_rotate_zyx.rotation,
                                               &new_op.matrix_translate_rotate_zyx.rotation);
                            Matrix_softcv3_mult(&tmp_vec3f, &tmp_vec3s);
                            break;

                        case Op::MatrixSetTranslateRotateYXZ:
                            interpolate_angles(&tmp_vec3s, &old_op.matrix_set_translate_rotate_yxz.rot,
                                               &new_op.matrix_set_translate_rotate_yxz.rot);
                            Matrix_softcv3_load(&tmp_vec3s,
                                                lerp(old_op.matrix_set_translate_rotate_yxz.translateX,
                                                     new_op.matrix_set_translate_rotate_yxz.translateX),
                                                lerp(old_op.matrix_set_translate_rotate_yxz.translateY,
                                                     new_op.matrix_set_translate_rotate_yxz.translateY),
                                                lerp(old_op.matrix_set_translate_rotate_yxz.translateZ,
                                                     new_op.matrix_set_translate_rotate_yxz.translateZ));
                            if (new_op.matrix_set_translate_rotate_yxz.has_mtx &&
                                old_op.matrix_set_translate_rotate_yxz.has_mtx) {
                                actor_mtx = *get_Matrix_now();
                            }
                            break;

                        case Op::MatrixMtxFToMtx:
                            interpolate_mtxf(new_replacement(new_op.matrix_mtxf_to_mtx.dest),
                                             &old_op.matrix_mtxf_to_mtx.src, &new_op.matrix_mtxf_to_mtx.src);
                            break;

                        case Op::MatrixToMtx: {
                            if (old_op.matrix_to_mtx.has_adjusted && new_op.matrix_to_mtx.has_adjusted) {
                                interpolate_mtxf(&tmp_mtxf, &old_op.matrix_to_mtx.src, &new_op.matrix_to_mtx.src);
                                Skin_Matrix_MulMatrix(&actor_mtx, &tmp_mtxf,
                                                      new_replacement(new_op.matrix_to_mtx.dest));
                            } else {
                                interpolate_mtxf(new_replacement(new_op.matrix_to_mtx.dest), &old_op.matrix_to_mtx.src,
                                                 &new_op.matrix_to_mtx.src);
                            }
                            break;
                        }

                        case Op::MatrixRotateAxis:
                            lerp_vec3f(&tmp_vec3f, &old_op.matrix_rotate_axis.axis, &new_op.matrix_rotate_axis.axis);
                            Matrix_RotateVector(
                                interpolate_angle(old_op.matrix_rotate_axis.angle, new_op.matrix_rotate_axis.angle),
                                &tmp_vec3f, new_op.matrix_rotate_axis.mode);
                            break;
                    }
                }
            }
        }
    }
};

} // anonymous namespace

FrameInterpolationResult FrameInterpolation_Interpolate(float step) {
    FrameInterpolationResult result;
    InterpolateCtx ctx;
    MtxF* saved;

    ctx.step = step;
    ctx.w = 1.0f - step;
    ctx.depth = 0;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            replay_stack[0].mf[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    ctx.actor_mtx = replay_stack[0];

    saved = Matrix_SwapStack(&replay_stack[0]);
    ctx.interpolate_branch(&previous_recording.root_path, &current_recording.root_path);
    Matrix_SwapStack(saved);

    result.mtx = std::move(ctx.mtx_replacements);
    return result;
}

static bool camera_interpolation = true;

void FrameInterpolation_ShouldInterpolateFrame(bool shouldInterpolate) {
    if (is_recording || pause_depth > 0) {
        if (!shouldInterpolate) {
            discard_current = true;
        }
        return;
    }
    camera_interpolation = shouldInterpolate;
}

void FrameInterpolation_StartRecord(void) {
    previous_recording = move(current_recording);
    current_recording = {};
    current_path.clear();
    current_path.push_back(&current_recording.root_path);
    discard_current = false;
    pause_depth = 0;
    has_inv_actor_mtx = false;
    next_is_actor_pos_rot_matrix = false;
    if (!camera_interpolation) {
        camera_interpolation = true;
        is_recording = false;
        return;
    }
    is_recording = FrameInterpolation_IsActive();
}

void FrameInterpolation_StopRecord(void) {
    previous_camera_epoch = camera_epoch;
    is_recording = false;
    pause_depth = 0;
    if (discard_current) {
        current_recording = {};
        current_path.clear();
        current_path.push_back(&current_recording.root_path);
        discard_current = false;
    }
}

void FrameInterpolation_RecordOpenChild(const void* a, int b) {
    if (!is_recording)
        return;
    label key = { a, b };
    auto& m = current_path.back()->children[key];
    append(Op::OpenChild).open_child = { key, m.size() };
    current_path.push_back(&m.emplace_back());
}

void FrameInterpolation_RecordCloseChild(void) {
    if (!is_recording)
        return;
    close_one();
}

int FrameInterpolation_RecordOpenChildScope(const void* a, int b) {
    if (!is_recording)
        return 0;
    int depth = (int)current_path.size();
    FrameInterpolation_RecordOpenChild(a, b);
    return depth;
}

void FrameInterpolation_RecordCloseChildScope(int depth) {
    if (!is_recording || depth <= 0)
        return;
    while ((int)current_path.size() > depth && current_path.size() > 1) {
        close_one();
    }
}

void FrameInterpolation_PauseRecord(void) {
    if (is_recording) {
        is_recording = false;
        pause_depth = 1;
    } else if (pause_depth > 0) {
        pause_depth++;
    }
}

void FrameInterpolation_ResumeRecord(void) {
    if (pause_depth > 0 && --pause_depth == 0) {
        is_recording = true;
    }
}

void FrameInterpolation_DontInterpolateCamera(void) {
    camera_epoch = previous_camera_epoch + 1;
}

int FrameInterpolation_GetCameraEpoch(void) {
    return (int)camera_epoch;
}

void FrameInterpolation_CameraUpdated(const Vec3f* eye, const Vec3f* at) {
    static f32 lastYaw = 0.0f;
    static f32 lastYawVelocity = 0.0f;
    static f32 lastPitch = 0.0f;
    static f32 lastPitchVelocity = 0.0f;
    static Vec3f lastEye = { 0.0f, 0.0f, 0.0f };
    static Vec3f lastEyeVelocity = { 0.0f, 0.0f, 0.0f };
    bool shouldInterpolate = true;
    f32 dx = eye->x - at->x;
    f32 dy = eye->y - at->y;
    f32 dz = eye->z - at->z;
    f32 yaw = atan2f(dx, dz) * kRadToDeg;
    f32 pitch = atan2f(dy, sqrtf(dx * dx + dz * dz)) * kRadToDeg;
    Vec3f eyeVelo = { eye->x - lastEye.x, eye->y - lastEye.y, eye->z - lastEye.z };
    Vec3f expectedEye = { lastEye.x + lastEyeVelocity.x, lastEye.y + lastEyeVelocity.y,
                          lastEye.z + lastEyeVelocity.z };
    f32 expectedYaw;
    f32 expectedPitch;

    if (yaw < 0.0f) {
        yaw += 360.0f;
    }
    expectedYaw = lastYaw + lastYawVelocity;
    if (expectedYaw > 360.0f) {
        expectedYaw -= 360.0f;
    } else if (expectedYaw < 0.0f) {
        expectedYaw += 360.0f;
    }
    expectedPitch = lastPitch + lastPitchVelocity;
    if (expectedPitch > 180.0f) {
        expectedPitch = 180.0f;
    } else if (expectedPitch < -180.0f) {
        expectedPitch = -180.0f;
    }

    f32 diffYaw = fabsf(yaw - expectedYaw);
    f32 diffPitch = fabsf(pitch - expectedPitch);
    f32 ex = eye->x - expectedEye.x;
    f32 ey = eye->y - expectedEye.y;
    f32 ez = eye->z - expectedEye.z;
    f32 diffDistEye = sqrtf(ex * ex + ey * ey + ez * ez);

    if ((diffYaw > 90.0f && diffYaw < 270.0f) || diffPitch > 60.0f || diffDistEye > 200.0f) {
        shouldInterpolate = false;
    }

    lastYawVelocity = yaw - lastYaw;
    lastPitchVelocity = pitch - lastPitch;
    lastEyeVelocity = eyeVelo;
    lastYaw = yaw;
    lastPitch = pitch;
    lastEye = *eye;

    if (!shouldInterpolate) {
        lastYawVelocity = 0.0f;
        lastPitchVelocity = 0.0f;
        lastEyeVelocity = { 0.0f, 0.0f, 0.0f };
        FrameInterpolation_ShouldInterpolateFrame(false);
    }
}

void FrameInterpolation_ResetObject(const void* object) {
    object_epochs[object]++;
}

int FrameInterpolation_GetObjectEpoch(const void* object) {
    auto it = object_epochs.find(object);
    return it == object_epochs.end() ? 0 : it->second;
}

void FrameInterpolation_RecordActorPosRotMatrix(void) {
    if (!is_recording)
        return;
    next_is_actor_pos_rot_matrix = true;
}

void FrameInterpolation_RecordMatrixPush(void) {
    if (!is_recording)
        return;
    append(Op::MatrixPush);
}

void FrameInterpolation_RecordMatrixPop(void) {
    if (!is_recording)
        return;
    append(Op::MatrixPop);
}

void FrameInterpolation_RecordMatrixPut(MtxF* src) {
    if (!is_recording)
        return;
    append(Op::MatrixPut).matrix_put = { *src };
}

void FrameInterpolation_RecordMatrixMult(MtxF* mf, u8 mode) {
    if (!is_recording)
        return;
    append(Op::MatrixMult).matrix_mult = { *mf, mode };
}

void FrameInterpolation_RecordMatrixTranslate(f32 x, f32 y, f32 z, u8 mode) {
    if (!is_recording)
        return;
    append(Op::MatrixTranslate).matrix_translate = { x, y, z, mode };
}

void FrameInterpolation_RecordMatrixScale(f32 x, f32 y, f32 z, u8 mode) {
    if (!is_recording)
        return;
    append(Op::MatrixScale).matrix_scale = { x, y, z, mode };
}

void FrameInterpolation_RecordMatrixRotate1Coord(u32 coord, s16 value, u8 mode) {
    if (!is_recording)
        return;
    append(Op::MatrixRotate1Coord).matrix_rotate_1_coord = { coord, value, mode };
}

void FrameInterpolation_RecordMatrixRotateXYZ(s16 x, s16 y, s16 z, u8 mode) {
    if (!is_recording)
        return;
    append(Op::MatrixRotateXYZ).matrix_rotate_xyz = { x, y, z, mode };
}

void FrameInterpolation_RecordMatrixTranslateRotateZYX(const Vec3f* translation, const Vec3s* rotation) {
    if (!is_recording)
        return;
    append(Op::MatrixTranslateRotateZYX).matrix_translate_rotate_zyx = { *translation, *rotation };
}

void FrameInterpolation_RecordMatrixSetTranslateRotateYXZ(f32 translateX, f32 translateY, f32 translateZ,
                                                          const Vec3s* rot) {
    if (!is_recording)
        return;
    auto& d = append(Op::MatrixSetTranslateRotateYXZ).matrix_set_translate_rotate_yxz = { translateX, translateY,
                                                                                          translateZ, *rot };
    if (next_is_actor_pos_rot_matrix) {
        d.has_mtx = true;
        if (invert_matrix((const float*)get_Matrix_now()->mf, (float*)inv_actor_mtx.mf)) {
            has_inv_actor_mtx = true;
            inv_actor_mtx_path_index = current_path.size();
        } else {
            d.has_mtx = false;
        }
        next_is_actor_pos_rot_matrix = false;
    }
}

void FrameInterpolation_RecordMatrixMtxFToMtx(MtxF* src, Mtx* dest) {
    if (!is_recording)
        return;
    append(Op::MatrixMtxFToMtx).matrix_mtxf_to_mtx = { *src, dest };
}

void FrameInterpolation_RecordMatrixF2L(float mf[4][4], Mtx* dest) {
    if (!is_recording)
        return;
    auto& d = append(Op::MatrixMtxFToMtx).matrix_mtxf_to_mtx;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            d.src.mf[i][j] = mf[i][j];
        }
    }
    d.dest = dest;
}

void FrameInterpolation_RecordMatrixToMtx(Mtx* dest) {
    if (!is_recording)
        return;
    auto& d = append(Op::MatrixToMtx).matrix_to_mtx = { dest };
    if (has_inv_actor_mtx) {
        d.has_adjusted = true;
        Skin_Matrix_MulMatrix(&inv_actor_mtx, get_Matrix_now(), &d.src);
    } else {
        d.src = *get_Matrix_now();
    }
}

void FrameInterpolation_RecordMatrixRotateAxis(s16 angle, const Vec3f* axis, u8 mode) {
    if (!is_recording)
        return;
    append(Op::MatrixRotateAxis).matrix_rotate_axis = { angle, *axis, mode };
}

// https://stackoverflow.com/questions/1148309/inverting-a-4x4-matrix
static bool invert_matrix(const float m[16], float invOut[16]) {
    float inv[16], det;
    int i;

    inv[0] = m[5] * m[10] * m[15] - m[5] * m[11] * m[14] - m[9] * m[6] * m[15] + m[9] * m[7] * m[14] +
             m[13] * m[6] * m[11] - m[13] * m[7] * m[10];

    inv[4] = -m[4] * m[10] * m[15] + m[4] * m[11] * m[14] + m[8] * m[6] * m[15] - m[8] * m[7] * m[14] -
             m[12] * m[6] * m[11] + m[12] * m[7] * m[10];

    inv[8] = m[4] * m[9] * m[15] - m[4] * m[11] * m[13] - m[8] * m[5] * m[15] + m[8] * m[7] * m[13] +
             m[12] * m[5] * m[11] - m[12] * m[7] * m[9];

    inv[12] = -m[4] * m[9] * m[14] + m[4] * m[10] * m[13] + m[8] * m[5] * m[14] - m[8] * m[6] * m[13] -
              m[12] * m[5] * m[10] + m[12] * m[6] * m[9];

    inv[1] = -m[1] * m[10] * m[15] + m[1] * m[11] * m[14] + m[9] * m[2] * m[15] - m[9] * m[3] * m[14] -
             m[13] * m[2] * m[11] + m[13] * m[3] * m[10];

    inv[5] = m[0] * m[10] * m[15] - m[0] * m[11] * m[14] - m[8] * m[2] * m[15] + m[8] * m[3] * m[14] +
             m[12] * m[2] * m[11] - m[12] * m[3] * m[10];

    inv[9] = -m[0] * m[9] * m[15] + m[0] * m[11] * m[13] + m[8] * m[1] * m[15] - m[8] * m[3] * m[13] -
             m[12] * m[1] * m[11] + m[12] * m[3] * m[9];

    inv[13] = m[0] * m[9] * m[14] - m[0] * m[10] * m[13] - m[8] * m[1] * m[14] + m[8] * m[2] * m[13] +
              m[12] * m[1] * m[10] - m[12] * m[2] * m[9];

    inv[2] = m[1] * m[6] * m[15] - m[1] * m[7] * m[14] - m[5] * m[2] * m[15] + m[5] * m[3] * m[14] +
             m[13] * m[2] * m[7] - m[13] * m[3] * m[6];

    inv[6] = -m[0] * m[6] * m[15] + m[0] * m[7] * m[14] + m[4] * m[2] * m[15] - m[4] * m[3] * m[14] -
             m[12] * m[2] * m[7] + m[12] * m[3] * m[6];

    inv[10] = m[0] * m[5] * m[15] - m[0] * m[7] * m[13] - m[4] * m[1] * m[15] + m[4] * m[3] * m[13] +
              m[12] * m[1] * m[7] - m[12] * m[3] * m[5];

    inv[14] = -m[0] * m[5] * m[14] + m[0] * m[6] * m[13] + m[4] * m[1] * m[14] - m[4] * m[2] * m[13] -
              m[12] * m[1] * m[6] + m[12] * m[2] * m[5];

    inv[3] = -m[1] * m[6] * m[11] + m[1] * m[7] * m[10] + m[5] * m[2] * m[11] - m[5] * m[3] * m[10] -
             m[9] * m[2] * m[7] + m[9] * m[3] * m[6];

    inv[7] = m[0] * m[6] * m[11] - m[0] * m[7] * m[10] - m[4] * m[2] * m[11] + m[4] * m[3] * m[10] +
             m[8] * m[2] * m[7] - m[8] * m[3] * m[6];

    inv[11] = -m[0] * m[5] * m[11] + m[0] * m[7] * m[9] + m[4] * m[1] * m[11] - m[4] * m[3] * m[9] -
              m[8] * m[1] * m[7] + m[8] * m[3] * m[5];

    inv[15] = m[0] * m[5] * m[10] - m[0] * m[6] * m[9] - m[4] * m[1] * m[10] + m[4] * m[2] * m[9] + m[8] * m[1] * m[6] -
              m[8] * m[2] * m[5];

    det = m[0] * inv[0] + m[1] * inv[4] + m[2] * inv[8] + m[3] * inv[12];

    if (det == 0) {
        return false;
    }

    det = 1.0 / det;

    for (i = 0; i < 16; i++) {
        invOut[i] = inv[i] * det;
    }

    return true;
}
