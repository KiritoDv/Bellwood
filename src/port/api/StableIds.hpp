#pragma once

#include <ship/Context.h>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

#include <fstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// Gives each content id a number in [first, last] and keeps it in custom_<key>.json, so the number stays the same
// across sessions. Save files store the same table under the key.
class StableIds {
  public:
    StableIds(const char* key, int first, int last)
        : mKey(key), mFile(std::string("custom_") + key + ".json"), mFirst(first), mLast(last) {
        All().push_back(this);
    }

    // Number of a registered id. owner is the mod registering it.
    int Register(const std::string& id, const std::string& owner) {
        const int number = Get(id);
        if (number < 0) {
            return -1;
        }
        mActive.insert(number);
        if (!owner.empty() && mIds[id].owner != owner) {
            mIds[id].owner = owner;
            Save();
        }
        return number;
    }

    void Unregister(int number) {
        mActive.erase(number);
    }

    bool IsActive(int number) const {
        return mActive.contains(number);
    }

    // Number of the id, giving it a free one the first time, preferred when that one is free. -1 when the range is
    // full.
    int Get(const std::string& id, int preferred = -1, const std::string& owner = {}) {
        Load();
        if (auto it = mIds.find(id); it != mIds.end()) {
            return it->second.number;
        }
        int number = preferred >= mFirst && preferred <= mLast && !mById.contains(preferred) ? preferred : -1;
        for (int candidate = mFirst; number < 0 && candidate <= mLast; candidate++) {
            if (!mById.contains(candidate)) {
                number = candidate;
            }
        }
        if (number < 0) {
            SPDLOG_ERROR("{}: no numbers left for {}", mFile, id);
            return -1;
        }
        mIds[id] = { number, owner };
        mById[number] = id;
        Save();
        return number;
    }

    // Id given the number, or nullptr.
    const std::string* IdOf(int number) {
        Load();
        auto it = mById.find(number);
        return it != mById.end() ? &it->second : nullptr;
    }

    // Mod that last registered the id, empty when unknown.
    std::string Owner(const std::string& id) {
        Load();
        auto it = mIds.find(id);
        return it != mIds.end() ? it->second.owner : std::string();
    }

    const std::string& Key() const {
        return mKey;
    }

    static std::vector<StableIds*>& All() {
        static std::vector<StableIds*> all;
        return all;
    }

    static StableIds* ForKey(const std::string& key) {
        for (StableIds* ids : All()) {
            if (ids->mKey == key) {
                return ids;
            }
        }
        return nullptr;
    }

  private:
    struct Entry {
        int number = -1;
        std::string owner;
    };

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
        for (const auto& [id, value] : json.items()) {
            Entry entry;
            if (value.is_number_integer()) {
                entry.number = value.get<int>();
            } else if (value.is_object()) {
                entry.number = value.value("number", -1);
                entry.owner = value.value("mod", std::string());
            }
            if (entry.number >= mFirst && entry.number <= mLast && !mById.contains(entry.number)) {
                mById[entry.number] = id;
                mIds[id] = std::move(entry);
            }
        }
    }

    void Save() const {
        nlohmann::json json = nlohmann::json::object();
        for (const auto& [id, entry] : mIds) {
            json[id] = { { "number", entry.number }, { "mod", entry.owner } };
        }
        std::ofstream out(Path(), std::ios::trunc);
        out << json.dump(2);
    }

    std::string mKey;
    std::string mFile;
    int mFirst;
    int mLast;
    bool mLoaded = false;
    std::unordered_map<std::string, Entry> mIds;
    std::unordered_map<int, std::string> mById;
    std::unordered_set<int> mActive;
};
