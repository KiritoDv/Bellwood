#ifdef ENABLE_SCRIPTING

#include "ui.h"

#include "port/ui/MenuTypes.h"
#include "port/ui/PortGui.hpp"
#include "port/ui/UIWidgets.hpp"

#include <algorithm>
#include <unordered_map>
#include <vector>

namespace {

std::vector<C_GuiDrawFunc> sGuiDrawCallbacks;

UIWidgets::ComponentAlignments MapAlign(C_ComponentAlign align) {
    return align == C_ALIGN_RIGHT ? UIWidgets::ComponentAlignments::Right : UIWidgets::ComponentAlignments::Left;
}

UIWidgets::LabelPositions MapLabel(C_LabelPos pos) {
    return pos == C_LABEL_FAR ? UIWidgets::LabelPositions::Far : UIWidgets::LabelPositions::Near;
}

const char* Tooltip(const char* tooltip) {
    return tooltip != nullptr ? tooltip : "";
}

} // namespace

void C_RunGuiDrawCallbacks() {
    for (auto& callback : sGuiDrawCallbacks) {
        callback();
    }
}

extern "C" void C_AddGuiDraw(C_GuiDrawFunc callback) {
    if (callback != nullptr) {
        sGuiDrawCallbacks.push_back(callback);
    }
}

extern "C" void C_RemoveGuiDraw(C_GuiDrawFunc callback) {
    sGuiDrawCallbacks.erase(std::remove(sGuiDrawCallbacks.begin(), sGuiDrawCallbacks.end(), callback),
                            sGuiDrawCallbacks.end());
}

extern "C" void C_AddSidebarEntry(const char* label, int column) {
    if (PortGui::mPortMenu == nullptr || label == nullptr) {
        return;
    }
    PortGui::mPortMenu->AddSidebarEntry("Mods", label, column);
}

extern "C" void C_RemoveSidebarEntry(const char* sidebarName) {
    if (PortGui::mPortMenu == nullptr || sidebarName == nullptr) {
        return;
    }
    PortGui::mPortMenu->RemoveSidebarEntry("Mods", sidebarName);
}

extern "C" void C_AddWidget(const char* sidebar, int column, const char* label, C_WidgetConfig* config) {
    if (PortGui::mPortMenu == nullptr || sidebar == nullptr || label == nullptr || config == nullptr) {
        return;
    }

    WidgetPath path = { "Mods", sidebar, static_cast<SectionColumns>(column) };
    WidgetInfo& widget = PortGui::mPortMenu->AddWidget(path, label, static_cast<WidgetType>(config->type))
                             .RaceDisable(config->race_disable)
                             .SameLine(config->same_line)
                             .HideInSearch(config->hide_in_search);

    if (config->cvar != nullptr) {
        widget.CVar(config->cvar);
    }
    if (config->window_name != nullptr) {
        widget.WindowName(config->window_name);
    }
    if (config->callback != nullptr) {
        widget.Callback([config](WidgetInfo&) { config->callback(); });
    }
    if (config->pre_func != nullptr) {
        widget.PreFunc([config](WidgetInfo&) { config->pre_func(); });
    }
    if (config->post_func != nullptr) {
        widget.PostFunc([config](WidgetInfo&) { config->post_func(); });
    }

    switch (config->type) {
        case C_WIDGET_CVAR_COMBOBOX:
        case C_WIDGET_COMBOBOX: {
            auto options = UIWidgets::ComboboxOptions()
                               .Tooltip(Tooltip(config->opts.combo.tooltip))
                               .DefaultIndex(config->opts.combo.default_index)
                               .ComponentAlignment(MapAlign(config->opts.combo.alignment))
                               .LabelPosition(MapLabel(config->opts.combo.label_pos));
            if (config->opts.combo.map != nullptr) {
                std::unordered_map<int32_t, const char*> comboMap;
                for (C_ComboboxOption* it = config->opts.combo.map; it->value != nullptr; it++) {
                    comboMap[it->id] = it->value;
                }
                options.ComboMap(comboMap);
            }
            widget.Options(options);
            break;
        }
        case C_WIDGET_AUDIO_BACKEND:
        case C_WIDGET_VIDEO_BACKEND:
            widget.Options(UIWidgets::ComboboxOptions()
                               .Tooltip(Tooltip(config->opts.combo.tooltip))
                               .DefaultIndex(config->opts.combo.default_index)
                               .ComponentAlignment(MapAlign(config->opts.combo.alignment))
                               .LabelPosition(MapLabel(config->opts.combo.label_pos)));
            break;
        case C_WIDGET_SLIDER_INT:
        case C_WIDGET_CVAR_SLIDER_INT: {
            auto options = UIWidgets::IntSliderOptions()
                               .Tooltip(Tooltip(config->opts.slider_int.tooltip))
                               .Min(config->opts.slider_int.min)
                               .Max(config->opts.slider_int.max)
                               .DefaultValue(config->opts.slider_int.default_val)
                               .Step(config->opts.slider_int.step);
            if (config->opts.slider_int.format != nullptr) {
                options.Format(config->opts.slider_int.format);
            }
            widget.Options(options);
            break;
        }
        case C_WIDGET_SLIDER_FLOAT:
        case C_WIDGET_CVAR_SLIDER_FLOAT: {
            auto options = UIWidgets::FloatSliderOptions()
                               .Tooltip(Tooltip(config->opts.slider_float.tooltip))
                               .Min(config->opts.slider_float.min)
                               .Max(config->opts.slider_float.max)
                               .DefaultValue(config->opts.slider_float.default_val)
                               .Step(config->opts.slider_float.step);
            if (config->opts.slider_float.format != nullptr) {
                options.Format(config->opts.slider_float.format);
            }
            widget.Options(options);
            break;
        }
        case C_WIDGET_CHECKBOX:
        case C_WIDGET_CVAR_CHECKBOX:
            widget.Options(UIWidgets::CheckboxOptions()
                               .Tooltip(Tooltip(config->opts.checkbox.tooltip))
                               .DefaultValue(config->opts.checkbox.default_val));
            break;
        case C_WIDGET_TEXT:
        case C_WIDGET_SEPARATOR_TEXT:
            widget.Options(UIWidgets::TextOptions());
            break;
        case C_WIDGET_COLOR_PICKER:
        case C_WIDGET_CVAR_COLOR_PICKER:
            widget.Options(UIWidgets::ColorPickerOptions().Tooltip(Tooltip(config->opts.color_picker.tooltip)));
            break;
        case C_WIDGET_WINDOW_BUTTON:
            widget.Options(UIWidgets::WindowButtonOptions().Tooltip(Tooltip(config->opts.generic.tooltip)));
            break;
        case C_WIDGET_BUTTON:
            widget.Options(UIWidgets::ButtonOptions().Tooltip(Tooltip(config->opts.generic.tooltip)));
            break;
        default:
            widget.Options(UIWidgets::WidgetOptions().Tooltip(Tooltip(config->opts.generic.tooltip)));
            break;
    }
}

#endif
