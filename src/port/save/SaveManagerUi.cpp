#include "port/save/SaveManager.h"

#include "port/ShipCompat.h"
#include "port/ShipInit.hpp"
#include "port/events/Events.h"
#include "port/portable-file-dialogs.h"
#include "port/ui/cvar_prefixes.h"

#include <SDL.h>
#include <imgui.h>
#include <ship/debug/Console.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <spdlog/spdlog.h>

#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

const char* const kPickerTitle = "Visit another town##SaveManagerPicker";
const char* const kQuitTitle = "Quit##SaveManagerQuit";

class SaveManagerWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void Draw() override {
        if (!IsVisible()) {
            return;
        }
        DrawElement();
    }

    void DrawElement() override {
        DrawPicker();
        DrawQuitPrompt();
    }

    void UpdateElement() override {
    }

  private:
    void OpenPicker() {
        mEntries = SaveManager::ListTownFiles();
        const char* current = SaveManager_GetTownFile();
        std::snprintf(mPath, sizeof(mPath), "%s", current != nullptr ? current : "");
        mError.clear();
        mPickerOpen = true;
    }

    void ClosePicker(bool picked) {
        mOpenDialog.reset();
        mSaveDialog.reset();
        mPickerOpen = false;
        SaveManager::CloseTownPicker(picked);
        ImGui::CloseCurrentPopup();
    }

    void PollDialogs() {
        if (mOpenDialog && mOpenDialog->ready(0)) {
            auto files = mOpenDialog->result();
            if (!files.empty()) {
                std::snprintf(mPath, sizeof(mPath), "%s", files[0].c_str());
            }
            mOpenDialog.reset();
        }
        if (mSaveDialog && mSaveDialog->ready(0)) {
            std::string file = mSaveDialog->result();
            if (!file.empty()) {
                std::snprintf(mPath, sizeof(mPath), "%s", file.c_str());
            }
            mSaveDialog.reset();
        }
    }

    void DrawPicker() {
        if (!SaveManager_IsPickingTownFile()) {
            mPickerOpen = false;
            return;
        }
        if (!mPickerOpen) {
            OpenPicker();
        }
        if (!ImGui::IsPopupOpen(kPickerTitle)) {
            ImGui::OpenPopup(kPickerTitle);
        }
        ImGui::SetNextWindowSize(ImVec2(620, 0), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal(kPickerTitle, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
            return;
        }
        PollDialogs();

        ImGui::TextWrapped("Pick the save file of the town to visit by train. To travel with a passport instead, "
                           "name a new file: the station writes your travel data into it.");
        ImGui::Spacing();
        ImGui::Text("Town files (%s):", SaveManager::TownDirectory().c_str());
        if (ImGui::BeginListBox("##towns", ImVec2(-FLT_MIN, 8 * ImGui::GetTextLineHeightWithSpacing()))) {
            if (mEntries.empty()) {
                ImGui::TextDisabled("No town files yet. Copy another town's town.json here.");
            }
            for (const auto& e : mEntries) {
                bool selected = e.path == mPath;
                if (ImGui::Selectable(e.label.c_str(), selected)) {
                    std::snprintf(mPath, sizeof(mPath), "%s", e.path.c_str());
                }
            }
            ImGui::EndListBox();
        }
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputTextWithHint("##path", "Path of a town or passport file", mPath, sizeof(mPath));

        if (ImGui::Button("Browse...") && !mOpenDialog) {
            mOpenDialog = std::make_unique<pfd::open_file>("Pick a town file", SaveManager::TownDirectory(),
                                                           std::vector<std::string>{ "Town saves", "*.json" });
        }
        ImGui::SameLine();
        if (ImGui::Button("New passport file...") && !mSaveDialog) {
            mSaveDialog = std::make_unique<pfd::save_file>("Passport file",
                                                           SaveManager::TownDirectory() + "/passport.json",
                                                           std::vector<std::string>{ "Town saves", "*.json" });
        }
        ImGui::SameLine();
        if (ImGui::Button("Refresh")) {
            mEntries = SaveManager::ListTownFiles();
        }

        if (!mError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", mError.c_str());
        }
        ImGui::Separator();

        if (ImGui::Button("Use this file")) {
            if (mPath[0] == '\0') {
                mError = "Pick or name a file first.";
            } else {
                SaveManager_SetTownFile(mPath);
                const char* now = SaveManager_GetTownFile();
                if (now != nullptr && std::strcmp(now, mPath) == 0) {
                    ClosePicker(true);
                } else {
                    mError = "That is your own town's file.";
                }
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("No other town")) {
            SaveManager_SetTownFile("");
            ClosePicker(false);
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            ClosePicker(SaveManager_GetTownFile() != nullptr);
        }
        ImGui::EndPopup();
    }

    void DrawQuitPrompt() {
        SaveManager::QuitPrompt prompt = SaveManager::GetQuitPrompt();
        if (prompt == SaveManager::QuitPrompt::None) {
            return;
        }
        if (!ImGui::IsPopupOpen(kQuitTitle)) {
            ImGui::OpenPopup(kQuitTitle);
        }
        if (!ImGui::BeginPopupModal(kQuitTitle, nullptr,
                                    ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
            return;
        }
        SaveManager::NotePromptDrawn();
        bool unsafe = SaveManager::UnsafeQuitAllowed();
        switch (prompt) {
            case SaveManager::QuitPrompt::SaveAndQuit:
                ImGui::Text("Save your town before quitting?");
                ImGui::TextDisabled("Quitting without saving counts as a reset.");
                if (ImGui::Button("Save & quit")) {
                    SaveManager::RequestSaveAndQuit();
                }
                break;
            case SaveManager::QuitPrompt::Visiting:
                ImGui::Text("You are visiting another town.");
                ImGui::TextDisabled("Take the train home before quitting, or your travel is lost.");
                break;
            case SaveManager::QuitPrompt::Saving:
                ImGui::Text("Saving...");
                break;
            case SaveManager::QuitPrompt::Failed:
                ImGui::Text("Saving failed. Your town was not saved.");
                if (ImGui::Button("Retry")) {
                    SaveManager::RequestSaveAndQuit();
                }
                break;
            default:
                break;
        }
        if (prompt != SaveManager::QuitPrompt::Saving) {
            if (unsafe) {
                ImGui::SameLine();
                if (ImGui::Button("Quit without saving")) {
                    SaveManager::QuitWithoutSaving();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) {
                SaveManager::DismissQuitPrompt();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::EndPopup();
    }

    bool mPickerOpen = false;
    char mPath[1024] = {};
    std::string mError;
    std::vector<SaveManager::PickerEntry> mEntries;
    std::unique_ptr<pfd::open_file> mOpenDialog;
    std::unique_ptr<pfd::save_file> mSaveDialog;
};

Uint32 sMainWindowId = 0;

int SDLCALL QuitFilter(void* userdata, SDL_Event* event) {
    bool close = event->type == SDL_QUIT ||
                 (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_CLOSE &&
                  (sMainWindowId == 0 || event->window.windowID == sMainWindowId));
    if (!close) {
        return 1;
    }
    return SaveManager::OnQuitRequest() ? 1 : 0;
}

void InstallQuitFilter() {
    if (SDL_WasInit(SDL_INIT_VIDEO) == 0) {
        SPDLOG_WARN("save: no SDL video, window close is not intercepted");
        return;
    }
    for (Uint32 id = 1; id < 64; id++) {
        if (SDL_GetWindowFromID(id) != nullptr) {
            sMainWindowId = id;
            break;
        }
    }
    SDL_SetEventFilter(QuitFilter, nullptr);
}

void RegisterCommands() {
    auto console = ShipCompat::GetConsole();
    if (!console) {
        return;
    }
    console->AddCommand("save_selftest",
                        { [](std::shared_ptr<Ship::Console>, std::vector<std::string> args, std::string* output) {
                             std::string report;
                             bool ok = SaveManager::RoundTripSelfTest(&report);
                             if (args.size() > 1 && args[1] == "io") {
                                 std::string io;
                                 ok &= SaveManager::IoSelfTest(&io);
                                 report += io;
                             }
                             SPDLOG_INFO("save_selftest:\n{}", report);
                             if (output != nullptr) {
                                 *output = report;
                             }
                             return ok ? 0 : 1;
                         },
                          "Round-trips the live save through the JSON adapters; 'io' also saves and loads it",
                          { { "io", Ship::ArgumentType::TEXT, true } } });
    console->AddCommand("save_pick_town",
                        { [](std::shared_ptr<Ship::Console>, std::vector<std::string>, std::string*) {
                             SaveManager_PickTownFile();
                             return 0;
                         },
                          "Picks the second town file (slot B)",
                          {} });
}

bool sQuitTestPushed = false;

void OnFrame(IEvent*) {
    SaveManager::Tick();
    if (!SaveManager::QuitTestArmed()) {
        return;
    }
    if (!sQuitTestPushed) {
        sQuitTestPushed = true;
        SDL_Event quit = {};
        quit.type = SDL_QUIT;
        SDL_PushEvent(&quit);
        SPDLOG_INFO("save: quit test sent SDL_QUIT, prompt {}", (int)SaveManager::GetQuitPrompt());
    } else if (SaveManager::GetQuitPrompt() == SaveManager::QuitPrompt::SaveAndQuit) {
        SPDLOG_INFO("save: quit test chooses Save & quit");
        SaveManager::RequestSaveAndQuit();
    }
}

void InitSaveManagerUi() {
    auto window = ShipCompat::GetWindow();
    if (!window || !window->GetGui()) {
        return;
    }
    auto saveWindow = std::make_shared<SaveManagerWindow>(CVAR_WINDOW("SaveManager"), "Save Manager");
    window->GetGui()->AddGuiWindow(saveWindow);
    saveWindow->Show();
    InstallQuitFilter();
    RegisterCommands();
    REGISTER_LISTENER(KeyboardInput, EVENT_PRIORITY_LOW, OnFrame);
}

RegisterShipInitFunc sInitSaveManagerUi(InitSaveManagerUi);

} // namespace
