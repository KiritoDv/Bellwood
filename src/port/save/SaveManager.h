#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void SaveManager_PickTownFile(void);
int SaveManager_IsPickingTownFile(void);
int SaveManager_IsSaving(void);
void SaveManager_OpenImport(void);

const char* SaveManager_GetTownFile(void);
void SaveManager_SetTownFile(const char* path);

#ifdef __cplusplus
}

#include <string>
#include <vector>
#include <cstdint>

namespace SaveManager {

struct PickerEntry {
    std::string path;
    std::string label;
};

struct ImportPreview {
    bool ok = false;
    std::string error;
    std::string town;
    std::vector<std::string> players;
    bool land = false;
    bool letters = false;
    bool designs = false;
    bool diary = false;
    int passports = 0;
    bool famicom = false;
    int nesGames = 0;
    int gifts = 0;
};

enum class QuitPrompt {
    None,
    SaveAndQuit,
    Visiting,
    Saving,
    Failed,
};

void CloseTownPicker(bool picked);
std::vector<PickerEntry> ListTownFiles();
std::string TownDirectory();

void Tick();
bool QuitTestArmed();

bool OnQuitRequest();
QuitPrompt GetQuitPrompt();
void NotePromptDrawn();
void DismissQuitPrompt();
void RequestSaveAndQuit();
void QuitWithoutSaving();
bool UnsafeQuitAllowed();

bool CanImportGameCubeSave(std::string* reason);
ImportPreview PreviewGameCubeSave(const std::string& path);
bool ImportGameCubeSave(const std::string& path, std::string* message);
bool TakeImportRequest(std::string* path);
void RequestImport(const std::string& path);

bool RoundTripSelfTest(std::string* report);
bool IoSelfTest(std::string* report);
bool InPlay();

// The home town file as last saved, packed to send to another player. Empty when there is none.
std::vector<uint8_t> ExportTown();
// Makes a town packed by ExportTown the second town, so the train station takes the player there as a visitor. The
// town chosen in the settings is left alone and comes back with EndVisit. Fails for a broken file or this town.
bool VisitTown(const std::vector<uint8_t>& packed);
// Puts back the second town chosen in the settings. Fails while the player is in the visited town.
bool EndVisit();
// The player is a visitor in another town.
bool IsVisitor();

} // namespace SaveManager
#endif
