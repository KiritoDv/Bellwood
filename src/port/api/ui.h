#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "api.h"

typedef enum {
    C_WIDGET_CHECKBOX,
    C_WIDGET_COMBOBOX,
    C_WIDGET_SLIDER_INT,
    C_WIDGET_SLIDER_FLOAT,
    C_WIDGET_CVAR_CHECKBOX,
    C_WIDGET_CVAR_COMBOBOX,
    C_WIDGET_CVAR_SLIDER_INT,
    C_WIDGET_CVAR_SLIDER_FLOAT,
    C_WIDGET_BUTTON,
    C_WIDGET_INPUT,
    C_WIDGET_CVAR_INPUT,
    C_WIDGET_CVAR_COLOR_PICKER,
    C_WIDGET_COLOR_PICKER,
    C_WIDGET_SEARCH,
    C_WIDGET_SEPARATOR,
    C_WIDGET_SEPARATOR_TEXT,
    C_WIDGET_TEXT,
    C_WIDGET_WINDOW_BUTTON,
    C_WIDGET_AUDIO_BACKEND,
    C_WIDGET_VIDEO_BACKEND,
    C_WIDGET_CUSTOM,
} C_WidgetType;

typedef enum { C_ALIGN_LEFT, C_ALIGN_RIGHT } C_ComponentAlign;
typedef enum { C_LABEL_NEAR, C_LABEL_FAR } C_LabelPos;

typedef struct {
    int id;
    const char* value;
} C_ComboboxOption;

typedef struct {
    const char* tooltip;
    int default_index;
    int combo_map_id;
    C_ComponentAlign alignment;
    C_LabelPos label_pos;
    C_ComboboxOption* map;
} C_ComboboxOpts;

typedef struct {
    const char* tooltip;
    int min, max, default_val, step;
    const char* format;
} C_SliderIntOpts;

typedef struct {
    const char* tooltip;
    float min, max, default_val, step;
    const char* format;
} C_SliderFloatOpts;

typedef struct {
    const char* tooltip;
    bool default_val;
} C_CheckboxOpts;

typedef struct {
    const char* text;
    bool center;
} C_TextOpts;

typedef struct {
    const char* tooltip;
    uint8_t default_r, default_g, default_b, default_a;
} C_ColorPickerOpts;

typedef struct {
    const char* tooltip;
} C_ButtonOpts;

typedef struct {
    C_WidgetType type;
    const char* cvar;
    const char* window_name;

    bool race_disable;
    bool same_line;
    bool hide_in_search;
    bool is_hidden;

    void (*callback)(void);
    void (*pre_func)(void);
    void (*post_func)(void);

    union {
        C_ComboboxOpts combo;
        C_SliderIntOpts slider_int;
        C_SliderFloatOpts slider_float;
        C_CheckboxOpts checkbox;
        C_TextOpts text;
        C_ColorPickerOpts color_picker;
        C_ButtonOpts generic;
    } opts;
} C_WidgetConfig;

BW_API void C_AddSidebarEntry(const char* label, int column);
BW_API void C_AddWidget(const char* sidebar, int column, const char* label, C_WidgetConfig* config);
BW_API void C_RemoveSidebarEntry(const char* sidebarName);

typedef void (*C_GuiDrawFunc)(void);
BW_API void C_AddGuiDraw(C_GuiDrawFunc callback);
BW_API void C_RemoveGuiDraw(C_GuiDrawFunc callback);
