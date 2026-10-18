#pragma once

#include <functional>
#include <string>

namespace ModOwner {

// Name of the mod whose code is running: the mod being initialized or exited, or the mod whose event listener or
// menu callback is being called. Empty for the game.
std::string Current();

// Makes Current() return mod until it goes out of scope.
class Scope {
  public:
    explicit Scope(const std::string& mod);
    ~Scope();
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};

// Adds a function that unregisters what a mod still has registered with one of the APIs.
void AddReleaser(std::function<void(const std::string& mod)> releaser);
// Unregisters everything the mod still has registered. Runs after its ModExit, before its code is freed.
void Release(const std::string& mod);

} // namespace ModOwner
