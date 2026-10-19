#ifdef USE_NETWORKING
#include "port/net/Relay.h"
#include "port/net/SatellaClient.h"
#include "port/api/ModOwner.hpp"
#include "port/events/Events.h"
#include "port/permissions/Permissions.h"
#include "port/ShipInit.hpp"

#include <spdlog/spdlog.h>

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace {

std::string PermKey(const std::string& channelId) {
    return "relay." + channelId;
}

// channelId@gameVersion, with -modVersion when the mod gives one: "mymod@1.0.0-1.2.0".
std::string VersionedChannelId(const std::string& channelId, const std::string& modVersion) {
    std::string id = channelId + "@" BELLWOOD_VERSION;
    if (!modVersion.empty()) {
        id += '-';
        id += modVersion;
    }
    return id;
}

struct ChannelState {
    Relay::ChannelDef def;
    bool subscribed = false;
    bool released = false;
};

std::mutex sMtx;
std::unordered_map<std::string, ChannelState> sChannels;

void Deliver(const std::string& channelId, const std::string& body) {
    Relay::MessageCallback callback;
    std::string owner;
    {
        std::lock_guard<std::mutex> lock(sMtx);
        const auto it = sChannels.find(channelId);
        if (it == sChannels.end() || it->second.released) {
            return;
        }
        callback = it->second.def.onMessage;
        owner = it->second.def.owner;
    }
    if (callback) {
        ModOwner::Scope scope(owner);
        callback(channelId, body.data(), static_cast<uint32_t>(body.size()));
    }
}

class RelaySubscriptionPacket : public Satella::IPacket {
  public:
    RelaySubscriptionPacket(std::string channelId, const std::string& modVersion)
        : mChannelId(std::move(channelId)),
          mRoute("/v1/relay/" + VersionedChannelId(mChannelId, modVersion) + "/subscribe") {
    }

    const char* GetRoute() const override {
        return mRoute.c_str();
    }

    bool IsSubscription() const override {
        return true;
    }

    void OnResponse(int16_t status, const std::string&) override {
        if (status != 200) {
            SPDLOG_WARN("Relay: subscribing to '{}' returned status {}", mChannelId, status);
        } else {
            SPDLOG_INFO("Relay: subscribed to '{}'", mChannelId);
        }
    }

    void OnPush(int16_t status, uint8_t packetType, const std::string& body) override {
        if (status != 200 || packetType != 0x03) {
            return;
        }
        Satella::Client::Instance().RunOnGameThread([id = mChannelId, body] { Deliver(id, body); });
    }

  private:
    std::string mChannelId;
    std::string mRoute;
};

void ActivateChannel(const std::string& channelId) {
    std::string modVersion;
    {
        std::lock_guard<std::mutex> lock(sMtx);
        const auto it = sChannels.find(channelId);
        if (it == sChannels.end() || it->second.subscribed) {
            return;
        }
        it->second.subscribed = true;
        modVersion = it->second.def.modVersion;
    }
    Satella::Client::Instance().RegisterLive(std::make_unique<RelaySubscriptionPacket>(channelId, modVersion));
}

// Asks for the channels mods registered while loading, once the game is up.
void OnEngineReady(IEvent*) {
    std::vector<std::string> ids;
    {
        std::lock_guard<std::mutex> lock(sMtx);
        for (const auto& [id, state] : sChannels) {
            if (!state.released) {
                ids.push_back(id);
            }
        }
    }
    for (const auto& id : ids) {
        switch (Relay::GetPermission(id)) {
            case Relay::Permission::Allowed:
                ActivateChannel(id);
                break;
            case Relay::Permission::Pending:
                Relay::RequestPermission(id);
                break;
            case Relay::Permission::Denied:
                break;
        }
    }
}

void ReleaseMod(const std::string& mod) {
    std::lock_guard<std::mutex> lock(sMtx);
    for (auto& [id, state] : sChannels) {
        if (state.def.owner == mod) {
            state.def.onMessage = nullptr;
            state.released = true;
        }
    }
}

} // namespace

namespace Relay {

void RegisterChannel(const std::string& channelId, ChannelDef def) {
    Permissions::Register(PermKey(channelId),
                          { def.modName + ": online channel",
                            def.modName + " wants to use the Satella channel\n\n    " + channelId + "\n\n" +
                                def.description + "\n\nAllow this mod to send and receive data over the internet?" });
    {
        std::lock_guard<std::mutex> lock(sMtx);
        auto it = sChannels.find(channelId);
        if (it != sChannels.end() && !it->second.released) {
            return;
        }
        if (it != sChannels.end()) {
            it->second.def = std::move(def);
            it->second.released = false;
        } else {
            sChannels[channelId] = { std::move(def) };
        }
    }
    if (GetPermission(channelId) == Permission::Allowed) {
        ActivateChannel(channelId);
    }
}

bool Send(const std::string& channelId, const char* data, uint32_t size) {
    if (GetPermission(channelId) != Permission::Allowed || !Satella::Client::Instance().IsConnected()) {
        return false;
    }
    std::string modVersion;
    {
        std::lock_guard<std::mutex> lock(sMtx);
        const auto it = sChannels.find(channelId);
        if (it != sChannels.end()) {
            modVersion = it->second.def.modVersion;
        }
    }
    Satella::Client::Instance().SendRaw("/v1/relay/" + VersionedChannelId(channelId, modVersion) + "/send", data,
                                        size);
    return true;
}

Permission GetPermission(const std::string& channelId) {
    switch (Permissions::Get(PermKey(channelId))) {
        case Permissions::State::Allowed:
            return Permission::Allowed;
        case Permissions::State::Denied:
            return Permission::Denied;
        default:
            return Permission::Pending;
    }
}

void RequestPermission(const std::string& channelId) {
    Permissions::Request(PermKey(channelId), [channelId]() { ActivateChannel(channelId); });
}

} // namespace Relay

static RegisterShipInitFunc sRelayInit([] {
    ModOwner::AddReleaser(ReleaseMod);
    REGISTER_LISTENER(EngineReady, EVENT_PRIORITY_NORMAL, OnEngineReady);
});

#endif
