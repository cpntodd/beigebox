// tests/test_combat_orders.cpp
// ─────────────────────────────────────────────────────────────
// Integration tests for Combat, Orders, and Economy Lua APIs.
// ─────────────────────────────────────────────────────────────

#include <doctest/doctest.h>

#include <entt/entt.hpp>

#include "beigebox/core/fixed_point.h"
#include "beigebox/ecs/components.h"
#include "beigebox/ecs/systems.h"
#include "beigebox/lua/lua_bridge.h"

using namespace beigebox;

static entt::registry g_registry2;
static LuaBridge      g_lua2;

TEST_CASE("Combat — LuaBridge Init")
{
    CHECK(g_lua2.Init(g_registry2));
}

TEST_CASE("Combat — DealDamage reduces health")
{
    auto entity = g_registry2.create();
    g_registry2.emplace<Transform>(entity, FixedPoint::FromInt(0), FixedPoint::FromInt(0));
    g_registry2.emplace<Health>(entity, FixedPoint::FromInt(100), FixedPoint::FromInt(100));
    g_registry2.emplace<Player>(entity, 1);

    bool ok = g_lua2.LoadScript(entity, "OnInit", R"lua(
        function OnInit(entity_id)
            Combat.DealDamage(entity_id, 30 * 256)  -- 30 HP in Q24.8
        end
    )lua");
    CHECK(ok);
    g_lua2.FireEvent(entity, "OnInit");

    auto& h = g_registry2.get<Health>(entity);
    CHECK(h.current.ToInt() == 70);
}

TEST_CASE("Combat — DealDamage kills entity")
{
    auto entity = g_registry2.create();
    g_registry2.emplace<Transform>(entity, FixedPoint::FromInt(0), FixedPoint::FromInt(0));
    g_registry2.emplace<Health>(entity, FixedPoint::FromInt(10), FixedPoint::FromInt(10));
    g_registry2.emplace<Player>(entity, 1);

    bool ok = g_lua2.LoadScript(entity, "OnInit", R"lua(
        function OnInit(entity_id)
            Combat.DealDamage(entity_id, 20 * 256)  -- overkill
        end
    )lua");
    CHECK(ok);
    g_lua2.FireEvent(entity, "OnInit");

    CHECK(g_registry2.all_of<Dead>(entity));
}

TEST_CASE("Combat — IsEnemy")
{
    auto e1 = g_registry2.create();
    g_registry2.emplace<Player>(e1, 1);

    auto e2 = g_registry2.create();
    g_registry2.emplace<Player>(e2, 2);

    auto e3 = g_registry2.create();
    g_registry2.emplace<Player>(e3, 1); // same faction as e1

    uint32_t id1 = static_cast<uint32_t>(entt::to_integral(e1));
    uint32_t id2 = static_cast<uint32_t>(entt::to_integral(e2));
    uint32_t id3 = static_cast<uint32_t>(entt::to_integral(e3));

    bool ok = g_lua2.LoadScript(e1, "OnInit", R"lua(
        function OnInit(entity_id)
            test_enemy = Combat.IsEnemy(entity_id, ENEMY_ID)
            test_ally  = Combat.IsEnemy(entity_id, ALLY_ID)
        end
    )lua");
    // Inject entity IDs into the script
    // (We'll use globals instead for simplicity)
    g_lua2.State()["ENEMY_ID"] = static_cast<int>(id2);
    g_lua2.State()["ALLY_ID"]  = static_cast<int>(id3);
    CHECK(ok);
    g_lua2.FireEvent(e1, "OnInit");

    bool isEnemy = g_lua2.State()["test_enemy"];
    bool isAlly  = g_lua2.State()["test_ally"];
    CHECK(isEnemy == true);
    CHECK(isAlly == false);
}

TEST_CASE("Orders — MoveTo sets Movement component")
{
    auto entity = g_registry2.create();
    g_registry2.emplace<Transform>(entity, FixedPoint::FromInt(2), FixedPoint::FromInt(2));

    bool ok = g_lua2.LoadScript(entity, "OnInit", R"lua(
        function OnInit(entity_id)
            Orders.MoveTo(entity_id, 8 * 256, 8 * 256)
        end
    )lua");
    CHECK(ok);
    g_lua2.FireEvent(entity, "OnInit");

    CHECK(g_registry2.all_of<Movement>(entity));
    auto& m = g_registry2.get<Movement>(entity);
    CHECK(m.targetX.ToInt() == 8);
    CHECK(m.targetY.ToInt() == 8);
}

TEST_CASE("MovementSystem — moves toward target")
{
    auto entity = g_registry2.create();
    g_registry2.emplace<Transform>(entity, FixedPoint::FromInt(0), FixedPoint::FromInt(0));
    g_registry2.emplace<Movement>(entity,
        FixedPoint::FromInt(10), FixedPoint::FromInt(0),
        FixedPoint::FromInt(2)); // 2 tiles per tick

    // Run several ticks
    for (int i = 0; i < 5; ++i)
        MovementSystem(g_registry2);

    auto& t = g_registry2.get<Transform>(entity);
    // Should have moved roughly 5 * 2 = 10 tiles toward target
    // Actually less because speed is per tick (not per second at 30Hz)
    CHECK(t.x.ToInt() >= 5);
    CHECK(t.y.ToInt() == 0);
}

TEST_CASE("Economy — GiveSalvage")
{
    bool ok = g_lua2.LoadScript(entt::entity{0}, "OnInit", R"lua(
        function OnInit(entity_id)
            Economy.GiveSalvage(1, 500 * 256)
            test_salvage = 1
        end
    )lua");
    CHECK(ok);
    g_lua2.FireEvent(entt::entity{0}, "OnInit");

    // Find faction 1 resource entity
    bool found = false;
    auto view = g_registry2.view<Player, FactionResources>();
    for (auto e : view) {
        if (g_registry2.get<Player>(e).factionId == 1) {
            auto& res = g_registry2.get<FactionResources>(e);
            CHECK(res.salvage.ToInt() == 500);
            found = true;
        }
    }
    CHECK(found);
}
