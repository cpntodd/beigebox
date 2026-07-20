// engine/include/beigebox/net/lockstep.h
// ─────────────────────────────────────────────────────────────
// Deterministic Lockstep Netcode
//
// Based on Glenn Fiedler's architecture:
//   - Fixed 30 Hz timestep
//   - UDP with redundant input sending (no TCP retransmit stalls)
//   - Playout delay buffer (3 frames / ~100ms)
//   - Each packet contains all unacked inputs
//
// For 8-player LAN: each peer sends inputs to all others.
// Bandwidth: ~6 bits per input × 30 Hz × 8 players ≈ 180 bytes/s
// ─────────────────────────────────────────────────────────────
#pragma once

#include <cstdint>
#include <vector>
#include <array>
#include <string>

#ifdef _WIN32
  #include <winsock2.h>
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <fcntl.h>
#endif

namespace beigebox {

// ── Player Input ─────────────────────────────────────────────
// Per-frame input for one player. 6 bits → very compact.
struct PlayerInput
{
    bool moveLeft   : 1;
    bool moveRight  : 1;
    bool moveUp     : 1;
    bool moveDown   : 1;
    bool action1    : 1;  // primary action / attack
    bool action2    : 1;  // secondary action / ability

    void Clear() { *reinterpret_cast<uint8_t*>(this) = 0; }
    uint8_t Pack() const { return *reinterpret_cast<const uint8_t*>(this) & 0x3F; }
    void Unpack(uint8_t byte) { *reinterpret_cast<uint8_t*>(this) = byte & 0x3F; }
};

// ── Frame Input (all players) ────────────────────────────────
struct FrameInput
{
    uint32_t frame = 0;
    PlayerInput inputs[8];  // up to 8 players
};

// ── Lockstep Manager ─────────────────────────────────────────
class LockstepManager
{
public:
    LockstepManager();
    ~LockstepManager();

    // ── Configuration ────────────────────────────────────────
    // Initialize as host (player 0) or client connecting to host.
    bool InitHost(uint16_t port);
    bool InitClient(const std::string& hostIP, uint16_t port);

    void Shutdown();

    // ── Per-Frame ────────────────────────────────────────────
    // Call at the START of each logic frame (30 Hz).
    // 1. Sample local input for this frame
    // 2. Send our input + all unacked inputs to peers
    // 3. Try to receive inputs from peers
    // 4. If we have input for the next expected frame, return it
    //
    // Returns nullptr if we need to wait (no input yet for next frame).
    // This causes the simulation to stall briefly — the playout buffer
    // smooths this over the network.
    const FrameInput* Tick(const PlayerInput& localInput);

    // Get the current simulation frame number.
    uint32_t CurrentFrame() const { return currentFrame_; }

    // Is the connection still alive?
    bool Connected() const { return connected_; }

    // Number of peers connected.
    int PeerCount() const { return peerCount_; }

private:
    struct Peer {
        uint32_t       addr;
        uint16_t       port;
        uint32_t       lastAckedFrame = 0;
    };

    // Send our input buffer (all frames since last ack) to peers.
    void SendInputs();

    // Try to receive an ACK or input packet from a peer.
    bool ReceivePacket();

    // Buffer of recent inputs (last N frames). We resend from
    // the oldest unacked frame up to current.
    static constexpr int kInputBufferSize = 128;
    std::array<FrameInput, kInputBufferSize> inputBuffer_;
    uint32_t oldestUnacked_ = 0;
    uint32_t currentFrame_  = 0;
    uint32_t ackedFrame_    = 0;    // highest frame acked by all peers

    // Playout delay: buffer inputs for smooth delivery
    static constexpr int kPlayoutDelay = 3;  // frames (~100ms at 30Hz)
    std::vector<FrameInput> receivedInputs_; // indexed by frame
    uint32_t receivedUpTo_ = 0;

    // Networking
    int socket_ = -1;
    uint16_t localPort_ = 0;
    bool connected_ = false;
    int peerCount_ = 0;
    Peer peers_[8];

    void MakeNonBlocking();
    void CloseSocket();
};

} // namespace beigebox
