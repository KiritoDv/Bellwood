#pragma once

#include "port/ui/cvar_prefixes.h"

#include <ship/window/gui/GuiWindow.h>

#define CVAR_TOUCH(var) CVAR_SETTING("TouchControls." var)

namespace PortGui {

class TouchControlsOverlay final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void Draw() override;
    void DrawElement() override {};
    void UpdateElement() override {};
};

} // namespace PortGui
