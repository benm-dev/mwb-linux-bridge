#pragma once
#include <string>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include "Protocol.h"
#include "CryptoHelper.h"

namespace mwb {

class NetworkManager {
public:
    NetworkManager(const std::string& host, int port, const std::string& key);
    ~NetworkManager();

    void SetOnMouseCallback(std::function<void(const MouseData&)> cb);
    void SetOnKeyboardCallback(std::function<void(const KeyboardData&)> cb);

    bool Connect();
    void RunLoop();
    bool SendPacket(MWBPacket& packet, bool isBig);
    void SetScreenSize(int w, int h) { m_screenW = w; m_screenH = h; }

private:
    std::string m_host;
    int m_port;
    std::string m_key;
    CryptoHelper m_crypto;
    int m_socket;
    uint32_t m_magic{0};
    uint32_t m_sessionId{0};
    uint32_t m_srcId{0};
    uint32_t m_desId{0};
    std::string m_myName;

    std::atomic<bool> m_running{false};
    bool m_handshakeDone{false};
    std::thread m_heartbeatThread;
    std::mutex m_sendMutex;

    uint32_t m_myId{0};     // Persistent machine ID, generated once, never changes
    int m_screenW{1920};
    int m_screenH{1080};

    // Server listener: accepts inbound connections from Windows
    int m_serverFd{-1};
    std::thread m_serverThread;

    std::function<void(const MouseData&)> m_onMouse;
    std::function<void(const KeyboardData&)> m_onKeyboard;

    void HeartbeatLoop();
    void SendIdentity();
    bool ConnectOutbound();
    void StartServerListener();
    void ServerListenerLoop();
    void HandleWindowsConnection(int fd);
};

}
