// engine/src/net/lockstep.cpp
// ─────────────────────────────────────────────────────────────
// Deterministic Lockstep Netcode implementation.
// ─────────────────────────────────────────────────────────────

#include "beigebox/net/lockstep.h"

#include <SDL2/SDL.h>
#include <cstring>
#include <cstdio>

#ifndef _WIN32
  #define SOCKET_ERROR -1
  #define INVALID_SOCKET -1
  #define closesocket close
#endif

namespace beigebox {

LockstepManager::LockstepManager()
{
    receivedInputs_.resize(kInputBufferSize);
}

LockstepManager::~LockstepManager()
{
    Shutdown();
}

// ── Init ─────────────────────────────────────────────────────

bool LockstepManager::InitHost(uint16_t port, int playerCount, int playoutDelay)
{
    socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ == INVALID_SOCKET) return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(socket_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
    {
        closesocket(socket_); socket_ = -1;
        return false;
    }

    MakeNonBlocking();
    localPort_ = port;
    connected_ = true;
    peerCount_ = 0;
    currentFrame_ = 0;
    ackedFrame_ = 0;
    oldestUnacked_ = 0;
    receivedUpTo_ = 0;
    playerCount_ = (playerCount >= 2 && playerCount <= kMaxPlayers) ? playerCount : 2;
    kPlayoutDelay_ = (playoutDelay >= 1 && playoutDelay <= 20) ? playoutDelay : 3;

    SDL_Log("Lockstep: hosting on port %u for %d players (playout delay: %d frames)",
        port, playerCount_, kPlayoutDelay_);
    return true;
}

bool LockstepManager::InitClient(const std::string& hostIP, uint16_t port, int playoutDelay)
{
    socket_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_ == INVALID_SOCKET) return false;

    MakeNonBlocking();

    peers_[0].addr = inet_addr(hostIP.c_str());
    peers_[0].port = port;
    peers_[0].lastAckedFrame = 0;
    peerCount_ = 1;

    connected_ = true;
    currentFrame_ = 0;
    kPlayoutDelay_ = (playoutDelay >= 1 && playoutDelay <= 20) ? playoutDelay : 3;

    SDL_Log("Lockstep: connected to %s:%u (playout delay: %d frames)",
        hostIP.c_str(), port, kPlayoutDelay_);
    return true;
}

void LockstepManager::Shutdown()
{
    if (socket_ >= 0) { closesocket(socket_); socket_ = -1; }
    connected_ = false;
}

bool LockstepManager::AddPeer(uint32_t ipAddr, uint16_t port)
{
    if (peerCount_ >= kMaxPlayers - 1) return false; // -1 for self
    for (int i = 0; i < peerCount_; ++i)
        if (peers_[i].addr == ipAddr && peers_[i].port == port) return true; // already added
    peers_[peerCount_].addr = ipAddr;
    peers_[peerCount_].port = port;
    peers_[peerCount_].lastAckedFrame = 0;
    peerCount_++;
    SDL_Log("Lockstep: peer added (%u.%u.%u.%u:%u) — total: %d",
        (ipAddr >> 24) & 0xFF, (ipAddr >> 16) & 0xFF,
        (ipAddr >> 8) & 0xFF, ipAddr & 0xFF, port, peerCount_);
    return true;
}

// ── Tick ─────────────────────────────────────────────────────

const FrameInput* LockstepManager::Tick(const PlayerInput& localInput)
{
    // Store our input for this frame
    inputBuffer_[currentFrame_ % kInputBufferSize].frame = currentFrame_;
    inputBuffer_[currentFrame_ % kInputBufferSize].inputs[0] = localInput;

    // Send inputs to peers
    SendInputs();

    // Try to receive from peers
    ReceivePacket();

    // Check if we have input for the next playout frame
    uint32_t nextExpected = receivedUpTo_ + 1;
    if (nextExpected <= ackedFrame_)
    {
        receivedUpTo_ = nextExpected;
        currentFrame_++;
        return &inputBuffer_[receivedUpTo_ % kInputBufferSize];
    }

    return nullptr; // wait — no input yet
}

// ── Send ─────────────────────────────────────────────────────

void LockstepManager::SendInputs()
{
    if (peerCount_ == 0) return;

    // Build packet: [numInputs(1B)] [frame(4B)] [packedInput(1B)] ...
    uint8_t packet[512];
    int offset = 0;

    // Number of inputs in this packet
    uint32_t count = currentFrame_ - oldestUnacked_ + 1;
    if (count > 64) count = 64; // safety cap
    packet[offset++] = static_cast<uint8_t>(count);

    // Pack inputs from oldest unacked to current
    for (uint32_t f = oldestUnacked_; f <= currentFrame_ && offset < 500; ++f)
    {
        uint32_t idx = f % kInputBufferSize;
        memcpy(packet + offset, &inputBuffer_[idx].frame, 4); offset += 4;
        packet[offset++] = inputBuffer_[idx].inputs[0].Pack();
    }

    // Send to all peers
    for (int p = 0; p < peerCount_; ++p)
    {
        sockaddr_in addr{};
        addr.sin_family = AF_INET;
        addr.sin_port = htons(peers_[p].port);
        addr.sin_addr.s_addr = peers_[p].addr;

        sendto(socket_, reinterpret_cast<const char*>(packet), offset, 0,
               reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    }
}

// ── Receive ──────────────────────────────────────────────────

bool LockstepManager::ReceivePacket()
{
    uint8_t buffer[512];
    sockaddr_in from{};
    socklen_t fromLen = sizeof(from);

    int n = recvfrom(socket_, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                     reinterpret_cast<sockaddr*>(&from), &fromLen);

    while (n > 0)
    {
        int offset = 0;
        uint8_t numInputs = buffer[offset++];

        for (int i = 0; i < numInputs && offset + 4 < n; ++i)
        {
            uint32_t frame;
            memcpy(&frame, buffer + offset, 4); offset += 4;
            uint8_t packed = buffer[offset++];

            if (frame > ackedFrame_)
            {
                uint32_t idx = frame % kInputBufferSize;
                inputBuffer_[idx].frame = frame;
                inputBuffer_[idx].inputs[0].Unpack(packed);
                ackedFrame_ = frame;

                // Register sender as a peer if new
                bool known = false;
                for (int p = 0; p < peerCount_; ++p)
                {
                    if (peers_[p].addr == from.sin_addr.s_addr && peers_[p].port == ntohs(from.sin_port))
                    {
                        peers_[p].lastAckedFrame = frame;
                        known = true;
                        break;
                    }
                }
                if (!known && peerCount_ < kMaxPlayers - 1)
                {
                    peers_[peerCount_].addr = from.sin_addr.s_addr;
                    peers_[peerCount_].port = ntohs(from.sin_port);
                    peers_[peerCount_].lastAckedFrame = frame;
                    peerCount_++;
                    SDL_Log("Lockstep: new peer %s:%u", inet_ntoa(from.sin_addr), ntohs(from.sin_port));
                }
            }
        }

        // Check for more packets
        n = recvfrom(socket_, reinterpret_cast<char*>(buffer), sizeof(buffer), 0,
                     reinterpret_cast<sockaddr*>(&from), &fromLen);
    }

    return true;
}

// ── LAN Discovery ───────────────────────────────────────────

void LockstepManager::BroadcastDiscovery(uint16_t port, const std::string& serverName)
{
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return;

    // Enable broadcast
    int broadcast = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST,
               reinterpret_cast<const char*>(&broadcast), sizeof(broadcast));

    // Build discovery packet: "MAD_DISCOVER" + server name
    std::string packet = "MAD_DISCOVER:" + serverName;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_BROADCAST;

    sendto(sock, packet.c_str(), packet.size(), 0,
           reinterpret_cast<sockaddr*>(&addr), sizeof(addr));

    closesocket(sock);
    SDL_Log("Lockstep: broadcast discovery beacon for '%s' on port %u",
        serverName.c_str(), port);
}

std::vector<std::string> LockstepManager::DiscoverHosts(uint16_t port, int timeoutMs)
{
    std::vector<std::string> results;

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) return results;

    // Bind to the discovery port
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        closesocket(sock);
        return results;
    }

    // Set timeout
#ifdef _WIN32
    DWORD tv = timeoutMs;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&tv), sizeof(tv));
#else
    struct timeval tv;
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    // Listen for broadcasts
    char buffer[256];
    sockaddr_in from{};
    socklen_t fromLen = sizeof(from);

    int n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                     reinterpret_cast<sockaddr*>(&from), &fromLen);

    while (n > 0)
    {
        buffer[n] = '\0';
        std::string msg(buffer);
        if (msg.find("MAD_DISCOVER:") == 0)
        {
            std::string serverName = msg.substr(13);
            char ipStr[32];
            snprintf(ipStr, sizeof(ipStr), "%u.%u.%u.%u",
                (ntohl(from.sin_addr.s_addr) >> 24) & 0xFF,
                (ntohl(from.sin_addr.s_addr) >> 16) & 0xFF,
                (ntohl(from.sin_addr.s_addr) >> 8) & 0xFF,
                ntohl(from.sin_addr.s_addr) & 0xFF);
            results.push_back(std::string(ipStr) + "|" + serverName);
        }

        n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0,
                     reinterpret_cast<sockaddr*>(&from), &fromLen);
    }

    closesocket(sock);
    SDL_Log("Lockstep: discovered %zu host(s)", results.size());
    return results;
}

// ── Helpers ──────────────────────────────────────────────────

void LockstepManager::MakeNonBlocking()
{
#ifdef _WIN32
    u_long mode = 1;
    ioctlsocket(socket_, FIONBIO, &mode);
#else
    int flags = fcntl(socket_, F_GETFL, 0);
    fcntl(socket_, F_SETFL, flags | O_NONBLOCK);
#endif
}

void LockstepManager::CloseSocket()
{
    if (socket_ >= 0) { closesocket(socket_); socket_ = -1; }
}

} // namespace beigebox
