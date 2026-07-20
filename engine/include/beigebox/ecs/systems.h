// engine/include/beigebox/ecs/systems.h
// ─────────────────────────────────────────────────────────────
// ECS Systems — deterministic game logic tick functions.
//
// All systems operate on the EnTT registry using FixedPoint
// math exclusively. No floating-point anywhere in simulation.
//
// Systems run at a fixed timestep (30 Hz logic rate).
// ─────────────────────────────────────────────────────────────
#pragma once

#include <entt/entt.hpp>

namespace beigebox {

// ── MovementSystem ───────────────────────────────────────────
// Drives entities with Transform + Movement toward their target.
// Uses fixed-point lerp — moves at constant speed per tick.
// Entities that reach their target have Movement removed.
void MovementSystem(entt::registry& registry);

// ── CombatSystem ─────────────────────────────────────────────
// Processes attack orders: entities with Weapon + Movement
// that are within range of their target deal damage each tick.
// On-kill: applies Dead tag, fires OnDeath event.
// (OnTakeDamage / OnDeath are fired by the Lua bridge, not here.)
void CombatSystem(entt::registry& registry);

// ── CleanupSystem ────────────────────────────────────────────
// Destroys all entities tagged with Dead.
void CleanupSystem(entt::registry& registry);

// ── ResourceSystem ───────────────────────────────────────────
// Harvesters near ResourceNodes gather resources each tick.
void ResourceSystem(entt::registry& registry);

// ── GarrisonSystem ───────────────────────────────────────────
// Entities with Movement targeting a Garrison entity enter it.
void GarrisonSystem(entt::registry& registry);

// ── PowerGridSystem ──────────────────────────────────────────
// Tracks whether total power output ≥ demand. Disables
// PowerConsumer entities when the grid is overloaded.
void PowerGridSystem(entt::registry& registry);

// ── DestructibleTerrainSystem ────────────────────────────────
// Terrain tiles with DestructibleTerrain take damage and
// transition to their destroyed tile type at 0 HP.
void DestructibleTerrainSystem(entt::registry& registry);

// ── SuperweaponSystem ────────────────────────────────────────
// Cooldown tick for superweapons. Ready flag set when
// cooldown reaches 0.
void SuperweaponSystem(entt::registry& registry);

// ── TechTreeSystem ───────────────────────────────────────────
// TechLabs with unresearched Technology advance research
// progress each tick until researched.
void TechTreeSystem(entt::registry& registry);

// ── FogOfWarSystem ───────────────────────────────────────────
// Updates Visibility exploredBy bitmask for tiles within
// sight range of each player's units.
void FogOfWarSystem(entt::registry& registry);

// ── Tick All ─────────────────────────────────────────────────
// Runs all simulation systems in order. Call once per logic frame.
inline void TickSystems(entt::registry& registry)
{
    MovementSystem(registry);
    CombatSystem(registry);
    ResourceSystem(registry);
    GarrisonSystem(registry);
    PowerGridSystem(registry);
    DestructibleTerrainSystem(registry);
    SuperweaponSystem(registry);
    TechTreeSystem(registry);
    FogOfWarSystem(registry);
    CleanupSystem(registry);
}

} // namespace beigebox
