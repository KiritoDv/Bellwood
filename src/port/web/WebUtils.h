#pragma once
#ifdef __EMSCRIPTEN__

#include <string>

void WebCache_Mount(const char* path);
void WebCache_Load();
void WebCache_Save();
void WebCache_SaveNoWait();

void WebFrame_Wait(double milliseconds);

enum class WebPickResult { Cancelled, Disc, Archive, Failed };

WebPickResult WebFilePicker_PickDisc(const std::string& discDir, const std::string& archivePath, std::string& error);

#endif
