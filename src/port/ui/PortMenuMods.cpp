#ifdef ENABLE_SCRIPTING

#include "PortMenu.h"
#include "port/Engine.h"
#include "port/ModList.h"
#include "port/ShipCompat.h"

#include <ship/resource/archive/Archive.h>
#include <ship/resource/archive/ArchiveManager.h>
#include <ship/scripting/ScriptLoader.h>
#include <ship/security/Keystore.h>
#include <ship/utils/StringHelper.h>
#include <ship/window/gui/IconsFontAwesome4.h>
#include <SDL2/SDL.h>

#include <algorithm>

namespace PortGui {

using namespace UIWidgets;

void PortMenu::AddModMenu() {
    if (mModMenuAdded) {
        return;
    }
    mModMenuAdded = true;

    AddMenuEntry("Mods", CVAR_SETTING("Menu.ModsSidebarSection"));
    WidgetPath path = { "Mods", "Installed", SECTION_COLUMN_1 };
    AddSidebarEntry(path.sectionName, path.sidebarName, 1);

    AddWidget(path, "Reload Scripts", WIDGET_BUTTON)
        .Options(ButtonOptions().Tooltip("Rebuilds and reloads every code mod from disk.").Color(Colors::Orange))
        .Callback([](WidgetInfo& info) {
            ShipCompat::GetScriptLoader()->UnloadAll();
            GameEngine::Instance->LoadScripts();
        });

    const auto keystore = ShipCompat::GetKeystore();
    const auto keys = keystore != nullptr ? keystore->GetAllKeys() : std::vector<Ship::KeystoreEntry>{};
    bool any = false;

    for (const ModList::Entry& entry : ModList::Entries()) {
        const auto& archive = entry.archive;
        const auto& info = archive->GetManifest();
        any = true;

        std::string title = info.Name;
        if (!info.Main.empty() || !info.Binaries.empty()) {
            title += " (Code Mod)";
        }
        AddWidget(path, title, WIDGET_SEPARATOR_TEXT).Options(TextOptions{});

        std::string metadata = "Author: " + (info.Author.empty() ? std::string("Unknown") : info.Author);
        if (!info.Version.empty()) {
            metadata += "  |  Version: " + info.Version;
        }
        if (!info.License.empty()) {
            metadata += "  |  License: " + info.License;
        }
        AddWidget(path, metadata, WIDGET_TEXT).Options(TextOptions{});

        const std::string name = info.Name;
        AddWidget(path, "##Settings_" + name, WIDGET_CUSTOM)
            .CustomFunction([name](WidgetInfo&) {
                const auto& entries = ModList::Entries();
                const auto it = std::find_if(entries.begin(), entries.end(), [&](const ModList::Entry& e) {
                    return e.archive->GetManifest().Name == name;
                });
                if (it != entries.end() && !it->status.empty()) {
                    ImGui::TextColored(it->failed ? ImVec4(1.0f, 0.4f, 0.4f, 1.0f) : ImVec4(0.5f, 0.9f, 0.5f, 1.0f),
                                       "%s", it->status.c_str());
                }
                bool enabled = ModList::IsEnabled(name);
                if (ImGui::Checkbox(("Enabled##" + name).c_str(), &enabled)) {
                    ModList::SetEnabled(name, enabled);
                }
                ImGui::SameLine();
                int priority = ModList::Priority(name);
                ImGui::SetNextItemWidth(120.0f);
                if (ImGui::InputInt(("Priority##" + name).c_str(), &priority)) {
                    ModList::SetPriority(name, priority);
                }
                if (it != entries.end() && (enabled != it->enabled || priority != it->priority)) {
                    ImGui::TextDisabled("Takes effect when Bellwood restarts.");
                }
            })
            .Options(WidgetOptions().Tooltip("Mods with a higher priority load later: their files replace those of "
                                             "mods loaded before them, and their code initializes after."));

        if (archive->IsSigned()) {
            Ship::KeyOrigin origin = Ship::KeyOrigin::User;
            const auto publicKey = StringHelper::HexToBytes(info.PublicKey);
            for (const auto& key : keys) {
                if (key.Data == publicKey) {
                    origin = key.Origin;
                    break;
                }
            }
            const char* originText = "[User Approved]";
            Colors originColor = Colors::Yellow;
            if (origin == Ship::KeyOrigin::Game) {
                originText = "[Game]";
                originColor = Colors::Purple;
            } else if (origin == Ship::KeyOrigin::System) {
                originText = "[System]";
                originColor = Colors::Red;
            }
            AddWidget(path, std::string(ICON_FA_CHECK_CIRCLE) + " Signed", WIDGET_TEXT)
                .Options(TextOptions().Color(Colors::Green));
            AddWidget(path, originText, WIDGET_TEXT).SameLine(true).Options(TextOptions().Color(originColor));
        } else if (archive->IsChecksumValid()) {
            AddWidget(path, std::string(ICON_FA_EXCLAMATION_TRIANGLE) + " Unsigned", WIDGET_TEXT)
                .Options(TextOptions().Color(Colors::Orange));
        } else {
            AddWidget(path, std::string(ICON_FA_EXCLAMATION_TRIANGLE) + " Untrusted", WIDGET_TEXT)
                .Options(TextOptions().Color(Colors::Red));
        }

        if (!info.Dependencies.empty()) {
            std::string dependencies = "Dependencies: ";
            for (size_t i = 0; i < info.Dependencies.size(); i++) {
                dependencies += (i > 0 ? ", " : "") + info.Dependencies[i];
            }
            AddWidget(path, dependencies, WIDGET_TEXT).Options(TextOptions{});
        }
        if (!info.Description.empty()) {
            AddWidget(path, info.Description, WIDGET_TEXT).Options(TextOptions{});
        }
        if (!info.Website.empty()) {
            const std::string website = info.Website;
            AddWidget(path, "Open Webpage##" + info.Name, WIDGET_BUTTON)
                .Options(ButtonOptions{})
                .Callback([website](WidgetInfo&) { SDL_OpenURL(website.c_str()); });
        }
        AddWidget(path, "##Spacer_" + info.Name, WIDGET_SEPARATOR).Options(WidgetOptions{});
    }

    if (!any) {
        AddWidget(path, "No mods with a manifest.json are installed.", WIDGET_TEXT).Options(TextOptions{});
    }
}

} // namespace PortGui

#endif
