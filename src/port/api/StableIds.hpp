#pragma once

#include <ship/Context.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

class StableIds {
  public:
    StableIds(const char* file, int first, int last) : mFile(file), mFirst(first), mLast(last) {
    }

    int Get(const std::string& id) {
        Load();
        if (auto it = mIds.find(id); it != mIds.end()) {
            return it->second;
        }
        std::unordered_set<int> taken;
        for (const auto& [key, number] : mIds) {
            taken.insert(number);
        }
        for (int number = mFirst; number <= mLast; number++) {
            if (!taken.contains(number)) {
                mIds[id] = number;
                Save();
                return number;
            }
        }
        SPDLOG_ERROR("{}: no numbers left for {}", mFile, id);
        return -1;
    }

  private:
    std::string Path() const {
        return Ship::Context::GetPathRelativeToAppDirectory(mFile);
    }

    void Load() {
        if (mLoaded) {
            return;
        }
        mLoaded = true;
        std::ifstream in(Path());
        if (!in) {
            return;
        }
        const auto json = nlohmann::json::parse(in, nullptr, false);
        if (!json.is_object()) {
            SPDLOG_WARN("{} is not valid, mod content will get new numbers", mFile);
            return;
        }
        for (const auto& [id, number] : json.items()) {
            if (number.is_number_integer() && number.get<int>() >= mFirst && number.get<int>() <= mLast) {
                mIds[id] = number.get<int>();
            }
        }
    }

    void Save() const {
        nlohmann::json json = nlohmann::json::object();
        for (const auto& [id, number] : mIds) {
            json[id] = number;
        }
        std::ofstream out(Path(), std::ios::trunc);
        out << json.dump(2);
    }

    std::string mFile;
    int mFirst;
    int mLast;
    bool mLoaded = false;
    std::unordered_map<std::string, int> mIds;
};
