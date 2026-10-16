#pragma once

#include <string>

namespace GbaPort {

bool ScanWaiting();
std::string ScanDescription();
std::string ECardDirectory();
bool SubmitCard(const std::string& path, std::string* error);
void CancelScan();
void Tick();

} // namespace GbaPort
