#pragma once

#include <stdint.h>

#include "api.h"

#define C_MESSAGE_FIRST 0x4000
#define C_MESSAGE_LAST 0x7FFF

// Message text is UTF-8. A new line is "\n" and control codes use the same "<<NAME [hex bytes]>>" tags as
// tools/msg_tool.py, e.g. "<<STR_PLAYERNAME>>", "<<BTN>>" or "<<SETFORCEMSG [40 01]>>". An end code is added
// when the text doesn't finish with one.

// Registers a new message and returns its number, or -1.
BW_API int32_t C_RegisterMessage(const char* text);
// Frees a message registered with C_RegisterMessage.
BW_API void C_UnregisterMessage(int32_t msg_no);
// Replaces the text of any message, built-in or registered.
BW_API void C_SetMessage(int32_t msg_no, const char* text);
// Restores the original text of a built-in message changed with C_SetMessage.
BW_API void C_ResetMessage(int32_t msg_no);
// Encodes text into the game's character set, without adding an end code. Returns the number of bytes written.
BW_API int C_EncodeText(const char* text, uint8_t* out, int capacity);
