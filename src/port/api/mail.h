#pragma once

#include <stdint.h>

#include "api.h"
#include "m_name_table.h"

// Text is UTF-8 with the same control code tags as messages (see port/api/message.h). Longer text is cut off.
typedef struct C_LetterDef_s {
    const char* header;    // up to 24 characters. The recipient's name goes where it has a "\n", or after it
    const char* body;      // up to 192 characters
    const char* footer;    // up to 32 characters, e.g. "From, the mayor"
    const char* sender;    // name shown as the sender, up to 8 characters
    mActor_name_t present; // item attached to the letter, EMPTY_NO for none
    mActor_name_t paper;   // ITM_PAPER* stationery, EMPTY_NO for the first design
} C_LetterDef;

// Puts a letter in the mailbox of player, 0 to 3 in the town, or -1 for the player being played. When the mailbox is
// full the post office keeps it, like the game's own letters. Returns FALSE when it can't be delivered.
#define C_SendLetter(def, player) C_SendLetterSized((def), (player), sizeof(*(def)))
BW_API int C_SendLetterSized(const C_LetterDef* def, int player, uint32_t size);
