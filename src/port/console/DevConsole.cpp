#include "DevConsole.h"
#include "port/ShipCompat.h"

#include <ship/Context.h>
#include <ship/window/Window.h>
#include <ship/window/gui/Gui.h>
#include <ship/window/gui/ConsoleWindow.h>
#include <ship/debug/Console.h>

#define CMD_REGISTER ShipCompat::GetConsole()->AddCommand
// TODO: Commands should be using the output passed in.
#define ERROR_MESSAGE                                                                                              \
    std::reinterpret_pointer_cast<Ship::ConsoleWindow>(ShipCompat::GetWindow()->GetGui()->GetGuiWindow("Console")) \
        ->SendErrorMessage
#define INFO_MESSAGE                                                                                               \
    std::reinterpret_pointer_cast<Ship::ConsoleWindow>(ShipCompat::GetWindow()->GetGui()->GetGuiWindow("Console")) \
        ->SendInfoMessage

void DevConsole_Init(void) {
}
