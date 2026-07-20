// engine/include/beigebox/ecs/components.h
// ─────────────────────────────────────────────────────────────
// ECS Component Definitions
//
// All gameplay components use FixedPoint for deterministic
// lockstep compatibility. No float/double in any component.
//
// Components are plain structs (POD where possible) to
// maximize EnTT's cache-friendly contiguous storage.
// ─────────────────────────────────────────────────────────────
#pragma once

#include "beigebox/core/fixed_point.h"
#include <cstdint>

namespace beigebox {

// ── Transform ────────────────────────────────────────────────
// World-space position in tile units (Q24.8 fixed-point).
// Origin (0,0) is the top-left tile of the map.
struct Transform
{
    FixedPoint x;
    FixedPoint y;
};

// ── Health ───────────────────────────────────────────────────
// Current and maximum health, both in Q24.8.
struct Health
{
    FixedPoint current;
    FixedPoint max;
};

// ── Player ───────────────────────────────────────────────────
// Ownership tag — which faction controls this entity.
// factionId 0 = neutral / world.
struct Player
{
    int factionId = 0;
};

// ── Velocity ─────────────────────────────────────────────────
// Movement direction and speed for the movement system.
struct Velocity
{
    FixedPoint dx;
    FixedPoint dy;
};

// ── Renderable ───────────────────────────────────────────────
// Hints for the renderer. For now, just a color.
struct Renderable
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
};

} // namespace beigebox
