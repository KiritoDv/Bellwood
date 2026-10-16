#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void SaveManager_PickTownFile(void);
int SaveManager_IsPickingTownFile(void);

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

bool RoundTripSelfTest(std::string* report);
bool IoSelfTest(std::string* report);
bool InPlay();

} // namespace SaveManager
#endif
