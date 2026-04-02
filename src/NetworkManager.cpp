#include "NetworkManager.h"
#include <iostream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <vector>
#include <endian.h>
#include <csignal>
#include <execinfo.h>

#include <openssl/rand.h>

namespace mwb {

static void handle_sig(int sig) {
    void *array[10];
    size_t size = backtrace(array, 10);
    fprintf(stderr, "FATAL: Signal %d caught. Backtrace:\n", sig);
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    _exit(1);
}

NetworkManager::NetworkManager(const std::string& host, int port, const std::string& key)
    : m_host(host), m_port(port), m_key(key), m_crypto(key), m_socket(-1) {
    std::signal(SIGSEGV, handle_sig);
    std::signal(SIGABRT, handle_sig);
    std::signal(SIGPIPE, SIG_IGN);
}

NetworkManager::~NetworkManager() {
    m_running = false;
    if (m_serverFd >= 0) { shutdown(m_serverFd, SHUT_RDWR); close(m_serverFd); m_serverFd = -1; }
    if (m_heartbeatThread.joinable()) m_heartbeatThread.join();
    if (m_serverThread.joinable()) m_serverThread.join();
    if (m_socket >= 0) {
        shutdown(m_socket, SHUT_RDWR);
        close(m_socket);
    }
}

bool NetworkManager::Connect() {
    if (m_myName.empty()) { char hostname[256]; gethostname(hostname, 256); m_myName = hostname; }
    if (m_myId == 0) {
        RAND_bytes(reinterpret_cast<uint8_t*>(&m_myId), 4);
        if (m_myId == 0) m_myId = 0xDEAD0001;
        printf("[IDENT] Permanent machine ID: %08x  name: %s\n", m_myId, m_myName.c_str());
        fflush(stdout);
    }
    m_running = true; // must be set before ServerListenerLoop thread starts
    // Start listening FIRST so Windows can connect back immediately after we reach it
    StartServerListener();
    return ConnectOutbound();
}

bool NetworkManager::ConnectOutbound() {
    // Close any previous socket first so HeartbeatLoop can't race on it
    if (m_socket >= 0) { close(m_socket); m_socket = -1; }
    // Reset crypto stream state for the new connection, holding sendMutex so
    // HeartbeatLoop can't be mid-EncryptStream when we reinitialize the context
    {
        std::lock_guard<std::mutex> lk(m_sendMutex);
        m_sessionId = 0; m_srcId = m_myId; m_desId = 0;
        m_handshakeDone = false;
        m_crypto.Reset();
    }

    m_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (m_socket < 0) { perror("[OUTBOUND] socket"); return false; }

    struct timeval tv; tv.tv_sec = 5; tv.tv_usec = 0;
    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
    int flag = 1; setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof(int));

    struct sockaddr_in s; std::memset(&s, 0, sizeof(s));
    s.sin_family = AF_INET; s.sin_port = htons(m_port);
    if (inet_pton(AF_INET, m_host.c_str(), &s.sin_addr) <= 0) return false;

    printf("[OUTBOUND] Connecting to %s:%d...\n", m_host.c_str(), m_port);
    fflush(stdout);
    if (connect(m_socket, (struct sockaddr*)&s, sizeof(s)) < 0) {
        perror("[OUTBOUND] connect failed");
        close(m_socket); m_socket = -1;
        return false;
    }

    m_magic = m_crypto.Get24BitHash();

    // Phase 1: IV Sync Noise
    std::vector<uint8_t> noise(16);
    RAND_bytes(noise.data(), 16);
    std::vector<uint8_t> encNoise;
    if (!m_crypto.EncryptStream(noise, encNoise)) return false;
    if (write(m_socket, encNoise.data(), 16) != 16) return false;

    // Phase 2: Type 126 Handshake
    MWBPacket pk; std::memset(&pk, 0, sizeof(pk));
    pk.type = 126;
    RAND_bytes(pk.data, 48);
    return SendPacket(pk, true);
}

static bool IsBig(uint8_t t) {
    if (t==3||t==20||t==21||t==51||t==69||t==76||t==78||t==79||t==124||t==125||t==126||t==127||t==128) return true;
    return (t & 128) == 128;
}

bool NetworkManager::SendPacket(MWBPacket& pkt, bool isBig) {
    std::lock_guard<std::mutex> lock(m_sendMutex);
    if (m_socket < 0) return false;
    pkt.magic0 = (m_magic >> 16) & 0xFF;
    pkt.magic1 = (m_magic >> 24) & 0xFF;

    uint32_t sId = htole32(m_sessionId), sSrc = htole32(m_srcId), sDes = htole32(m_desId);
    if (pkt.id == 0) std::memcpy(&pkt.id, &sId, 4);
    if (pkt.src == 0) std::memcpy(&pkt.src, &sSrc, 4);
    if (pkt.des == 0) std::memcpy(&pkt.des, &sDes, 4);
    if (isBig) {
        // C# encodes name as: Common.GetBytes(value.PadRight(32, ' '))
        // = plain ASCII, right-padded with spaces (0x20) to 32 bytes.
        // We were sending UTF-16LE with null padding — server read only "c" (first char + null stop).
        char n[32];
        std::memset(n, ' ', 32);  // pad with spaces, not nulls
        size_t len = std::min(m_myName.length(), (size_t)32);
        std::memcpy(n, m_myName.data(), len);
        std::memcpy(&pkt.data[16], n, 32);
    }
    pkt.checksum = 0; uint8_t cs = 0; uint8_t* p = reinterpret_cast<uint8_t*>(&pkt);
    for (int i=2; i < 32; i++) cs += p[i];
    pkt.checksum = cs;
    std::vector<uint8_t> plain(isBig ? 64 : 32), enc;
    std::memcpy(plain.data(), &pkt, plain.size());
    if (!m_crypto.EncryptStream(plain, enc)) return false;
    return write(m_socket, enc.data(), enc.size()) == (ssize_t)enc.size();
}

// Send type 51 (Heartbeat_ex) with our machine name and screen dimensions.
// Windows MWB uses this to know our screen size and associate us with the layout.
void NetworkManager::SendIdentity() {
    MWBPacket id; std::memset(&id, 0, sizeof(id));
    id.type = 51;
    // Pack screen dimensions into the MouseData fields (data[0..3])
    uint16_t w = static_cast<uint16_t>(m_screenW);
    uint16_t h = static_cast<uint16_t>(m_screenH);
    std::memcpy(&id.data[0], &w, 2);
    std::memcpy(&id.data[2], &h, 2);
    // des = broadcast (like the server does with its own type 51)
    uint32_t bcast = htole32(0x000000ff);
    std::memcpy(&id.des, &bcast, 4);
    printf("[IDENT] Sending identity: name=%s screen=%dx%d\n", m_myName.c_str(), m_screenW, m_screenH);
    fflush(stdout);
    SendPacket(id, true);
}

void NetworkManager::HeartbeatLoop() {
    while (m_running) {
        std::this_thread::sleep_for(std::chrono::seconds(2));
        if (!m_running) break;
        if (m_sessionId == 0) continue;
        // Send type 51 (Heartbeat_ex with identity) so Windows keeps us in the machine list
        SendIdentity();
    }
}

void NetworkManager::RunLoop() {
    // m_running already set in Connect(); StartServerListener() already called there
    m_heartbeatThread = std::thread(&NetworkManager::HeartbeatLoop, this);

    // These lambdas always use the current m_socket / m_crypto via 'this',
    // so they stay valid across reconnects.
    auto recvN = [&](uint8_t* b, int n) -> bool {
        int g = 0;
        while (g < n && m_running) {
            int r = read(m_socket, b + g, n - g);
            if (r < 0) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                fprintf(stderr, "[OUTBOUND] recvN errno=%d\n", errno);
                return false;
            } else if (r == 0) {
                fprintf(stderr, "[OUTBOUND] recvN: socket closed gracefully\n");
                return false;
            }
            g += r;
        }
        return m_running;
    };
    auto recvP = [&](std::vector<uint8_t>& out) -> bool {
        uint8_t b1[32];
        if (!recvN(b1, 32)) return false;
        std::vector<uint8_t> vt(b1, b1 + 32), d1;
        if (!m_crypto.DecryptStream(vt, d1)) return false;
        out = std::move(d1);
        if (IsBig(out[0])) {
            uint8_t b2[32];
            if (!recvN(b2, 32)) return false;
            std::vector<uint8_t> v2(b2, b2 + 32), p2;
            if (!m_crypto.DecryptStream(v2, p2)) return false;
            out.insert(out.end(), p2.begin(), p2.end());
        }
        return true;
    };

    bool firstRun = true;

    while (m_running) {
        if (!firstRun) {
            printf("[RECONNECT] Waiting 5s before reconnect...\n");
            fflush(stdout);
            for (int i = 0; i < 50 && m_running; i++)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            if (!m_running) break;
            if (!ConnectOutbound()) {
                printf("[RECONNECT] ConnectOutbound failed, will retry\n");
                fflush(stdout);
                continue;
            }
        }
        firstRun = false;

        // Receive server noise (advances decrypt stream state)
        uint8_t ns[16];
        if (!recvN(ns, 16)) {
            fprintf(stderr, "[OUTBOUND] Failed to receive server noise\n");
            continue;
        }
        std::vector<uint8_t> vN(ns, ns + 16), dN;
        m_crypto.DecryptStream(vN, dN);
        printf("[OUTBOUND] Server noise received, starting handshake\n");
        fflush(stdout);

        // Handshake loop
        for (int i = 0; i < 20 && m_running && !m_handshakeDone; i++) {
            std::vector<uint8_t> d;
            if (!recvP(d)) break;
            MWBPacket* p = reinterpret_cast<MWBPacket*>(d.data());
            uint32_t sid = le32toh(p->id), ssrc = le32toh(p->src), sdes = le32toh(p->des);

            printf("[RECV] #%d type=%d size=%zu sid=%08x src=%08x des=%08x\n",
                   i, p->type, d.size(), sid, ssrc, sdes);
            fflush(stdout);

            if (sid != 0) m_sessionId = sid;
            // m_srcId = m_myId, never overwritten; learn Windows's ID from ssrc
            if (ssrc != 0) m_desId = ssrc;

            if (p->type == 126) {
                // Adopt the ID Windows has cached for us (des field).
                // This ensures pool["carbon"].Id == socket.MachineId on the Windows side,
                // so mouse unicast routing works regardless of TryUpdateMachineID.
                if (sdes != 0 && sdes != 0xFF && sdes != 0xFFFFFFFF) {
                    if (m_myId != sdes) {
                        printf("[IDENT] Adopting Windows cached ID for us: %08x (was %08x)\n", sdes, m_myId);
                        fflush(stdout);
                        m_myId = sdes;
                        m_srcId = sdes;
                    }
                }
                MWBPacket ack; std::memset(&ack, 0, sizeof(ack));
                ack.type = 127;
                for (int j = 0; j < 16; j++) ack.data[j] = ~p->data[j];
                SendPacket(ack, true);
            } else if (p->type == 127) {
                printf("[OUTBOUND] HandshakeAck received. Our ID: %08x  Server ID: %08x\n", m_srcId, m_desId);
                fflush(stdout);
                m_handshakeDone = true;
            }
        }

        if (!m_handshakeDone) {
            fprintf(stderr, "[OUTBOUND] Handshake failed, will reconnect\n");
            continue;
        }

        printf("[SUCCESS] MWB Session Established. Entering Main Loop.\n");
        fflush(stdout);
        SendIdentity();

        // Main packet loop — exits on any receive error, then outer loop reconnects
        while (m_running) {
            std::vector<uint8_t> d;
            if (!recvP(d)) {
                fprintf(stderr, "[OUTBOUND] recvP failed, connection lost\n");
                break;
            }
            MWBPacket* p = reinterpret_cast<MWBPacket*>(d.data());
            uint8_t t = p->type;
            uint32_t sid = le32toh(p->id), ssrc = le32toh(p->src), sdes = le32toh(p->des);

            if (sid != 0) m_sessionId = sid;
            // m_srcId = m_myId, never overwritten
            if (ssrc != 0) m_desId = ssrc;

            if (t == 51 || t == 52 || t == 53 || t == 20) {
                static int hb_count = 0;
                if (hb_count++ % 10 == 0) printf("[HEARTBEAT] Received type %d from server\n", t);
                fflush(stdout);
            } else if (t != 122 && t != 123) {
                printf("[PKT] type=%d sid=%08x src=%08x des=%08x\n", t, sid, ssrc, sdes);
                fflush(stdout);
            }

            if (t == 123) {
                printf("[MOUSE-RAW] data[0..15]: ");
                for (int b = 0; b < 16; b++) printf("%02x ", p->data[b]);
                printf("\n");
                MouseData m;
                std::memcpy(&m, &p->data[0], sizeof(MouseData));
                printf("[MOUSE] x=%d y=%d wParam=0x%04x mData=0x%08x\n", m.x, m.y, m.wParam, m.mouseData);
                fflush(stdout);
                if (m_onMouse) m_onMouse(m);
            } else if (t == 122) {
                KeyboardData k;
                std::memcpy(&k, &p->data[8], 8);
                printf("[KEY] vk=0x%02x flags=0x%04x\n", k.vkCode, k.flags);
                fflush(stdout);
                if (m_onKeyboard) m_onKeyboard(k);
            } else if (t == 126) {
                MWBPacket ack; std::memset(&ack, 0, sizeof(ack));
                ack.type = 127;
                for (int j = 0; j < 16; j++) ack.data[j] = ~p->data[j];
                SendPacket(ack, true);
            }
        }
        // Fall through to outer loop — will wait and reconnect
    }
}

void NetworkManager::SetOnMouseCallback(std::function<void(const MouseData&)> cb) { m_onMouse = cb; }
void NetworkManager::SetOnKeyboardCallback(std::function<void(const KeyboardData&)> cb) { m_onKeyboard = cb; }

void NetworkManager::StartServerListener() {
    m_serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (m_serverFd < 0) { perror("[SERVER] socket"); return; }

    int opt = 1;
    setsockopt(m_serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr;
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(m_port);

    if (bind(m_serverFd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[SERVER] bind");
        close(m_serverFd); m_serverFd = -1;
        return;
    }
    if (listen(m_serverFd, 4) < 0) {
        perror("[SERVER] listen");
        close(m_serverFd); m_serverFd = -1;
        return;
    }
    printf("[SERVER] Listening on port %d for incoming Windows connections\n", m_port);
    fflush(stdout);

    m_serverThread = std::thread(&NetworkManager::ServerListenerLoop, this);
}

void NetworkManager::ServerListenerLoop() {
    while (m_running) {
        struct sockaddr_in caddr;
        socklen_t clen = sizeof(caddr);
        int cfd = accept(m_serverFd, (struct sockaddr*)&caddr, &clen);
        if (cfd < 0) {
            if (m_running) perror("[SERVER] accept");
            break;
        }
        char ipbuf[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &caddr.sin_addr, ipbuf, sizeof(ipbuf));
        printf("[SERVER] Accepted connection from %s\n", ipbuf);
        fflush(stdout);

        // Handle in a detached thread so accept loop continues
        std::thread(&NetworkManager::HandleWindowsConnection, this, cfd).detach();
    }
}

void NetworkManager::HandleWindowsConnection(int fd) {
    CryptoHelper sc(m_key);
    uint32_t magic = sc.Get24BitHash();

    auto sendRaw = [&](const void* buf, int n) -> bool {
        return write(fd, buf, n) == n;
    };
    auto recvN = [&](uint8_t* b, int n) -> bool {
        int g = 0;
        while (g < n && m_running) {
            int r = read(fd, b + g, n - g);
            if (r <= 0) return false;
            g += r;
        }
        return m_running;
    };

    // Exchange noise: receive Windows noise first, then send ours
    uint8_t winNoise[16];
    if (!recvN(winNoise, 16)) { close(fd); return; }
    std::vector<uint8_t> wv(winNoise, winNoise + 16), wd;
    sc.DecryptStream(wv, wd);  // advance decipher state

    std::vector<uint8_t> myNoise(16);
    RAND_bytes(myNoise.data(), 16);
    std::vector<uint8_t> encNoise;
    sc.EncryptStream(myNoise, encNoise);
    if (!sendRaw(encNoise.data(), 16)) { close(fd); return; }

    printf("[SERVER] Noise exchanged with Windows, starting handshake\n");
    fflush(stdout);

    // Helper: send a big packet using sc
    auto sendPkt = [&](MWBPacket& pkt, bool isBig) -> bool {
        pkt.magic0 = (magic >> 16) & 0xFF;
        pkt.magic1 = (magic >> 24) & 0xFF;
        // fill src/des/id from what we know (they're already set by caller)
        pkt.checksum = 0;
        uint8_t cs = 0;
        uint8_t* pp = reinterpret_cast<uint8_t*>(&pkt);
        for (int i = 2; i < 32; i++) cs += pp[i];
        pkt.checksum = cs;

        if (isBig) {
            // write machine name into second block
            char n[32];
            std::memset(n, ' ', 32);
            size_t len = std::min(m_myName.length(), (size_t)32);
            std::memcpy(n, m_myName.data(), len);
            std::memcpy(&pkt.data[16], n, 32);
        }

        std::vector<uint8_t> plain(isBig ? 64 : 32), enc;
        std::memcpy(plain.data(), &pkt, plain.size());
        if (!sc.EncryptStream(plain, enc)) return false;
        return sendRaw(enc.data(), enc.size());
    };

    // Handshake: receive type 126 from Windows, respond with type 127
    int handshakePkts = 0;
    bool trusted = false;

    for (int i = 0; i < 30 && m_running && !trusted; i++) {
        uint8_t b1[32];
        if (!recvN(b1, 32)) break;
        std::vector<uint8_t> vt(b1, b1 + 32), d1;
        if (!sc.DecryptStream(vt, d1)) { printf("[SERVER] DecryptStream failed at pkt %d\n", i); break; }

        bool big = IsBig(d1[0]);
        std::vector<uint8_t> full = d1;
        if (big) {
            uint8_t b2[32];
            if (!recvN(b2, 32)) break;
            std::vector<uint8_t> v2(b2, b2 + 32), p2;
            if (!sc.DecryptStream(v2, p2)) break;
            full.insert(full.end(), p2.begin(), p2.end());
        }

        MWBPacket* p = reinterpret_cast<MWBPacket*>(full.data());
        uint32_t sdes = le32toh(p->des);

        printf("[SERVER-RECV] type=%d des=%08x\n", p->type, sdes);
        fflush(stdout);

        if (p->type == 126) {
            handshakePkts++;

            // Adopt the ID Windows has cached for us so pool["carbon"].Id == socket.MachineId
            if (sdes != 0 && sdes != 0xFF && sdes != 0xFFFFFFFF && m_myId != sdes) {
                printf("[SERVER] Adopting Windows cached ID: %08x (was %08x)\n", sdes, m_myId);
                fflush(stdout);
                m_myId = sdes;
                m_srcId = sdes;
            }

            // Respond with type 127 (XOR complement), using our persistent m_myId as src
            // so Windows's UpdateClientSockets sets socket.MachineId = m_myId from the start
            MWBPacket ack;
            std::memset(&ack, 0, sizeof(ack));
            ack.type = 127;
            for (int j = 0; j < 16; j++) ack.data[j] = ~p->data[j];
            uint32_t myid = htole32(m_myId);
            std::memcpy(&ack.src, &myid, 4);
            if (!sendPkt(ack, true)) break;
        } else if (p->type == 127 || p->type == 51 || p->type == 52) {
            // Handshake validated from Windows side, or heartbeats starting
            trusted = true;
            printf("[SERVER] Handshake complete. myId=%08x, handshakePkts=%d\n", m_myId, handshakePkts);
            fflush(stdout);
        }
    }

    if (!trusted) {
        printf("[SERVER] Handshake failed, closing\n");
        close(fd); return;
    }

    // Send identity immediately so Windows updates MachinePool for carbon
    {
        MWBPacket id;
        std::memset(&id, 0, sizeof(id));
        id.type = 51; // Heartbeat_ex with identity
        uint16_t w = static_cast<uint16_t>(m_screenW);
        uint16_t h = static_cast<uint16_t>(m_screenH);
        std::memcpy(&id.data[0], &w, 2);
        std::memcpy(&id.data[2], &h, 2);
        uint32_t bcast = htole32(0x000000ff);
        std::memcpy(&id.des, &bcast, 4);
        uint32_t myid = htole32(m_myId);
        std::memcpy(&id.src, &myid, 4);
        sendPkt(id, true);
        printf("[SERVER] Sent identity on inbound socket (myId=%08x)\n", m_myId);
        fflush(stdout);
    }

    // Main receive loop for this inbound connection
    while (m_running) {
        uint8_t b1[32];
        if (!recvN(b1, 32)) break;
        std::vector<uint8_t> vt(b1, b1 + 32), d1;
        if (!sc.DecryptStream(vt, d1)) break;

        bool big = IsBig(d1[0]);
        std::vector<uint8_t> full = d1;
        if (big) {
            uint8_t b2[32];
            if (!recvN(b2, 32)) break;
            std::vector<uint8_t> v2(b2, b2 + 32), p2;
            if (!sc.DecryptStream(v2, p2)) break;
            full.insert(full.end(), p2.begin(), p2.end());
        }

        MWBPacket* p = reinterpret_cast<MWBPacket*>(full.data());
        uint8_t t = p->type;

        if (t == 123) {
            MouseData m;
            std::memcpy(&m, &p->data[0], sizeof(MouseData));
            printf("[SERVER-MOUSE] x=%d y=%d wParam=0x%04x\n", m.x, m.y, m.wParam);
            fflush(stdout);
            if (m_onMouse) m_onMouse(m);
        } else if (t == 122) {
            KeyboardData k;
            std::memcpy(&k, &p->data[8], 8);
            printf("[SERVER-KEY] vk=0x%02x flags=0x%04x\n", k.vkCode, k.flags);
            fflush(stdout);
            if (m_onKeyboard) m_onKeyboard(k);
        } else if (t == 126) {
            // Re-handshake request
            MWBPacket ack;
            std::memset(&ack, 0, sizeof(ack));
            ack.type = 127;
            for (int j = 0; j < 16; j++) ack.data[j] = ~p->data[j];
            uint32_t myid = htole32(m_myId);
            std::memcpy(&ack.src, &myid, 4);
            sendPkt(ack, true);
        } else if (t == 51 || t == 52 || t == 20) {
            // Heartbeat from Windows: echo back a small heartbeat to keep connection alive
            static int hb_cnt = 0;
            if (hb_cnt++ % 5 == 0) {
                printf("[SERVER-HB] Received type %d from Windows, echoing back\n", t);
                fflush(stdout);
            }
            MWBPacket hb;
            std::memset(&hb, 0, sizeof(hb));
            hb.type = 52; // small heartbeat
            uint32_t myid = htole32(m_myId);
            std::memcpy(&hb.src, &myid, 4);
            sendPkt(hb, false);
        } else {
            printf("[SERVER-PKT] type=%d\n", t);
            fflush(stdout);
        }
    }

    printf("[SERVER] Inbound connection closed\n");
    fflush(stdout);
    close(fd);
}

}
