#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ECard {

bool RawToBin(const std::vector<uint8_t>& raw, std::vector<uint8_t>& bin);
bool Vpk0Decompress(const uint8_t* data, size_t size, std::vector<uint8_t>& out);
bool ReadPayloads(const std::vector<uint8_t>& file, std::vector<std::vector<uint8_t>>& payloads, std::string* error);

} // namespace ECard
