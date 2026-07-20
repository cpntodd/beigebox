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
#include <string>

namespace beigebox {

// ── Transform ────────────────────────────────────────────────
struct Transform
{
    FixedPoint x;
    FixedPoint y;
};

// ── Health ───────────────────────────────────────────────────
struct Health
{
    FixedPoint current;
    FixedPoint max;
};

// ── Player ───────────────────────────────────────────────────
struct Player
{
    int factionId = 0;  // 0 = neutral/world, 1+ = faction
};

// ── Movement ─────────────────────────────────────────────────
// Target-based movement. The MovementSystem drives the entity
// toward targetX/targetY at the given speed each tick.
struct Movement
{
    FixedPoint targetX;
    FixedPoint targetY;
    FixedPoint speed;  // tiles per second, Q24.8
};

// ── Weapon ───────────────────────────────────────────────────
struct Weapon
{
    FixedPoint damage;
    FixedPoint range;      // max attack distance in tile units
    int        damageType = 0;  // 0=kinetic, 1=thermal, 2=pure
};

// ── FactionResources ─────────────────────────────────────────
// Per-faction economy. Typically attached to a singleton
// "faction manager" entity.
struct FactionResources
{
    FixedPoint salvage;
    FixedPoint heat;
};

// ── HeatSource ───────────────────────────────────────────────
// Marks an entity as emitting heat into the Thaw Grid.
struct HeatSource
{
    FixedPoint radius;     // thaw radius in tile units
    FixedPoint intensity;  // heat added per tick at center
    uint32_t   sourceId = 0;
};

// ── Dead ─────────────────────────────────────────────────────
// Tag component — entity is pending removal at end of tick.
struct Dead {};

// ── Renderable ───────────────────────────────────────────────
struct Renderable
{
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
};

// ── Sprite ───────────────────────────────────────────────────
// References a sprite by name (loaded from assets/sprites/).
// The renderer looks up the Texture from the SpriteRegistry.
struct Sprite
{
    std::string name;  // filename in assets/sprites/, e.g. "driller.png"
};

// ── Velocity (legacy, kept for compatibility) ────────────────
struct Velocity
{
    FixedPoint dx;
    FixedPoint dy;
};

} // namespace beigebox

