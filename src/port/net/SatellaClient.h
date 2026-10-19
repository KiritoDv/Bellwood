#pragma once

#ifdef USE_NETWORKING
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <ixwebsocket/IXWebSocketMessage.h>

// Overridden by "Satella.Host" in the config.
#define SATELLA_HOST "wss://satella.net64.dev"

namespace Satella {

enum class Phase { Idle, Connecting, FetchingKeys, Done };

class IPacket {
  public:
    virtual ~IPacket() = default;
    virtual const char* GetRoute() const = 0;
    virtual void OnResponse(int16_t status, const std::string& body) = 0;
    // Sent before the game mounts its archives, e.g. the keys mods are signed with.
    virtual bool IsBlocking() const {
        return false;
    }
    // Keeps receiving pushes for its route after the response.
    virtual bool IsSubscription() const {
        return false;
    }
    virtual void OnPush(int16_t status, uint8_t packetType, const std::string& body) {
    }
};

class Client {
  public:
    static Client& Instance();

    // Adds a packet Execute sends once connected.
    void Register(std::unique_ptr<IPacket> packet);
    // Connects and sends the registered packets, the blocking ones first. Waits for every response, so it runs off
    // the game thread.
    void Execute();
    Phase GetPhase() const {
        return mPhase.load(std::memory_order_relaxed);
    }
    bool IsConnected() const;

    // Sends a packet on the open connection from a worker thread, or with the next Execute when not connected.
    // Responses and pushes arrive on the network threads; use RunOnGameThread to touch the game.
    void RegisterLive(std::unique_ptr<IPacket> packet);

    // Sends a frame that gets no response: the route and the body. Does nothing when not connected.
    void SendRaw(const std::string& route, const void* data, size_t size);

    // Queues work for the game thread, run by Poll once per main loop iteration.
    void RunOnGameThread(std::function<void()> work);
    void Poll();

  private:
    Client() = default;
    ~Client();

    void Connect(const std::string& url);
    void SendAndReceive(IPacket& packet);
    void OnMessage(const ix::WebSocketMessagePtr& msg);
    void Keep(std::unique_ptr<IPacket> packet);
    void WorkerLoop();

    std::mutex mPacketsMtx;
    std::vector<std::unique_ptr<IPacket>> mPackets;
    std::vector<IPacket*> mSubscriptions;

    std::string mUrl;

    std::mutex mMtx;
    std::condition_variable mCv;
    std::atomic<Phase> mPhase{ Phase::Idle };
    bool mConnected = false;
    bool mWaitingForResponse = false;
    bool mResponseReady = false;
    bool mResponseValid = false;
    int16_t mResponseStatus = 0;
    std::string mResponseBody;

    std::mutex mQueueMtx;
    std::condition_variable mQueueCv;
    std::deque<std::unique_ptr<IPacket>> mQueue;
    std::thread mWorker;
    bool mStopping = false;

    std::mutex mGameMtx;
    std::vector<std::function<void()>> mGameWork;
};

template <typename T> struct PacketAutoReg {
    PacketAutoReg() {
        Client::Instance().Register(std::make_unique<T>());
    }
};

#define SATELLA_REGISTER_PACKET(T) static ::Satella::PacketAutoReg<T> _satella_pkt_##T

} // namespace Satella
#endif
