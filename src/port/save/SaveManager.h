#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void SaveManager_PickTownFile(void);
int SaveManager_IsPickingTownFile(void);
void SaveManager_OpenImport(void);

const char* SaveManager_GetTownFile(void);
void SaveManager_SetTownFile(const char* path);

#ifdef __cplusplus
}

#include <string>
#include <vector>

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

} // namespace SaveManager
#endif
