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

// ── Tick All ─────────────────────────────────────────────────
// Runs all simulation systems in order. Call once per logic frame.
inline void TickSystems(entt::registry& registry)
{
    MovementSystem(registry);
    CombatSystem(registry);
    CleanupSystem(registry);
}

} // namespace beigebox
