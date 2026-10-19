#include "port/save/SaveManager.h"
#include "port/gba/GbaPort.h"

#include "port/ShipCompat.h"
#include "port/ShipInit.hpp"
#include "port/events/Events.h"
#include "port/portable-file-dialogs.h"
#include "port/ui/cvar_prefixes.h"

#include <SDL.h>
#include <imgui.h>
#include <ship/Context.h>
#include <ship/debug/Console.h>
#include <ship/window/FileDrop.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <spdlog/spdlog.h>
#ifdef __EMSCRIPTEN__
#include "port/web/WebUtils.h"
#endif

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#if defined(__IOS__) || defined(__ANDROID__) || defined(__EMSCRIPTEN__)
#define SAVE_UI_FILE_DIALOGS 0
#else
#define SAVE_UI_FILE_DIALOGS 1
#endif

namespace {

bool IsGameCubeSaveFile(const std::filesystem::path& path) {
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return ext == ".gci" || ext == ".gcs" || ext == ".raw" || ext == ".gcp";
}

std::filesystem::path ImportDirectory() {
    return Ship::Context::GetPathRelativeToAppDirectory("import");
}

const char* const kPickerTitle = "Visit another town##SaveManagerPicker";
const char* const kQuitTitle = "Quit##SaveManagerQuit";
const char* const kImportTitle = "Import GameCube save##SaveManagerImport";

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
        DrawImport();
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

#if SAVE_UI_FILE_DIALOGS
        if (ImGui::Button("Browse...") && !mOpenDialog) {
            mOpenDialog = std::make_unique<pfd::open_file>("Pick a town file", SaveManager::TownDirectory(),
                                                           std::vector<std::string>{ "Town saves", "*.json" });
        }
        ImGui::SameLine();
        if (ImGui::Button("New passport file...") && !mSaveDialog) {
            mSaveDialog =
                std::make_unique<pfd::save_file>("Passport file", SaveManager::TownDirectory() + "/passport.json",
                                                 std::vector<std::string>{ "Town saves", "*.json" });
        }
        ImGui::SameLine();
#endif
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
                if (!unsafe && ImGui::Button("Quit anyway")) {
                    SaveManager::QuitWithoutSaving();
                }
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

    void ReadImport() {
        mPreview = SaveManager::PreviewGameCubeSave(mImportPath);
        mImportMessage.clear();
        mImportDone = false;
    }

    void DrawImportFolder() {
        const std::filesystem::path dir = ImportDirectory();
#ifdef __IOS__
        ImGui::TextDisabled("Files in Files > On My iPhone > %s > import:", PORT_NAME);
#else
        ImGui::TextDisabled("Files in %s:", dir.string().c_str());
#endif
        if (ImGui::BeginListBox("##importfiles", ImVec2(-FLT_MIN, 5 * ImGui::GetTextLineHeightWithSpacing()))) {
            std::error_code ec;
            bool any = false;
            for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
                if (!entry.is_regular_file(ec) || !IsGameCubeSaveFile(entry.path())) {
                    continue;
                }
                any = true;
                const std::string path = entry.path().string();
                if (ImGui::Selectable(entry.path().filename().string().c_str(), path == mImportPath)) {
                    std::snprintf(mImportPath, sizeof(mImportPath), "%s", path.c_str());
                    ReadImport();
                }
            }
            if (!any) {
                ImGui::TextDisabled("Copy a .gci, .gcs or .raw file into this folder.");
            }
            ImGui::EndListBox();
        }
    }

    void DrawImport() {
        std::string requested;
        if (SaveManager::TakeImportRequest(&requested)) {
            std::snprintf(mImportPath, sizeof(mImportPath), "%s", requested.c_str());
            mPreview = SaveManager::ImportPreview{};
            mImportMessage.clear();
            mImportDone = false;
            if (!requested.empty()) {
                ReadImport();
            }
            mImportOpen = true;
        }
        if (!mImportOpen) {
            return;
        }
        if (!ImGui::IsPopupOpen(kImportTitle)) {
            ImGui::OpenPopup(kImportTitle);
        }
        ImGui::SetNextWindowSize(ImVec2(620, 0), ImGuiCond_Always);
        if (!ImGui::BeginPopupModal(kImportTitle, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
            return;
        }
        if (mImportDialog && mImportDialog->ready(0)) {
            auto files = mImportDialog->result();
            if (!files.empty()) {
                std::snprintf(mImportPath, sizeof(mImportPath), "%s", files[0].c_str());
                ReadImport();
            }
            mImportDialog.reset();
        }

        ImGui::TextWrapped("Bring a town over from the GameCube: pick a .gci file exported with Dolphin or GCMM, or a "
                           "whole memory card image (.raw). You can also drop the file onto the game window.");
        ImGui::Spacing();
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputTextWithHint("##importpath", "Path of a .gci, .gcs or .raw file", mImportPath,
                                     sizeof(mImportPath), ImGuiInputTextFlags_EnterReturnsTrue)) {
            ReadImport();
        }
#if SAVE_UI_FILE_DIALOGS
        if (ImGui::Button("Browse...") && !mImportDialog) {
            mImportDialog = std::make_unique<pfd::open_file>(
                "Pick a GameCube save", "",
                std::vector<std::string>{ "GameCube saves", "*.gci *.gcs *.raw *.gcp", "All files", "*" });
        }
        ImGui::SameLine();
#elif defined(__EMSCRIPTEN__)
        if (ImGui::Button("Browse...")) {
            WebFiles_Pick("Choose a GameCube save: a .gci or .gcs file, or a memory card image (.raw).",
                          ".gci,.gcs,.raw,.gcp");
        }
        ImGui::SameLine();
#else
        DrawImportFolder();
#endif
        if (ImGui::Button("Read")) {
            ReadImport();
        }
        ImGui::Separator();

        if (!mPreview.ok && !mPreview.error.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", mPreview.error.c_str());
        } else if (mPreview.ok) {
            if (mPreview.land) {
                ImGui::Text("Town: %s", mPreview.town.c_str());
                std::string players;
                for (const auto& name : mPreview.players) {
                    players += (players.empty() ? "" : ", ") + name;
                }
                ImGui::Text("Players: %s", players.empty() ? "none yet" : players.c_str());
            } else {
                ImGui::Text("An erased town: only its letters, designs and diary are kept.");
            }
            ImGui::Text("Letters: %s   Designs: %s   Diary: %s", mPreview.letters ? "yes" : "no",
                        mPreview.designs ? "yes" : "no", mPreview.diary ? "yes" : "no");
            if (mPreview.passports > 0) {
                ImGui::Text("Travel passports: %d", mPreview.passports);
            }
            if (mPreview.famicom) {
                ImGui::Text("NES game saves: yes");
            }
            if (mPreview.nesGames > 0) {
                ImGui::Text("Memory card NES games: %d", mPreview.nesGames);
            }
            if (mPreview.gifts > 0) {
                ImGui::Text("Gift letter files: %d", mPreview.gifts);
            }
            ImGui::Spacing();
            ImGui::TextDisabled("Importing replaces your current town. It is copied to save/backup first.");
        }

        std::string reason;
        bool allowed = SaveManager::CanImportGameCubeSave(&reason);
        if (!allowed) {
            ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "%s", reason.c_str());
        }
        if (!mImportMessage.empty()) {
            ImGui::TextWrapped("%s", mImportMessage.c_str());
        }
        ImGui::Separator();

        ImGui::BeginDisabled(!mPreview.ok || !allowed || mImportDone);
        if (ImGui::Button("Import")) {
            mImportDone = SaveManager::ImportGameCubeSave(mImportPath, &mImportMessage);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (ImGui::Button(mImportDone ? "Done" : "Cancel")) {
            mImportDialog.reset();
            mImportOpen = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    bool mImportOpen = false;
    bool mImportDone = false;
    char mImportPath[1024] = {};
    std::string mImportMessage;
    SaveManager::ImportPreview mPreview;
    std::unique_ptr<pfd::open_file> mImportDialog;

    bool mPickerOpen = false;
    char mPath[1024] = {};
    std::string mError;
    std::vector<SaveManager::PickerEntry> mEntries;
    std::unique_ptr<pfd::open_file> mOpenDialog;
    std::unique_ptr<pfd::save_file> mSaveDialog;
};

Uint32 sMainWindowId = 0;

int SDLCALL QuitFilter(void* userdata, SDL_Event* event) {
    bool close =
        event->type == SDL_QUIT || (event->type == SDL_WINDOWEVENT && event->window.event == SDL_WINDOWEVENT_CLOSE &&
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
    console->AddCommand("save_import",
                        { [](std::shared_ptr<Ship::Console>, std::vector<std::string> args, std::string* output) {
                             if (args.size() < 2) {
                                 SaveManager::RequestImport("");
                                 return 0;
                             }
                             std::string message;
                             bool ok = SaveManager::ImportGameCubeSave(args[1], &message);
                             SPDLOG_INFO("save_import: {}", message);
                             if (output != nullptr) {
                                 *output = message;
                             }
                             return ok ? 0 : 1;
                         },
                          "Imports a GameCube town from a .gci, .gcs or memory card image",
                          { { "path", Ship::ArgumentType::TEXT, true } } });
    console->AddCommand("save_pick_town", { [](std::shared_ptr<Ship::Console>, std::vector<std::string>, std::string*) {
                                               SaveManager_PickTownFile();
                                               return 0;
                                           },
                                            "Picks the second town file (slot B)",
                                            {} });
}

bool sQuitTestPushed = false;

bool OnFileDropped(char* path) {
    if (GbaPort::ScanWaiting()) {
        return false;
    }
    if (!IsGameCubeSaveFile(path)) {
        return false;
    }
    SaveManager::RequestImport(path);
    return true;
}

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
#if defined(__IOS__) || defined(__ANDROID__)
    std::error_code ec;
    std::filesystem::create_directories(ImportDirectory(), ec);
#endif
    InstallQuitFilter();
    RegisterCommands();
    if (auto drop = ShipCompat::GetFileDropMgr()) {
        drop->RegisterDropHandler(OnFileDropped);
    }
    REGISTER_LISTENER(GameLoopTick, EVENT_PRIORITY_LOW, OnFrame);
}

RegisterShipInitFunc sInitSaveManagerUi(InitSaveManagerUi);

} // namespace
