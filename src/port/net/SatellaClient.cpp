#ifdef USE_NETWORKING
#include "port/net/SatellaClient.h"
#include "port/net/PlayerIdentity.h"
#include "port/ShipCompat.h"
#include "port/ShipInit.hpp"
#include "port/events/Events.h"
#include "port/ui/Notification.h"

#include <ship/config/Config.h>
#include <ship/security/Keystore.h>
#include <ship/utils/StringHelper.h>
#include <spdlog/spdlog.h>

#include <nlohmann/json.hpp>

#include <ixwebsocket/IXNetSystem.h>
#include <ixwebsocket/IXWebSocket.h>

#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>

#if !defined(_WIN32)
#include <unistd.h>
#endif

static ix::WebSocket sWs;

namespace Satella {

static void WriteU32LE(std::vector<uint8_t>& buf, uint32_t value) {
    buf.push_back(static_cast<uint8_t>(value & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    buf.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

static int16_t ReadI16LE(const uint8_t* buf) {
    return static_cast<int16_t>(static_cast<uint16_t>(buf[0]) | (static_cast<uint16_t>(buf[1]) << 8));
}

// "HM64", the frame type, then the route with its length.
static std::vector<uint8_t> BuildFrame(uint8_t type, const std::string& route) {
    std::vector<uint8_t> frame = { 'H', 'M', '6', '4', type };
    WriteU32LE(frame, static_cast<uint32_t>(route.size()));
    frame.insert(frame.end(), route.begin(), route.end());
    return frame;
}

Client& Client::Instance() {
    static Client instance;
    return instance;
}

Client::~Client() {
    {
        std::lock_guard<std::mutex> lock(mQueueMtx);
        mStopping = true;
    }
    mQueueCv.notify_all();
    if (mWorker.joinable()) {
        mWorker.join();
    }
    if (sWs.getReadyState() != ix::ReadyState::Closed) {
        sWs.stop();
    }
    ix::uninitNetSystem();
}

void Client::Register(std::unique_ptr<IPacket> packet) {
    std::lock_guard<std::mutex> lock(mPacketsMtx);
    mPackets.push_back(std::move(packet));
}

bool Client::IsConnected() const {
    return sWs.getReadyState() == ix::ReadyState::Open;
}

void Client::Keep(std::unique_ptr<IPacket> packet) {
    std::lock_guard<std::mutex> lock(mPacketsMtx);
    if (packet->IsSubscription()) {
        mSubscriptions.push_back(packet.get());
        mPackets.push_back(std::move(packet));
    }
}

void Client::RegisterLive(std::unique_ptr<IPacket> packet) {
    if (!IsConnected()) {
        Register(std::move(packet));
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mQueueMtx);
        mQueue.push_back(std::move(packet));
        if (!mWorker.joinable()) {
            mWorker = std::thread([this] { WorkerLoop(); });
        }
    }
    mQueueCv.notify_one();
}

void Client::WorkerLoop() {
    while (true) {
        std::unique_ptr<IPacket> packet;
        {
            std::unique_lock<std::mutex> lock(mQueueMtx);
            mQueueCv.wait(lock, [this] { return mStopping || !mQueue.empty(); });
            if (mStopping) {
                return;
            }
            packet = std::move(mQueue.front());
            mQueue.pop_front();
        }
        SendAndReceive(*packet);
        Keep(std::move(packet));
    }
}

void Client::SendRaw(const std::string& route, const void* data, size_t size) {
    if (!IsConnected()) {
        return;
    }
    std::vector<uint8_t> frame = BuildFrame(0x03, route);
    const auto* bytes = static_cast<const uint8_t*>(data);
    frame.insert(frame.end(), bytes, bytes + size);
    sWs.sendBinary(std::string(frame.begin(), frame.end()));
}

void Client::RunOnGameThread(std::function<void()> work) {
    std::lock_guard<std::mutex> lock(mGameMtx);
    mGameWork.push_back(std::move(work));
}

void Client::Poll() {
    std::vector<std::function<void()>> work;
    {
        std::lock_guard<std::mutex> lock(mGameMtx);
        work.swap(mGameWork);
    }
    for (auto& fn : work) {
        fn();
    }
}

void Client::OnMessage(const ix::WebSocketMessagePtr& msg) {
    switch (msg->type) {
        case ix::WebSocketMessageType::Open: {
            std::lock_guard<std::mutex> lock(mMtx);
            mConnected = true;
            mCv.notify_all();
            break;
        }
        case ix::WebSocketMessageType::Message: {
            const auto* data = reinterpret_cast<const uint8_t*>(msg->str.data());
            const size_t size = msg->str.size();
            constexpr size_t kHeaderSize = 5;

            if (size < kHeaderSize || data[0] != 'H' || data[1] != 'M') {
                SPDLOG_WARN("Satella: malformed message ({} bytes)", size);
                break;
            }

            const uint8_t type = data[2];
            const int16_t status = ReadI16LE(data + 3);
            const std::string body = (type != 0x01 && size > kHeaderSize)
                                         ? std::string(msg->str.begin() + kHeaderSize, msg->str.end())
                                         : std::string();

            bool isResponse;
            {
                std::lock_guard<std::mutex> lock(mMtx);
                isResponse = mWaitingForResponse;
                if (isResponse) {
                    mResponseStatus = status;
                    mResponseBody = body;
                    mResponseValid = true;
                    mResponseReady = true;
                    mCv.notify_all();
                }
            }

            if (!isResponse) {
                std::vector<IPacket*> subscriptions;
                {
                    std::lock_guard<std::mutex> lock(mPacketsMtx);
                    subscriptions = mSubscriptions;
                }
                for (auto* sub : subscriptions) {
                    sub->OnPush(status, type, body);
                }
            }
            break;
        }
        case ix::WebSocketMessageType::Error:
            SPDLOG_WARN("Satella: WebSocket error: {}", msg->errorInfo.reason);
            [[fallthrough]];
        case ix::WebSocketMessageType::Close: {
            std::lock_guard<std::mutex> lock(mMtx);
            mConnected = false;
            mResponseReady = true;
            mResponseValid = false;
            mCv.notify_all();
            break;
        }
        default:
            break;
    }
}

static ix::SocketTLSOptions BuildTLSOptions() {
    ix::SocketTLSOptions opts;
#if defined(__ANDROID__)
    // Android keeps its trusted CAs as one PEM file each, in the updatable Conscrypt module since Android 14. Passing
    // them in memory lets mbedtls trust the same certificates the system does.
    static const std::string bundle = [] {
        std::string pem;
        for (const char* dir : { "/apex/com.android.conscrypt/cacerts", "/system/etc/security/cacerts" }) {
            std::error_code ec;
            for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
                std::ifstream in(entry.path(), std::ios::binary);
                pem.append(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
                pem += '\n';
            }
            if (!pem.empty()) {
                break;
            }
        }
        return pem;
    }();
    if (!bundle.empty()) {
        opts.caFile = bundle;
    } else {
        SPDLOG_WARN("Satella: no system CA certificates found");
    }
#elif defined(__linux__) || defined(__APPLE__)
    // ixwebsocket's mbedtls backend only finds the system certificates on Windows.
#if defined(__APPLE__)
    static constexpr const char* kCAPaths[] = {
        "/etc/ssl/cert.pem",
    };
#else
    static constexpr const char* kCAPaths[] = {
        "/etc/ssl/certs/ca-certificates.crt",
        "/etc/pki/tls/certs/ca-bundle.crt",
        "/etc/ssl/ca-bundle.pem",
        "/etc/pki/ca-trust/extracted/pem/tls-ca-bundle.pem",
    };
#endif
    for (const char* path : kCAPaths) {
        if (access(path, R_OK) == 0) {
            opts.caFile = path;
            break;
        }
    }
#endif
    return opts;
}

void Client::Connect(const std::string& url) {
    if (mUrl == url && IsConnected()) {
        return;
    }
    if (sWs.getReadyState() != ix::ReadyState::Closed) {
        sWs.stop();
    }

    mUrl = url;
    sWs.setUrl(url + "/ws?v=" BELLWOOD_VERSION);
#if defined(__linux__) || defined(__APPLE__)
    sWs.setTLSOptions(BuildTLSOptions());
#endif
    sWs.setOnMessageCallback([this](const ix::WebSocketMessagePtr& msg) { OnMessage(msg); });

    std::unique_lock<std::mutex> lock(mMtx);
    mConnected = false;
    sWs.start();
    mCv.wait_for(lock, std::chrono::seconds(10), [this] { return mConnected; });

    if (!mConnected) {
        SPDLOG_WARN("Satella: could not connect to {}", url);
    } else {
        SPDLOG_INFO("Satella: connected to {}", url);
    }
}

void Client::SendAndReceive(IPacket& packet) {
    if (!IsConnected()) {
        SPDLOG_WARN("Satella: not connected, skipping '{}'", packet.GetRoute());
        return;
    }

    const std::vector<uint8_t> request = BuildFrame(0x02, packet.GetRoute());
    {
        std::lock_guard<std::mutex> lock(mMtx);
        mResponseReady = false;
        mResponseValid = false;
        mWaitingForResponse = true;
    }

    sWs.sendBinary(std::string(request.begin(), request.end()));

    int16_t status;
    std::string body;
    {
        std::unique_lock<std::mutex> lock(mMtx);
        mCv.wait_for(lock, std::chrono::seconds(10), [this] { return mResponseReady; });
        mWaitingForResponse = false;
        if (!mResponseValid) {
            SPDLOG_WARN("Satella: no response for '{}'", packet.GetRoute());
            return;
        }
        status = mResponseStatus;
        body = std::move(mResponseBody);
    }

    packet.OnResponse(status, body);
}

void Client::Execute() {
    std::vector<IPacket*> packets;
    {
        std::lock_guard<std::mutex> lock(mPacketsMtx);
        for (auto& packet : mPackets) {
            packets.push_back(packet.get());
        }
    }
    if (packets.empty()) {
        return;
    }

    ix::initNetSystem();
    mPhase = Phase::Connecting;
    auto config = ShipCompat::GetConfig();
    Connect(config != nullptr ? config->GetString("Satella.Host", SATELLA_HOST) : SATELLA_HOST);

    if (!mConnected) {
        mPhase = Phase::Done;
        return;
    }

    mPhase = Phase::FetchingKeys;
    for (auto* packet : packets) {
        if (packet->IsBlocking()) {
            SendAndReceive(*packet);
        }
    }

    mPhase = Phase::Done;
    for (auto* packet : packets) {
        if (!packet->IsBlocking()) {
            SendAndReceive(*packet);
        }
    }

    std::lock_guard<std::mutex> lock(mPacketsMtx);
    for (auto* packet : packets) {
        if (packet->IsSubscription()) {
            mSubscriptions.push_back(packet);
        }
    }
}

// Registers this install's player key, so the stores can check its signatures.
class RegisterPlayerPacket : public IPacket {
    mutable std::string mRoute;

  public:
    const char* GetRoute() const override {
        if (mRoute.empty()) {
            const auto pub = PlayerIdentity::GetPublicKey();
            mRoute = "/v1/player/" + StringHelper::BytesToHex({ pub.begin(), pub.end() }) + "/register";
        }
        return mRoute.c_str();
    }

    bool IsBlocking() const override {
        return true;
    }

    void OnResponse(int16_t status, const std::string&) override {
        if (status != 200) {
            SPDLOG_WARN("Satella: player registration returned status {}", status);
        } else {
            SPDLOG_INFO("Satella: player registered");
        }
    }
};

SATELLA_REGISTER_PACKET(RegisterPlayerPacket);

// Adds the keys Satella vouches for to the keystore, before the game mounts mods, so mods signed with them load
// without asking.
class PublicKeysPacket : public IPacket {
  public:
    const char* GetRoute() const override {
        return "/v1/security/pub-keys";
    }

    bool IsBlocking() const override {
        return true;
    }

    void OnResponse(int16_t status, const std::string& body) override {
        if (status != 200) {
            SPDLOG_WARN("Satella: pub-keys returned status {}", status);
            return;
        }

        const auto keys = nlohmann::json::parse(body, nullptr, false);
        if (!keys.is_object()) {
            SPDLOG_WARN("Satella: pub-keys response is not a JSON object");
            return;
        }

        auto keystore = ShipCompat::GetKeystore();
        if (keystore == nullptr) {
            SPDLOG_WARN("Satella: keystore not initialised yet");
            return;
        }

        int added = 0;
        for (const auto& [name, value] : keys.items()) {
            if (!value.is_string()) {
                continue;
            }
            const std::vector<uint8_t> keyData = StringHelper::HexToBytes(value.get<std::string>());
            if (keyData.empty() || keystore->HasKey(keyData)) {
                continue;
            }
            keystore->AddKey(name, keyData, Ship::KeyOrigin::Game);
            ++added;
        }
        SPDLOG_INFO("Satella: {} new public key(s)", added);
    }
};

SATELLA_REGISTER_PACKET(PublicKeysPacket);

// Shows the notifications Satella pushes.
class NotificationsPacket : public IPacket {
  public:
    const char* GetRoute() const override {
        return "/v1/notifications/subscribe";
    }

    bool IsSubscription() const override {
        return true;
    }

    void OnResponse(int16_t status, const std::string&) override {
        if (status != 200) {
            SPDLOG_WARN("Satella: notifications subscribe returned status {}", status);
        }
    }

    void OnPush(int16_t status, uint8_t packetType, const std::string& body) override {
        if (status != 200 || packetType != 0x02 || body.empty()) {
            return;
        }
        const auto data = nlohmann::json::parse(body, nullptr, false);
        if (!data.is_object()) {
            return;
        }

        Notification::Options opts;
        opts.prefix = data.value("prefix", std::string());
        opts.message = data.value("message", std::string());
        opts.suffix = data.value("suffix", std::string());
        if (data.contains("duration") && data["duration"].is_number()) {
            opts.remainingTime = data["duration"].get<float>();
        }
        Client::Instance().RunOnGameThread([opts] { Notification::Emit(opts); });
    }
};

SATELLA_REGISTER_PACKET(NotificationsPacket);

} // namespace Satella

static void SatellaPoll(IEvent*) {
    Satella::Client::Instance().Poll();
}

static RegisterShipInitFunc sSatellaInit([] { REGISTER_LISTENER(GameLoopTick, EVENT_PRIORITY_NORMAL, SatellaPoll); });

#endif
