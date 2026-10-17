#include "PortGui.hpp"
#include "port/ShipCompat.h"

#include <spdlog/spdlog.h>
#include <imgui.h>
#include <imgui_internal.h>

#ifdef __APPLE__
#include <fast/backends/gfx_metal.h>
#endif

#include "Notification.h"
#include "PortInputEditorWindow.h"
#include "TouchControls.h"

#include <ship/window/gui/ConsoleWindow.h>
#include <ship/window/gui/EventDebuggerWindow.h>
#include <libultraship/bridge/eventsbridge.h>
#if __has_include(<ship/window/gui/ShaderSettingsWindow.h>)
#include <ship/window/gui/ShaderSettingsWindow.h>
#define PORT_HAS_SHADER_SETTINGS 1
#endif
#include <libultraship/window/gui/GfxDebuggerWindow.h>

namespace PortGui {
// MARK: - Delegates
std::shared_ptr<Ship::GuiWindow> mInputEditorWindow;
std::shared_ptr<PortMenu> mPortMenu;
std::shared_ptr<Notification::Window> mNotificationWindow;
std::shared_ptr<InputViewer> mInputViewer;
std::shared_ptr<InputViewerSettingsWindow> mInputViewerSettings;
std::shared_ptr<PortModalWindow> mModalWindow;
std::shared_ptr<TouchControlsOverlay> mTouchControlsOverlay;
std::shared_ptr<Ship::GuiWindow> mConsoleWindow;
std::shared_ptr<Ship::EventDebuggerWindow> mEventDebuggerWindow;
std::shared_ptr<LUS::GfxDebuggerWindow> mGfxDebuggerWindow;
#ifdef PORT_HAS_SHADER_SETTINGS
std::shared_ptr<Ship::ShaderSettingsWindow> mShaderSettingsWindow;
#endif

UIWidgets::Colors GetMenuThemeColor() {
    return mPortMenu->GetMenuThemeColor();
}

void SetupMenu() {
    auto gui = ShipCompat::GetWindow()->GetGui();
    mPortMenu = std::make_shared<PortGui::PortMenu>(CVAR_WINDOW("Menu"), "Port Menu");
    gui->SetMenu(mPortMenu);

    mModalWindow = std::make_shared<PortModalWindow>(CVAR_WINDOW("ModalWindow"), "Modal Window");
    gui->AddGuiWindow(mModalWindow);
    mModalWindow->Show();
}

void SetupGuiElements() {
    auto gui = ShipCompat::GetWindow()->GetGui();

    auto& style = ImGui::GetStyle();
    style.FramePadding = ImVec2(4.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.Colors[ImGuiCol_MenuBarBg] = UIWidgets::ColorValues.at(UIWidgets::Colors::DarkGray);

    mConsoleWindow = std::make_shared<Ship::ConsoleWindow>(CVAR_WINDOW("DevConsole"), "Console##Dev", ImVec2(820, 630));
    gui->AddGuiWindow(mConsoleWindow);

    mEventDebuggerWindow = std::make_shared<Ship::EventDebuggerWindow>(ShipCompat::GetConsoleVariables(),
                                                                       ShipCompat::GetWindow(), EventSystemGetEvents(),
                                                                       CVAR_WINDOW("EventDebugger"), "Event Debugger");
    gui->AddGuiWindow(mEventDebuggerWindow);

    if (gui->GetGuiWindow("Gfx Debugger") == nullptr) {
        mGfxDebuggerWindow = std::make_shared<LUS::GfxDebuggerWindow>(
            CVAR_WINDOW("GfxDebugger"), "Gfx Debugger",
            std::dynamic_pointer_cast<Fast::Fast3dWindow>(ShipCompat::GetWindow()), ShipCompat::GetGfxDebugger(),
            ShipCompat::GetResourceManager());
        gui->AddGuiWindow(mGfxDebuggerWindow);
    }

    mPortMenu = std::make_shared<PortMenu>(CVAR_WINDOW("Menu"), "Settings Menu");
    gui->SetMenu(mPortMenu);

    mInputEditorWindow =
        std::make_shared<PortInputEditorWindow>(CVAR_WINDOW("ControllerConfiguration"), "Configure Controller");
    gui->AddGuiWindow(mInputEditorWindow);

    mNotificationWindow = std::make_shared<Notification::Window>(CVAR_WINDOW("Notifications"), "Notifications Window");
    gui->AddGuiWindow(mNotificationWindow);
    mNotificationWindow->Show();

    mInputViewer = std::make_shared<InputViewer>(CVAR_WINDOW("InputViewer"), "Input Viewer");
    gui->AddGuiWindow(mInputViewer);

    mInputViewerSettings = std::make_shared<InputViewerSettingsWindow>(CVAR_WINDOW("InputViewerSettings"),
                                                                       "Input Viewer Settings", ImVec2(500, 525));
    gui->AddGuiWindow(mInputViewerSettings);

    mTouchControlsOverlay = std::make_shared<TouchControlsOverlay>(CVAR_WINDOW("TouchControls"), "##TouchControls");
    gui->AddGuiWindow(mTouchControlsOverlay);
    mTouchControlsOverlay->Show();

#ifdef PORT_HAS_SHADER_SETTINGS
    mShaderSettingsWindow = std::make_shared<Ship::ShaderSettingsWindow>(CVAR_WINDOW("ShaderSettings"),
                                                                         "Shader Settings", ImVec2(420, 520));
    gui->AddGuiWindow(mShaderSettingsWindow);
#endif
}

void Destroy() {
    auto gui = ShipCompat::GetWindow()->GetGui();

    gui->RemoveAllGuiWindows();
    mPortMenu = nullptr;
    mModalWindow = nullptr;
    mInputEditorWindow = nullptr;
    mNotificationWindow = nullptr;
    mInputViewer = nullptr;
    mInputViewerSettings = nullptr;
    mTouchControlsOverlay = nullptr;
    mConsoleWindow = nullptr;
#ifdef PORT_HAS_SHADER_SETTINGS
    mShaderSettingsWindow = nullptr;
#endif
    mGfxDebuggerWindow = nullptr;
    mEventDebuggerWindow = nullptr;
}

void RegisterPopup(std::string title, std::string message, std::string button1, std::string button2,
                   std::function<void()> button1callback, std::function<void()> button2callback) {
    mModalWindow->RegisterPopup(title, message, button1, button2, button1callback, button2callback);
}

size_t PopupsQueued() {
    return mModalWindow->PopupsQueued();
}

} // namespace PortGui
