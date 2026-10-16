#pragma once

#include <filesystem>
#include <vector>
#include <cstdint>
#include <optional>
#include <atomic>
#include <string>
#include <utility>

namespace fs = std::filesystem;

class GameExtractor {
public:
    static std::string sLastError;

    static bool IsDiscInputAvailable();
    static std::string GetCurrentAssetName();
    bool RunStandalone(std::string disc);
    bool SelectGameFromUI(bool pickFolder);
    static std::optional<std::string> DetectVersion(const std::string& discPath);
    // name) pairs deduplicated by version so the user is offered one entry per version.
    std::vector<std::pair<std::string, std::string>> FindSupportedDiscs(const std::vector<std::string>& searchPaths);
    std::string GetDiscPath();
    bool Parse(std::atomic<size_t>& assetCount, std::string appShortName = "");
    bool GenerateOTR(std::string appShortName = "");
    bool GenerateOTR(std::atomic<size_t>& assetCount, std::string appShortName = "");
    // Same as GenerateOTR, but with the two directories named outright instead of
    bool GenerateOTRTo(std::atomic<size_t>& assetCount, const std::string& assetsPath, const std::string& gamePath);
    void WritePortVersion();
private:
    bool RunTorch(std::atomic<size_t>& assetCount, const std::string& assetsPath, const std::string& gamePath,
                  bool process);
    fs::path mGamePath;
};
