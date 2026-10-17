#include "port/gba/GbaPort.h"
#include "port/gba/DesignFiles.h"

#include "port/ShipCompat.h"
#include "port/ShipInit.hpp"
#include "port/events/Events.h"
#include "port/portable-file-dialogs.h"
#include "port/ui/cvar_prefixes.h"

#include <imgui.h>
#include <ship/debug/Console.h>
#include <ship/window/FileDrop.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/GuiWindow.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

const char* const kScanTitle = "e-Reader##GbaScan";

std::string sDroppedCard;

class GbaWindow final : public Ship::GuiWindow {
  public:
    using GuiWindow::GuiWindow;

    void Draw() override {
        if (!IsVisible()) {
            return;
        }
        DrawElement();
    }

    void DrawElement() override {
        DrawScan();
    }

    void UpdateElement() override {
    }

  private:
    void Refresh() {
        mFiles.clear();
        std::error_code ec;
        fs::path dir = GbaPort::ECardDirectory();
        fs::create_directories(dir, ec);
        for (const auto& e : fs::directory_iterator(dir, ec)) {
            if (e.is_regular_file(ec)) {
                mFiles.push_back(e.path().string());
            }
        }
        std::sort(mFiles.begin(), mFiles.end());
    }

    void Submit(const std::string& path) {
        std::string error;
        if (GbaPort::SubmitCard(path, &error)) {
            Close();
        } else {
            mError = error;
        }
    }

    void Close() {
        mDialog.reset();
        mOpen = false;
        mError.clear();
        ImGui::CloseCurrentPopup();
    }

    void DrawScan() {
        if (!GbaPort::ScanWaiting()) {
            mOpen = false;
            mDialog.reset();
            return;
        }
        if (!mOpen) {
            mOpen = true;
            mError.clear();
            mSelected.clear();
            Refresh();
        }
        if (!ImGui::IsPopupOpen(kScanTitle)) {
            ImGui::OpenPopup(kScanTitle);
        }
        ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Appearing);
        if (!ImGui::BeginPopupModal(kScanTitle, nullptr, ImGuiWindowFlags_NoSavedSettings)) {
            return;
        }
        if (!sDroppedCard.empty()) {
            std::string path = sDroppedCard;
            sDroppedCard.clear();
            Submit(path);
            if (!mOpen) {
                ImGui::EndPopup();
                return;
            }
        }
        if (mDialog && mDialog->ready(0)) {
            auto files = mDialog->result();
            mDialog.reset();
            if (!files.empty()) {
                Submit(files[0]);
                if (!mOpen) {
                    ImGui::EndPopup();
                    return;
                }
            }
        }

        ImGui::TextWrapped("Scan %s: pick a card file (.raw or .bin dot code, or a decoded card), or drop it onto "
                           "the window.",
                           GbaPort::ScanDescription().c_str());
        ImGui::Spacing();
        ImGui::Text("Cards in %s:", GbaPort::ECardDirectory().c_str());
        if (ImGui::BeginListBox("##cards", ImVec2(-FLT_MIN, 7 * ImGui::GetTextLineHeightWithSpacing()))) {
            if (mFiles.empty()) {
                ImGui::TextDisabled("No card files yet.");
            }
            for (const auto& f : mFiles) {
                std::string label = fs::path(f).filename().string();
                if (ImGui::Selectable(label.c_str(), f == mSelected, ImGuiSelectableFlags_AllowDoubleClick)) {
                    mSelected = f;
                    if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        Submit(f);
                    }
                }
            }
            ImGui::EndListBox();
        }
        if (!mError.empty()) {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", mError.c_str());
        }
        ImGui::Separator();
        ImGui::BeginDisabled(mSelected.empty());
        if (ImGui::Button("Scan")) {
            Submit(mSelected);
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
#if !defined(__IOS__) && !defined(__ANDROID__) && !defined(__EMSCRIPTEN__)
        if (ImGui::Button("Browse...") && !mDialog) {
            mDialog = std::make_unique<pfd::open_file>(
                "Pick an e-Reader card", GbaPort::ECardDirectory(),
                std::vector<std::string>{ "e-Reader cards", "*.raw *.bin *.dec", "All files", "*" });
        }
        ImGui::SameLine();
#endif
        if (ImGui::Button("Refresh")) {
            Refresh();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            GbaPort::CancelScan();
            Close();
        }
        ImGui::EndPopup();
    }

    bool mOpen = false;
    std::string mSelected;
    std::string mError;
    std::vector<std::string> mFiles;
    std::unique_ptr<pfd::open_file> mDialog;
};

bool OnFileDropped(char* path) {
    if (!GbaPort::ScanWaiting()) {
        return false;
    }
    sDroppedCard = path;
    return true;
}

void OnFrame(IEvent*) {
    GbaPort::Tick();
}

void RegisterCommands() {
    auto console = ShipCompat::GetConsole();
    if (!console) {
        return;
    }
    console->AddCommand("ecard_scan",
                        { [](std::shared_ptr<Ship::Console>, std::vector<std::string> args, std::string* output) {
                             std::string error;
                             bool ok = args.size() > 1 && GbaPort::SubmitCard(args[1], &error);
                             if (output != nullptr) {
                                 *output = ok ? "Card read." : (args.size() > 1 ? error : "Give the card file path.");
                             }
                             return ok ? 0 : 1;
                         },
                          "Scans an e-Reader card file while the game is waiting for one",
                          { { "path", Ship::ArgumentType::TEXT } } });
}

void InitGbaUi() {
    auto window = ShipCompat::GetWindow();
    if (!window || !window->GetGui()) {
        return;
    }
    auto gbaWindow = std::make_shared<GbaWindow>(CVAR_WINDOW("Gba"), "Game Boy Advance");
    window->GetGui()->AddGuiWindow(gbaWindow);
    gbaWindow->Show();
    if (auto drop = ShipCompat::GetFileDropMgr()) {
        drop->RegisterDropHandler(OnFileDropped);
    }
    RegisterCommands();
    REGISTER_LISTENER(KeyboardInput, EVENT_PRIORITY_LOW, OnFrame);
}

RegisterShipInitFunc sInitGbaUi(InitGbaUi);

} // namespace
