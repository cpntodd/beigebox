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
#include <vector>
#include <entt/entity/entity.hpp>

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

// ── Rank (Veterancy) ─────────────────────────────────────────
struct Rank {
    int level = 0, kills = 0, nextAt = 3;
};

// ── Personality (AI Autonomy) ────────────────────────────────
enum class Personality : uint8_t { Aggressive=0, Defensive=1, Explorer=2, Coward=3 };
struct UnitPersonality { Personality type = Personality::Aggressive; };

// ── Victory Condition ────────────────────────────────────────
struct VictoryCondition { int countdown=0; bool achieved=false; };

// ── Resource Harvesting ──────────────────────────────────────
enum class ResourceType : uint8_t { Ore=0, Gas=1, Scrap=2 };
struct ResourceNode { ResourceType type=ResourceType::Ore; int amount=1000; };
struct Harvester { int capacity=10, load=0; float rate=1.0f; };

// ── Garrison ─────────────────────────────────────────────────
struct Garrison { int capacity=4; std::vector<entt::entity> occupants; };

// ── Power Grid ───────────────────────────────────────────────
struct PowerProvider { int output=10; };
struct PowerConsumer { int demand=5; };

// ── Destructible Terrain ─────────────────────────────────────
struct DestructibleTerrain { int hp=100; int destroyedTile=0; };

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

