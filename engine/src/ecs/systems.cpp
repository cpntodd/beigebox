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

// ── ResourceSystem ───────────────────────────────────────────

void ResourceSystem(entt::registry& registry)
{
    // Harvesters with load < capacity near a ResourceNode
    // increase load by their rate each tick. When the node
    // is depleted (amount <= 0), remove the ResourceNode.
    auto harvesterView = registry.view<Harvester, Transform>();

    for (auto entity : harvesterView)
    {
        auto& harv = registry.get<Harvester>(entity);
        if (harv.load >= harv.capacity) continue;

        auto& t = registry.get<Transform>(entity);
        auto nodeView = registry.view<ResourceNode, Transform>();
        for (auto nodeEnt : nodeView)
        {
            auto& nodeT = registry.get<Transform>(nodeEnt);
            FixedPoint dx = nodeT.x - t.x;
            FixedPoint dy = nodeT.y - t.y;
            FixedPoint dist = Abs(dx) + Abs(dy); // Manhattan

            if (dist <= FixedPoint::FromInt(2)) // within 2 tiles
            {
                harv.load += 1;
                auto& node = registry.get<ResourceNode>(nodeEnt);
                node.amount -= 1;
                if (node.amount <= 0)
                    registry.destroy(nodeEnt);
                break;
            }
        }
    }
}

// ── GarrisonSystem ───────────────────────────────────────────

void GarrisonSystem(entt::registry& registry)
{
    // When a unit reaches a garrisonable building, remove its
    // Movement and add it to the building's occupants list.
    auto garrisonView = registry.view<Garrison, Transform>();

    for (auto garrisonEnt : garrisonView)
    {
        auto& g = registry.get<Garrison>(garrisonEnt);
        if (static_cast<int>(g.occupants.size()) >= g.capacity) continue;

        auto& gT = registry.get<Transform>(garrisonEnt);
        auto moverView = registry.view<Movement, Transform>(entt::exclude<Dead>);

        for (auto moverEnt : moverView)
        {
            auto& m = registry.get<Movement>(moverEnt);
            auto& t = registry.get<Transform>(moverEnt);

            FixedPoint dx = gT.x - m.targetX;
            FixedPoint dy = gT.y - m.targetY;
            if (Abs(dx) < FixedPoint::FromInt(1) && Abs(dy) < FixedPoint::FromInt(1))
            {
                // Arrived at garrison — snap inside
                t.x = gT.x;
                t.y = gT.y;
                registry.remove<Movement>(moverEnt);
                g.occupants.push_back(moverEnt);
                break;
            }
        }
    }
}

// ── PowerGridSystem ──────────────────────────────────────────

void PowerGridSystem(entt::registry& registry)
{
    int totalOutput = 0;
    int totalDemand = 0;

    auto provView = registry.view<PowerProvider>();
    for (auto ent : provView)
        totalOutput += registry.get<PowerProvider>(ent).output;

    auto consView = registry.view<PowerConsumer>();
    for (auto ent : consView)
        totalDemand += registry.get<PowerConsumer>(ent).demand;

    // If demand > output, consumers operate at reduced efficiency.
    // For now, just flag overload — actual effect applied in Lua.
    (void)totalOutput;
    (void)totalDemand;
}

// ── DestructibleTerrainSystem ────────────────────────────────

void DestructibleTerrainSystem(entt::registry& registry)
{
    auto view = registry.view<DestructibleTerrain>();
    for (auto ent : view)
    {
        auto& dt = registry.get<DestructibleTerrain>(ent);
        if (dt.hp <= 0)
        {
            // Terrain destroyed — replace tile type via ThawGrid.
            // The Lua bridge or map system handles visual transition.
            registry.destroy(ent);
        }
    }
}

} // namespace beigebox
