#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace DesignFiles {

constexpr int kWidth = 32;
constexpr int kHeight = 32;

std::string Directory();
std::vector<uint8_t> EncodePng(const uint8_t* rgba, int width, int height);
bool DecodePng(const std::vector<uint8_t>& png, std::vector<uint8_t>& rgba, int* width, int* height);
bool Export(const uint8_t* designs, int count, std::string* error);
bool HasImport();
bool Import(uint8_t* designs, int count, std::string* error);

} // namespace DesignFiles
