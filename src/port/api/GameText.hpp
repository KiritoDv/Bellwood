#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

// Encodes UTF-8 text into the game's character set. With commands, "<<NAME [hex bytes]>>" tags become control codes.
std::string GameText_Encode(const std::string& text, bool commands);
// Size in bytes of a control code, including its 0x7F prefix and id.
size_t GameText_CodeSize(uint8_t code);
// Control code id for a tag name, or -1.
int GameText_CommandIndex(const char* name);
