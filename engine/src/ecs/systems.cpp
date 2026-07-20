// engine/src/ecs/systems.cpp
// ─────────────────────────────────────────────────────────────
// ECS System implementations — all deterministic, fixed-point.
// ─────────────────────────────────────────────────────────────

#include "beigebox/ecs/systems.h"
#include "beigebox/ecs/components.h"
#include "beigebox/core/fixed_point.h"

#include <cmath>

namespace beigebox {

// ── MovementSystem ───────────────────────────────────────────

void MovementSystem(entt::registry& registry)
{
    auto view = registry.view<Transform, Movement>();

    for (auto entity : view)
    {
        auto& t = registry.get<Transform>(entity);
        auto& m = registry.get<Movement>(entity);

        // ── Compute direction vector ─────────────────────────
        FixedPoint dx = m.targetX - t.x;
        FixedPoint dy = m.targetY - t.y;

        // ── Check if we've arrived ───────────────────────────
        // Within ~0.1 tiles → snap to target and stop moving.
        FixedPoint distSq = dx * dx + dy * dy;
        FixedPoint arrivalThreshold = FixedPoint::FromInt(0) + FixedPoint(FixedPoint::HALF / 2); // ~0.125 tiles^2

        if (distSq <= arrivalThreshold)
        {
            t.x = m.targetX;
            t.y = m.targetY;
            registry.remove<Movement>(entity);
            continue;
        }

        // ── Move toward target at constant speed ─────────────
        // Normalize direction and scale by speed.
        // For determinism, we use a fixed-point approximation of
        // the normalized step rather than true sqrt+division.
        FixedPoint absDx = Abs(dx);
        FixedPoint absDy = Abs(dy);

        // Manhattan-normalized step: move 1 speed-unit along the
        // dominant axis, and proportionally along the other.
        if (absDx > absDy)
        {
            FixedPoint ratio = (absDx.Raw() > 0)
                ? FixedPoint(static_cast<FixedPoint::raw_type>(
                    (static_cast<int64_t>(absDy.Raw()) << FixedPoint::FRACTIONAL_BITS) / absDx.Raw()))
                : FixedPoint::FromInt(0);

            FixedPoint stepX = (dx.Raw() >= 0) ? m.speed : FixedPoint(-m.speed.Raw());
            FixedPoint stepY = (dy.Raw() >= 0)
                ? FixedPoint(static_cast<FixedPoint::raw_type>(
                    (static_cast<int64_t>(ratio.Raw()) * m.speed.Raw()) >> FixedPoint::FRACTIONAL_BITS))
                : FixedPoint(-static_cast<FixedPoint::raw_type>(
                    (static_cast<int64_t>(ratio.Raw()) * m.speed.Raw()) >> FixedPoint::FRACTIONAL_BITS));

            t.x = t.x + stepX;
            t.y = t.y + stepY;
        }
        else
        {
            FixedPoint ratio = (absDy.Raw() > 0)
                ? FixedPoint(static_cast<FixedPoint::raw_type>(
                    (static_cast<int64_t>(absDx.Raw()) << FixedPoint::FRACTIONAL_BITS) / absDy.Raw()))
                : FixedPoint::FromInt(0);

            FixedPoint stepY = (dy.Raw() >= 0) ? m.speed : FixedPoint(-m.speed.Raw());
            FixedPoint stepX = (dx.Raw() >= 0)
                ? FixedPoint(static_cast<FixedPoint::raw_type>(
                    (static_cast<int64_t>(ratio.Raw()) * m.speed.Raw()) >> FixedPoint::FRACTIONAL_BITS))
                : FixedPoint(-static_cast<FixedPoint::raw_type>(
                    (static_cast<int64_t>(ratio.Raw()) * m.speed.Raw()) >> FixedPoint::FRACTIONAL_BITS));

            t.x = t.x + stepX;
            t.y = t.y + stepY;
        }
    }
}

// ── CombatSystem ─────────────────────────────────────────────

void CombatSystem(entt::registry& registry)
{
    // Terrain combat modifiers: FrozenGround = -10% damage,
    // ThawedGround = normal, SalvageField = +5% damage (cover)
    // Applied via Lua Combat.DealDamage — modifier lookup table.
    (void)registry;
}

// ── CleanupSystem ────────────────────────────────────────────

void CleanupSystem(entt::registry& registry)
{
    registry.view<Dead>().each([&](entt::entity entity) {
        registry.destroy(entity);
    });
}

} // namespace beigebox
