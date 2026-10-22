#pragma once

#include <libultraship/libultraship.h>
#include "UIWidgets.hpp"
#include "Menu.h"
#include <fast/backends/gfx_rendering_api.h>
#include "port/ui/cvar_prefixes.h"

namespace PortGui {
class PortMenu : public Ship::Menu {
  public:
    PortMenu(const std::string& consoleVariable, const std::string& name);

    void OnInit(const nlohmann::json& initArgs = nlohmann::json::object()) override;
    void DrawElement() override;
    void UpdateElement() override;
    void Draw() override;

    void AddSidebarEntry(std::string sectionName, std::string sidbarName, uint32_t columnCount);
    void RemoveMenuEntry(std::string entryName);
    void RemoveSidebarEntry(std::string sectionName, std::string sidebarName);
    WidgetInfo& AddWidget(WidgetPath& pathInfo, std::string widgetName, WidgetType widgetType);
    void RemoveWidget(const WidgetPath& pathInfo, const std::string& widgetName);
    void AddMenuSettings();
    void AddMenuEnhancements();
    void AddMenuDevTools();
    void AddMenuShaderSettings();
#ifdef ENABLE_SCRIPTING
    void AddModMenu();
#endif

  private:
#ifdef ENABLE_SCRIPTING
    bool mModMenuAdded = false;
#endif
    char mGitCommitHashTruncated[8];
    bool mIsTaggedVersion;
};
} // namespace PortGui
