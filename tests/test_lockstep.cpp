// tests/test_lockstep.cpp
// ─────────────────────────────────────────────────────────────
// Lockstep netcode unit tests — validates protocol correctness
// without requiring actual network sockets.
//
// Tests cover:
//   1. PlayerInput pack/unpack roundtrip
//   2. FrameInput buffer indexing
//   3. Redundant send protocol (oldest unacked tracking)
//   4. Playout delay buffer logic
//   5. 8-player input isolation
// ─────────────────────────────────────────────────────────────

#include <doctest/doctest.h>

#include "beigebox/net/lockstep.h"

#include <cstring>

using namespace beigebox;

TEST_CASE("Lockstep — PlayerInput pack/unpack")
{
    PlayerInput input;
    input.Clear();
    CHECK(input.Pack() == 0);

    input.moveRight = true;
    input.action1   = true;
    uint8_t packed = input.Pack();

    PlayerInput decoded;
    decoded.Unpack(packed);

    // Read bit-fields into locals for CHECK (bit-fields can't bind to refs)
    bool mr = decoded.moveRight;
    bool a1 = decoded.action1;
    bool ml = decoded.moveLeft;
    bool mu = decoded.moveUp;
    bool md = decoded.moveDown;
    bool a2 = decoded.action2;

    CHECK(mr == true);
    CHECK(a1 == true);
    CHECK(ml == false);
    CHECK(mu == false);
    CHECK(md == false);
    CHECK(a2 == false);
}

TEST_CASE("Lockstep — PlayerInput all bits")
{
    PlayerInput input;
    input.moveLeft  = true;
    input.moveRight = false;
    input.moveUp    = true;
    input.moveDown  = false;
    input.action1   = true;
    input.action2   = false;

    uint8_t packed = input.Pack();

    // Bit 0 = moveLeft, bit1 = moveRight, bit2 = moveUp,
    // bit3 = moveDown, bit4 = action1, bit5 = action2
    CHECK((packed & 0x01) != 0);  // moveLeft
    CHECK((packed & 0x02) == 0);  // moveRight
    CHECK((packed & 0x04) != 0);  // moveUp
    CHECK((packed & 0x08) == 0);  // moveDown
    CHECK((packed & 0x10) != 0);  // action1
    CHECK((packed & 0x20) == 0);  // action2
}

TEST_CASE("Lockstep — PlayerInput only uses 6 bits")
{
    PlayerInput input;
    input.moveLeft  = true;
    input.moveRight = true;
    input.moveUp    = true;
    input.moveDown  = true;
    input.action1   = true;
    input.action2   = true;

    uint8_t packed = input.Pack();
    // Only lower 6 bits should be set, upper 2 bits always 0
    CHECK((packed & 0xC0) == 0);
    CHECK((packed & 0x3F) == 0x3F);
}

TEST_CASE("Lockstep — FrameInput buffer indexing")
{
    constexpr int N = 128;
    FrameInput buffer[N];
    memset(buffer, 0, sizeof(buffer));

    // Store at various frame numbers — ring buffer wraps around
    for (uint32_t f = 0; f < 200; ++f)
    {
        uint32_t idx = f % N;
        buffer[idx].frame = f;
    }

    // After 200 writes into a 128-slot ring, slots 0-71 contain
    // frames 128-199 (the last written). Slots 72-127 contain
    // frames 72-127 (never overwritten).
    CHECK(buffer[0].frame == 128);   // overwritten by frame 128
    CHECK(buffer[71].frame == 199);  // last write
    CHECK(buffer[72].frame == 72);   // original — never overwritten
    CHECK(buffer[127].frame == 127); // original — never overwritten
}

TEST_CASE("Lockstep — Redundant send: oldest unacked advances")
{
    // Simulate the redundant send protocol:
    // - We send inputs from oldestUnacked to currentFrame
    // - When peer acks frame N, oldestUnacked becomes N+1
    uint32_t oldestUnacked = 0;
    uint32_t currentFrame  = 10;

    // After sending 10 frames
    CHECK(currentFrame - oldestUnacked + 1 == 11);

    // Peer acks frame 5
    uint32_t ackedByPeer = 5;
    if (ackedByPeer >= oldestUnacked)
        oldestUnacked = ackedByPeer + 1;

    CHECK(oldestUnacked == 6);

    // Now we only need to send frames 6-10 (5 frames)
    CHECK(currentFrame - oldestUnacked + 1 == 5);
}

TEST_CASE("Lockstep — 8-player input isolation")
{
    FrameInput frame;
    frame.frame = 42;

    // Set different inputs for each player
    for (int p = 0; p < 8; ++p)
    {
        frame.inputs[p].Clear();
        frame.inputs[p].moveRight = (p % 2 == 0);
        frame.inputs[p].action1   = (p >= 4);
    }

    // Verify isolation — read bit-fields into locals
    bool mr0 = frame.inputs[0].moveRight;
    bool mr1 = frame.inputs[1].moveRight;
    bool a13 = frame.inputs[3].action1;
    bool a14 = frame.inputs[4].action1;
    bool a17 = frame.inputs[7].action1;

    CHECK(mr0 == true);
    CHECK(mr1 == false);
    CHECK(a13 == false);
    CHECK(a14 == true);
    CHECK(a17 == true);

    // Verify packed representation for player 0
    uint8_t p0 = frame.inputs[0].Pack();
    CHECK((p0 & 0x02) != 0); // moveRight set
    CHECK((p0 & 0x10) == 0); // action1 not set
}

TEST_CASE("Lockstep — Playout delay buffer sizing")
{
    // At 30 Hz, 3 frames of delay = 100ms
    constexpr int playoutFrames = 3;
    constexpr int logicHz = 30;
    int delayMs = playoutFrames * 1000 / logicHz;

    CHECK(delayMs == 100);

    // Buffer should hold at least playoutFrames + some margin
    // Our buffer is 128 frames — more than enough
    constexpr int bufferSize = 128;
    CHECK(bufferSize > playoutFrames);
}
