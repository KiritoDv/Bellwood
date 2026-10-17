#include "PortMenu.h"
#include "port/Enhancements.h"

namespace PortGui {

using namespace UIWidgets;

void PortMenu::AddMenuEnhancements() {
    AddMenuEntry("Enhancements", CVAR_SETTING("Menu.EnhancementsSidebarSection"));

    WidgetPath path = { "Enhancements", "Gameplay", SECTION_COLUMN_1 };
    AddSidebarEntry("Enhancements", path.sidebarName, 1);
    AddWidget(path, "Disable Resetti", WIDGET_CVAR_CHECKBOX)
        .CVar(CVAR_DISABLE_RESETTI)
        .RaceDisable(false)
        .Options(CheckboxOptions().DefaultValue(false).Tooltip(
            "Mr. Resetti no longer shows up after you quit or reset without saving, and those resets are not "
            "counted. Takes effect the next time your town is loaded."));
}

} // namespace PortGui
