#include "TouchControls.h"

#include "port/ShipCompat.h"
#include "port/ShipInit.hpp"
#include "port/events/Events.h"
#include "port/input/GcButtons.h"

#include <SDL2/SDL.h>
#include <imgui.h>
#include <imgui_internal.h>
#include <libultraship/bridge/consolevariablebridge.h>
#include <ship/controller/controldeck/ControlDeck.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#if defined(__IOS__) || defined(__ANDROID__)
#define TOUCH_CONTROLS_DEFAULT 1
#else
#define TOUCH_CONTROLS_DEFAULT 0
#endif

namespace {

constexpr float kStickMax = 80.0f;
constexpr float kCStickThreshold = 0.5f;
constexpr int64_t kMouseFingerId = -0x4D6F7573;

enum class Shape { Circle, Pill, Arrow };

enum DragTarget {
    kDragNone = -1,
    kDragStick = -2,
    kDragMenu = -3,
    kDragCStick = -4,
};

struct TouchButton {
    uint16_t mask;
    const char* id;
    const char* label;
    Shape shape;
    ImU32 color;
    ImVec2 center;
    float halfW;
    float halfH;
    float dirX;
    float dirY;
    bool pressed;
};

struct Stick {
    ImVec2 rest;
    ImVec2 anchor;
    ImVec2 pos;
    float travel = 0.0f;
    float nub = 0.0f;
    float dx = 0.0f;
    float dy = 0.0f;
    bool held = false;
    int64_t finger = -1;
};

struct Finger {
    ImVec2 pos;
    int64_t id;
};

struct OverlayState {
    bool active = false;
    bool menuOpen = false;
    bool editMode = false;
    uint16_t buttons = 0;
    float unit = 0.0f;
    Stick control;
    Stick cstick;
    ImVec2 menuCenter;
    float menuRadius = 0.0f;
    bool menuDown = false;
    ImVec2 doneCenter, doneHalf;
    ImVec2 resetCenter, resetHalf;
    std::vector<TouchButton> gameButtons;
};

OverlayState sState;

int64_t sDragFinger = -1;
int sDragTarget = kDragNone;
ImVec2 sDragOffset;
ImVec2 sDragPos;
bool sDoneDown = false;
bool sResetDown = false;

void LayoutKey(char* out, size_t outSize, const char* id, const char* axis) {
    snprintf(out, outSize, CVAR_TOUCH("Layout.%s.%s"), id, axis);
}

ImVec2 LoadPos(const char* id, ImVec2 fallback, float w, float h) {
    char kx[96], ky[96];
    LayoutKey(kx, sizeof(kx), id, "X");
    LayoutKey(ky, sizeof(ky), id, "Y");
    const float nx = CVarGetFloat(kx, -1.0f);
    const float ny = CVarGetFloat(ky, -1.0f);
    if (nx >= 0.0f && ny >= 0.0f) {
        return ImVec2(nx * w, ny * h);
    }
    return fallback;
}

void SavePos(const char* id, ImVec2 center, float w, float h) {
    char kx[96], ky[96];
    LayoutKey(kx, sizeof(kx), id, "X");
    LayoutKey(ky, sizeof(ky), id, "Y");
    CVarSetFloat(kx, std::clamp(center.x / w, 0.0f, 1.0f));
    CVarSetFloat(ky, std::clamp(center.y / h, 0.0f, 1.0f));
}

void ClearPos(const char* id) {
    char kx[96], ky[96];
    LayoutKey(kx, sizeof(kx), id, "X");
    LayoutKey(ky, sizeof(ky), id, "Y");
    CVarClear(kx);
    CVarClear(ky);
}

void RegisterCVars() {
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    CVarRegisterInteger(CVAR_TOUCH("Enabled"), TOUCH_CONTROLS_DEFAULT);
    CVarRegisterFloat(CVAR_TOUCH("Scale"), 1.0f);
    CVarRegisterFloat(CVAR_TOUCH("Opacity"), 0.8f);
    CVarSetInteger(CVAR_TOUCH("EditMode"), 0);
}

bool Enabled() {
    RegisterCVars();
    return CVarGetInteger(CVAR_TOUCH("Enabled"), TOUCH_CONTROLS_DEFAULT) != 0;
}

float Unit() {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    return std::min<float>(display.x, display.y) * 0.055f *
           std::clamp(CVarGetFloat(CVAR_TOUCH("Scale"), 1.0f), 0.5f, 2.0f);
}

void ComputeLayout(OverlayState& state) {
    const ImVec2 display = ImGui::GetIO().DisplaySize;
    const float w = display.x;
    const float h = display.y;
    const float u = Unit();
    state.unit = u;

    const ImU32 kGreen = IM_COL32(40, 175, 95, 255);
    const ImU32 kRed = IM_COL32(215, 55, 60, 255);
    const ImU32 kLight = IM_COL32(205, 205, 215, 255);
    const ImU32 kPurple = IM_COL32(120, 85, 200, 255);
    const ImU32 kGray = IM_COL32(140, 140, 155, 255);

    const auto make = [w, h](uint16_t mask, const char* id, const char* label, Shape shape, ImU32 color,
                             ImVec2 center, float halfW, float halfH, float dirX = 0.0f, float dirY = 0.0f) {
        TouchButton b {};
        b.mask = mask;
        b.id = id;
        b.label = label;
        b.shape = shape;
        b.color = color;
        b.center = LoadPos(id, center, w, h);
        b.halfW = halfW;
        b.halfH = halfH;
        b.dirX = dirX;
        b.dirY = dirY;
        return b;
    };

    state.control.rest = LoadPos("Stick", ImVec2(4.8f * u, h - 4.8f * u), w, h);
    state.control.travel = 2.4f * u;
    state.control.nub = 1.6f * u;
    if (!state.control.held) {
        state.control.anchor = state.control.rest;
        state.control.pos = state.control.rest;
    }

    state.cstick.rest = LoadPos("CStick", ImVec2(w - 9.0f * u, h - 6.2f * u), w, h);
    state.cstick.travel = 1.3f * u;
    state.cstick.nub = 0.85f * u;
    state.cstick.anchor = state.cstick.rest;
    if (!state.cstick.held) {
        state.cstick.pos = state.cstick.rest;
    }

    state.menuCenter = LoadPos("Menu", ImVec2(2.0f * u, 1.8f * u), w, h);
    state.menuRadius = 1.0f * u;

    state.editMode = CVarGetInteger(CVAR_TOUCH("EditMode"), 0) != 0;
    state.doneCenter = ImVec2(w * 0.5f + 3.4f * u, 1.6f * u);
    state.doneHalf = ImVec2(2.6f * u, 1.0f * u);
    state.resetCenter = ImVec2(w * 0.5f - 3.4f * u, 1.6f * u);
    state.resetHalf = ImVec2(2.6f * u, 1.0f * u);

    state.gameButtons.clear();
    state.gameButtons.push_back(
        make(BTN_A, "A", "A", Shape::Circle, kGreen, ImVec2(w - 4.6f * u, h - 4.2f * u), 1.7f * u, 1.7f * u));
    state.gameButtons.push_back(
        make(BTN_B, "B", "B", Shape::Circle, kRed, ImVec2(w - 7.5f * u, h - 2.5f * u), 1.05f * u, 1.05f * u));
    state.gameButtons.push_back(
        make(BTN_X, "X", "X", Shape::Pill, kLight, ImVec2(w - 1.9f * u, h - 5.6f * u), 0.75f * u, 1.15f * u));
    state.gameButtons.push_back(
        make(BTN_Y, "Y", "Y", Shape::Pill, kLight, ImVec2(w - 5.0f * u, h - 7.4f * u), 1.15f * u, 0.75f * u));
    state.gameButtons.push_back(
        make(BTN_L, "L", "L", Shape::Pill, kGray, ImVec2(7.0f * u, 1.7f * u), 1.8f * u, 0.6f * u));
    state.gameButtons.push_back(
        make(BTN_R, "R", "R", Shape::Pill, kGray, ImVec2(w - 4.2f * u, 1.7f * u), 1.8f * u, 0.6f * u));
    state.gameButtons.push_back(
        make(BTN_Z, "Z", "Z", Shape::Pill, kPurple, ImVec2(w - 4.2f * u, 3.5f * u), 1.4f * u, 0.55f * u));
    state.gameButtons.push_back(make(BTN_START, "Start", "START", Shape::Pill, kGray,
                                     ImVec2(w * 0.5f, h - 1.6f * u), 1.3f * u, 0.5f * u));

    const ImVec2 d(3.4f * u, h - 10.2f * u);
    const float dOff = 1.0f * u;
    const float dArm = 0.55f * u;
    state.gameButtons.push_back(make(BTN_DUP, "DUp", "", Shape::Arrow, kGray, ImVec2(d.x, d.y - dOff), dArm, dArm,
                                     0.0f, -1.0f));
    state.gameButtons.push_back(make(BTN_DDOWN, "DDown", "", Shape::Arrow, kGray, ImVec2(d.x, d.y + dOff), dArm,
                                     dArm, 0.0f, 1.0f));
    state.gameButtons.push_back(make(BTN_DLEFT, "DLeft", "", Shape::Arrow, kGray, ImVec2(d.x - dOff, d.y), dArm,
                                     dArm, -1.0f, 0.0f));
    state.gameButtons.push_back(make(BTN_DRIGHT, "DRight", "", Shape::Arrow, kGray, ImVec2(d.x + dOff, d.y), dArm,
                                     dArm, 1.0f, 0.0f));
}

float Dist(const ImVec2& a, const ImVec2& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    return std::sqrt(dx * dx + dy * dy);
}

float HitScore(const TouchButton& b, const ImVec2& p) {
    const float mx = std::max<float>(b.halfW * 1.15f, b.halfH * 1.3f);
    const float my = std::max<float>(b.halfH * 1.3f, b.halfW * 0.9f);
    const float nx = std::fabs(p.x - b.center.x) / mx;
    const float ny = std::fabs(p.y - b.center.y) / my;
    const float score = std::max<float>(nx, ny);
    return score <= 1.0f ? score : -1.0f;
}

bool InRect(const ImVec2& p, const ImVec2& center, const ImVec2& half) {
    return std::fabs(p.x - center.x) <= half.x && std::fabs(p.y - center.y) <= half.y;
}

bool OnStick(const Stick& stick, const ImVec2& p) {
    return Dist(p, stick.rest) <= stick.travel + stick.nub;
}

bool OnMenu(const OverlayState& state, const ImVec2& p) {
    return Dist(p, state.menuCenter) <= state.menuRadius * 1.3f;
}

void SaveDrag(const OverlayState& state, float w, float h) {
    if (sDragTarget >= 0 && sDragTarget < (int)state.gameButtons.size()) {
        SavePos(state.gameButtons[sDragTarget].id, sDragPos, w, h);
    } else if (sDragTarget == kDragStick) {
        SavePos("Stick", sDragPos, w, h);
    } else if (sDragTarget == kDragMenu) {
        SavePos("Menu", sDragPos, w, h);
    } else if (sDragTarget == kDragCStick) {
        SavePos("CStick", sDragPos, w, h);
    }
}

void ApplyDrag(OverlayState& state) {
    if (sDragTarget >= 0 && sDragTarget < (int)state.gameButtons.size()) {
        state.gameButtons[sDragTarget].center = sDragPos;
    } else if (sDragTarget == kDragStick) {
        state.control.rest = state.control.anchor = state.control.pos = sDragPos;
    } else if (sDragTarget == kDragMenu) {
        state.menuCenter = sDragPos;
    } else if (sDragTarget == kDragCStick) {
        state.cstick.rest = state.cstick.anchor = state.cstick.pos = sDragPos;
    }
}

void StartDrag(int target, const Finger& finger, ImVec2 center) {
    sDragFinger = finger.id;
    sDragTarget = target;
    sDragOffset = ImVec2(center.x - finger.pos.x, center.y - finger.pos.y);
    sDragPos = center;
}

void HandleEditMode(OverlayState& state, const std::vector<Finger>& fingers, float w, float h) {
    bool doneNow = false;
    bool resetNow = false;
    for (const auto& finger : fingers) {
        if (finger.id != sDragFinger) {
            doneNow |= InRect(finger.pos, state.doneCenter, state.doneHalf);
            resetNow |= InRect(finger.pos, state.resetCenter, state.resetHalf);
        }
    }
    if (sDoneDown && !doneNow) {
        CVarSetInteger(CVAR_TOUCH("EditMode"), 0);
        CVarSave();
        sDragFinger = -1;
        sDragTarget = kDragNone;
        sDoneDown = sResetDown = false;
        return;
    }
    if (sResetDown && !resetNow) {
        for (const auto& button : state.gameButtons) {
            ClearPos(button.id);
        }
        ClearPos("Stick");
        ClearPos("CStick");
        ClearPos("Menu");
        CVarSave();
        sDragFinger = -1;
        sDragTarget = kDragNone;
        sDoneDown = sResetDown = false;
        return;
    }
    sDoneDown = doneNow;
    sResetDown = resetNow;

    if (sDragFinger != -1) {
        const Finger* held = nullptr;
        for (const auto& finger : fingers) {
            if (finger.id == sDragFinger) {
                held = &finger;
                break;
            }
        }
        if (held == nullptr) {
            SaveDrag(state, w, h);
            CVarSave();
            sDragFinger = -1;
            sDragTarget = kDragNone;
            return;
        }
        sDragPos =
            ImVec2(std::clamp(held->pos.x + sDragOffset.x, 0.0f, w), std::clamp(held->pos.y + sDragOffset.y, 0.0f, h));
        ApplyDrag(state);
        return;
    }

    for (const auto& finger : fingers) {
        if (InRect(finger.pos, state.doneCenter, state.doneHalf) ||
            InRect(finger.pos, state.resetCenter, state.resetHalf)) {
            continue;
        }
        for (int i = 0; i < (int)state.gameButtons.size(); i++) {
            if (HitScore(state.gameButtons[i], finger.pos) >= 0.0f) {
                StartDrag(i, finger, state.gameButtons[i].center);
                return;
            }
        }
        if (OnStick(state.cstick, finger.pos)) {
            StartDrag(kDragCStick, finger, state.cstick.rest);
            return;
        }
        if (Dist(finger.pos, state.control.rest) <= state.control.travel) {
            StartDrag(kDragStick, finger, state.control.rest);
            return;
        }
        if (OnMenu(state, finger.pos)) {
            StartDrag(kDragMenu, finger, state.menuCenter);
            return;
        }
    }
}

std::vector<Finger> GatherFingers() {
    std::vector<Finger> fingers;
    const ImVec2 display = ImGui::GetIO().DisplaySize;

    for (int d = 0; d < SDL_GetNumTouchDevices(); d++) {
        const SDL_TouchID device = SDL_GetTouchDevice(d);
        if (device == 0) {
            continue;
        }
        for (int f = 0; f < SDL_GetNumTouchFingers(device); f++) {
            const SDL_Finger* finger = SDL_GetTouchFinger(device, f);
            if (finger != nullptr) {
                fingers.push_back({ ImVec2(finger->x * display.x, finger->y * display.y), finger->id });
            }
        }
    }

#if !defined(__IOS__) && !defined(__ANDROID__)
    const ImGuiIO& io = ImGui::GetIO();
    if (io.MouseDown[ImGuiMouseButton_Left]) {
        const ImVec2 origin = ImGui::GetMainViewport()->Pos;
        fingers.push_back({ ImVec2(io.MousePos.x - origin.x, io.MousePos.y - origin.y), kMouseFingerId });
    }
#endif

    return fingers;
}

const Finger* FindFinger(const std::vector<Finger>& fingers, int64_t id) {
    for (const auto& finger : fingers) {
        if (finger.id == id) {
            return &finger;
        }
    }
    return nullptr;
}

void ReleaseStick(Stick& stick) {
    stick.finger = -1;
    stick.held = false;
    stick.dx = stick.dy = 0.0f;
    stick.anchor = stick.pos = stick.rest;
}

void UpdateStick(Stick& stick, const std::vector<Finger>& fingers) {
    const Finger* finger = stick.finger != -1 ? FindFinger(fingers, stick.finger) : nullptr;
    if (finger == nullptr) {
        ReleaseStick(stick);
        return;
    }
    float dx = (finger->pos.x - stick.anchor.x) / stick.travel;
    float dy = (finger->pos.y - stick.anchor.y) / stick.travel;
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len > 1.0f) {
        dx /= len;
        dy /= len;
    }
    stick.held = true;
    stick.dx = dx;
    stick.dy = dy;
    stick.pos = ImVec2(stick.anchor.x + dx * stick.travel, stick.anchor.y + dy * stick.travel);
}

int8_t StickValue(float v) {
    return (int8_t)std::clamp(v * kStickMax, -kStickMax, kStickMax);
}

void ResetFrame() {
    sState.active = false;
    sState.buttons = 0;
    for (auto& button : sState.gameButtons) {
        button.pressed = false;
    }
}

void OnControllerRead(IEvent* event) {
    ResetFrame();

    if (ImGui::GetCurrentContext() == nullptr || !Enabled()) {
        ReleaseStick(sState.control);
        ReleaseStick(sState.cstick);
        return;
    }

    const auto window = ShipCompat::GetWindow();
    if (window == nullptr || window->GetGui() == nullptr) {
        ReleaseStick(sState.control);
        ReleaseStick(sState.cstick);
        return;
    }

    const auto menu = window->GetGui()->GetMenu();
    sState.menuOpen = menu != nullptr && menu->IsVisible();
    sState.active = true;

    ComputeLayout(sState);
    const std::vector<Finger> fingers = GatherFingers();
    const ImVec2 display = ImGui::GetIO().DisplaySize;

    if (sState.editMode && !sState.menuOpen) {
        ReleaseStick(sState.control);
        ReleaseStick(sState.cstick);
        HandleEditMode(sState, fingers, display.x, display.y);
        return;
    }

    bool menuDownNow = false;
    for (const auto& finger : fingers) {
        if (OnMenu(sState, finger.pos)) {
            menuDownNow = true;
            break;
        }
    }
    if (sState.menuDown && !menuDownNow && menu != nullptr) {
        menu->ToggleVisibility();
        sState.menuOpen = !sState.menuOpen;
    }
    sState.menuDown = menuDownNow;

    if (sState.menuOpen) {
        ReleaseStick(sState.control);
        ReleaseStick(sState.cstick);
        return;
    }

    for (const auto& finger : fingers) {
        if (finger.id == sState.control.finger || finger.id == sState.cstick.finger || OnMenu(sState, finger.pos)) {
            continue;
        }
        int best = -1;
        float bestScore = 2.0f;
        for (int i = 0; i < (int)sState.gameButtons.size(); i++) {
            const float score = HitScore(sState.gameButtons[i], finger.pos);
            if (score >= 0.0f && score < bestScore) {
                bestScore = score;
                best = i;
            }
        }
        if (best >= 0) {
            sState.gameButtons[best].pressed = true;
            sState.buttons |= sState.gameButtons[best].mask;
        } else if (sState.cstick.finger == -1 && OnStick(sState.cstick, finger.pos)) {
            sState.cstick.finger = finger.id;
        } else if (sState.control.finger == -1 && finger.pos.x < display.x * 0.45f &&
                   finger.pos.y > display.y * 0.25f) {
            sState.control.finger = finger.id;
            sState.control.anchor = finger.pos;
        }
    }

    UpdateStick(sState.control, fingers);
    UpdateStick(sState.cstick, fingers);

    if (sState.cstick.held) {
        if (sState.cstick.dx <= -kCStickThreshold) {
            sState.buttons |= BTN_CLEFT;
        } else if (sState.cstick.dx >= kCStickThreshold) {
            sState.buttons |= BTN_CRIGHT;
        }
        if (sState.cstick.dy <= -kCStickThreshold) {
            sState.buttons |= BTN_CUP;
        } else if (sState.cstick.dy >= kCStickThreshold) {
            sState.buttons |= BTN_CDOWN;
        }
    }

    auto* ev = (ControllerRead*)event;
    const auto controlDeck = ShipCompat::GetControlDeck();
    if (ev->pads == nullptr || ev->count <= 0 || controlDeck == nullptr || controlDeck->GamepadGameInputBlocked()) {
        return;
    }

    OSContPad* pad = &ev->pads[0];
    pad->button |= sState.buttons;
    if (sState.control.held && pad->stick_x == 0 && pad->stick_y == 0) {
        pad->stick_x = StickValue(sState.control.dx);
        pad->stick_y = StickValue(-sState.control.dy);
    }
    if (sState.cstick.held && pad->right_stick_x == 0 && pad->right_stick_y == 0) {
        pad->right_stick_x = StickValue(sState.cstick.dx);
        pad->right_stick_y = StickValue(-sState.cstick.dy);
    }
}

void InitTouchControls() {
    RegisterCVars();
    REGISTER_LISTENER(ControllerRead, EVENT_PRIORITY_NORMAL, OnControllerRead);
}

RegisterShipInitFunc sInitTouchControls(InitTouchControls);

void DrawOverlay(ImDrawList* drawList) {
    const float opacity = std::clamp(CVarGetFloat(CVAR_TOUCH("Opacity"), 0.8f), 0.1f, 1.0f);
    const float u = sState.unit;
    const float stroke = std::max<float>(2.0f, u * 0.09f);
    ImFont* font = ImGui::GetFont();

    const auto alpha = [opacity](float a) {
        return (ImU32)(std::min<float>(a, 1.0f) * opacity * 255.0f) << IM_COL32_A_SHIFT;
    };
    const auto rgb = [](ImU32 c) { return c & ~IM_COL32_A_MASK; };
    const auto dark = [](ImU32 c) {
        const int r = (c >> IM_COL32_R_SHIFT) & 0xFF;
        const int g = (c >> IM_COL32_G_SHIFT) & 0xFF;
        const int b = (c >> IM_COL32_B_SHIFT) & 0xFF;
        return r * 299 + g * 587 + b * 114 > 160000;
    };
    const auto text = [&](const ImVec2& center, float size, const char* s, ImU32 col) {
        const ImVec2 ts = font->CalcTextSizeA(size, FLT_MAX, 0.0f, s);
        drawList->AddText(font, size, ImVec2(center.x - ts.x * 0.5f, center.y - ts.y * 0.5f), col, s);
    };
    const ImU32 white = IM_COL32(255, 255, 255, 0);
    const ImU32 ink = IM_COL32(30, 30, 38, 0);

    drawList->AddCircleFilled(sState.menuCenter, sState.menuRadius, IM_COL32(20, 20, 26, 0) | alpha(0.55f));
    drawList->AddCircle(sState.menuCenter, sState.menuRadius, white | alpha(0.85f), 0, stroke);
    text(sState.menuCenter, sState.menuRadius * 1.1f, "=", white | alpha(0.9f));

    if (sState.menuOpen) {
        return;
    }

    for (const auto& button : sState.gameButtons) {
        const ImU32 fill = rgb(button.color) | alpha(button.pressed ? 1.0f : 0.5f);
        const ImU32 edge = white | alpha(button.pressed ? 1.0f : 0.8f);
        const ImU32 label = (dark(button.color) ? ink : white) | alpha(0.95f);
        const ImVec2 pMin(button.center.x - button.halfW, button.center.y - button.halfH);
        const ImVec2 pMax(button.center.x + button.halfW, button.center.y + button.halfH);
        switch (button.shape) {
            case Shape::Circle:
                drawList->AddCircleFilled(button.center, button.halfW, fill);
                drawList->AddCircle(button.center, button.halfW, edge, 0, stroke);
                text(button.center, button.halfW * 1.1f, button.label, label);
                break;
            case Shape::Pill: {
                const float rounding = std::min<float>(button.halfW, button.halfH);
                drawList->AddRectFilled(pMin, pMax, fill, rounding);
                drawList->AddRect(pMin, pMax, edge, rounding, 0, stroke);
                text(button.center, std::min<float>(button.halfH * 1.4f, button.halfW * 1.1f), button.label, label);
                break;
            }
            case Shape::Arrow: {
                drawList->AddRectFilled(pMin, pMax, fill, u * 0.15f);
                drawList->AddRect(pMin, pMax, edge, u * 0.15f, 0, stroke);
                const float a = button.halfW * 0.55f;
                const ImVec2 tip(button.center.x + button.dirX * a, button.center.y + button.dirY * a);
                const ImVec2 b1(button.center.x - button.dirX * a * 0.6f + button.dirY * a,
                                button.center.y - button.dirY * a * 0.6f + button.dirX * a);
                const ImVec2 b2(button.center.x - button.dirX * a * 0.6f - button.dirY * a,
                                button.center.y - button.dirY * a * 0.6f - button.dirX * a);
                drawList->AddTriangleFilled(tip, b1, b2, white | alpha(0.9f));
                break;
            }
        }
    }

    const auto drawStick = [&](const Stick& stick, ImU32 baseColor, ImU32 nubColor, const char* nubLabel) {
        const float baseAlpha = stick.held ? 0.85f : 0.55f;
        drawList->AddCircleFilled(stick.anchor, stick.travel + stick.nub * 0.5f, rgb(baseColor) | alpha(0.25f));
        drawList->AddCircle(stick.anchor, stick.travel + stick.nub * 0.5f, white | alpha(baseAlpha), 0, stroke);
        drawList->AddCircleFilled(stick.pos, stick.nub, rgb(nubColor) | alpha(baseAlpha));
        drawList->AddCircle(stick.pos, stick.nub, white | alpha(baseAlpha + 0.15f), 0, stroke);
        if (nubLabel != nullptr) {
            text(stick.pos, stick.nub * 1.1f, nubLabel, (dark(nubColor) ? ink : white) | alpha(0.95f));
        }
    };
    drawStick(sState.control, IM_COL32(20, 20, 26, 255), IM_COL32(190, 190, 200, 255), nullptr);
    drawStick(sState.cstick, IM_COL32(120, 95, 10, 255), IM_COL32(250, 200, 40, 255), "C");

    if (sState.editMode) {
        const ImU32 highlight = IM_COL32(255, 255, 255, 220);
        if (sDragTarget >= 0 && sDragTarget < (int)sState.gameButtons.size()) {
            const TouchButton& b = sState.gameButtons[sDragTarget];
            drawList->AddRect(ImVec2(b.center.x - b.halfW - u * 0.3f, b.center.y - b.halfH - u * 0.3f),
                              ImVec2(b.center.x + b.halfW + u * 0.3f, b.center.y + b.halfH + u * 0.3f), highlight,
                              u * 0.2f, 0, stroke);
        } else if (sDragTarget == kDragStick) {
            drawList->AddCircle(sState.control.rest, sState.control.travel + u * 0.3f, highlight, 0, stroke);
        } else if (sDragTarget == kDragCStick) {
            drawList->AddCircle(sState.cstick.rest, sState.cstick.travel + sState.cstick.nub + u * 0.3f, highlight, 0,
                                stroke);
        } else if (sDragTarget == kDragMenu) {
            drawList->AddCircle(sState.menuCenter, sState.menuRadius + u * 0.3f, highlight, 0, stroke);
        }

        const auto pill = [&](const ImVec2& c, const ImVec2& half, ImU32 fillColor, const char* s, bool down) {
            const ImVec2 pMin(c.x - half.x, c.y - half.y);
            const ImVec2 pMax(c.x + half.x, c.y + half.y);
            drawList->AddRectFilled(pMin, pMax, rgb(fillColor) | (down ? IM_COL32_A_MASK : alpha(0.85f)), half.y);
            drawList->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 230), half.y, 0, stroke);
            text(c, half.y, s, IM_COL32(255, 255, 255, 255));
        };
        pill(sState.doneCenter, sState.doneHalf, IM_COL32(40, 170, 80, 255), "Done", sDoneDown);
        pill(sState.resetCenter, sState.resetHalf, IM_COL32(120, 120, 130, 255), "Reset", sResetDown);

        const ImVec2 display = ImGui::GetIO().DisplaySize;
        text(ImVec2(display.x * 0.5f, 3.6f * u), u * 0.9f, "Drag controls to reposition them",
             IM_COL32(255, 255, 255, 200));
    }
}

} // namespace

namespace PortGui {

void TouchControlsOverlay::Draw() {
    if (!sState.active || !Enabled()) {
        return;
    }

    ImDrawList* drawList = ImGui::GetForegroundDrawList();
    const int vtxStart = drawList->VtxBuffer.Size;
    drawList->PushClipRect(ImVec2(-100000.0f, -100000.0f), ImVec2(100000.0f, 100000.0f));
    DrawOverlay(drawList);
    drawList->PopClipRect();
    const ImVec2 origin = ImGui::GetMainViewport()->Pos;
    if (origin.x != 0.0f || origin.y != 0.0f) {
        ImGui::ShadeVertsTransformPos(drawList, vtxStart, drawList->VtxBuffer.Size, ImVec2(0.0f, 0.0f), 1.0f, 0.0f,
                                      origin);
    }
}

} // namespace PortGui
