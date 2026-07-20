// tests/test_lua_bridge.cpp
// ─────────────────────────────────────────────────────────────
// Integration test for the Lua Bridge + ECS pipeline.
//
// Verifies:
//   1. LuaBridge initializes successfully
//   2. Transform API is accessible from Lua
//   3. Lua scripts can read/write ECS component data
//   4. Script errors are handled gracefully
//   5. Entity isolation (scripts don't cross-talk)
// ─────────────────────────────────────────────────────────────

#include <doctest/doctest.h>

#include <entt/entt.hpp>

#include "beigebox/core/fixed_point.h"
#include "beigebox/ecs/components.h"
#include "beigebox/lua/lua_bridge.h"

using namespace beigebox;

// Shared registry for all test cases in this file
static entt::registry g_registry;
static LuaBridge g_lua;

TEST_CASE("LuaBridge — Init")
{
    CHECK(g_lua.Init(g_registry));
}

TEST_CASE("LuaBridge — Transform.GetPosition")
{
    auto entity = g_registry.create();
    g_registry.emplace<Transform>(entity,
        FixedPoint::FromInt(7),
        FixedPoint::FromInt(3));

    // Run Lua code that reads the position
    bool ok = g_lua.LoadScript(entity, "OnInit", R"lua(
        function OnInit(entity_id)
            local x, y = Transform.GetPosition(entity_id)
            -- Store values in global for C++ to read back
            test_x = x
            test_y = y
        end
    )lua");
    CHECK(ok);

    g_lua.FireEvent(entity, "OnInit");

    // Read back the values that Lua stored in globals
    int x = g_lua.State()["test_x"];
    int y = g_lua.State()["test_y"];
    CHECK(x == FixedPoint::FromInt(7).Raw());  // 7 * 256 = 1792
    CHECK(y == FixedPoint::FromInt(3).Raw());  // 3 * 256 = 768
}

TEST_CASE("LuaBridge — Transform.SetPosition")
{
    auto entity = g_registry.create();
    g_registry.emplace<Transform>(entity,
        FixedPoint::FromInt(0),
        FixedPoint::FromInt(0));

    // Lua script that sets a new position
    bool ok = g_lua.LoadScript(entity, "OnInit", R"lua(
        function OnInit(entity_id)
            Transform.SetPosition(entity_id, 10 * 256, 20 * 256)
        end
    )lua");
    CHECK(ok);

    g_lua.FireEvent(entity, "OnInit");

    auto& t = g_registry.get<Transform>(entity);
    CHECK(t.x.ToInt() == 10);
    CHECK(t.y.ToInt() == 20);
}

TEST_CASE("LuaBridge — GetDistance")
{
    // Create two fresh entities with known positions
    auto e1 = g_registry.create();
    g_registry.emplace<Transform>(e1, FixedPoint::FromInt(0), FixedPoint::FromInt(0));
    uint32_t e1_id = static_cast<uint32_t>(entt::to_integral(e1));

    auto e2 = g_registry.create();
    g_registry.emplace<Transform>(e2, FixedPoint::FromInt(3), FixedPoint::FromInt(4));
    uint32_t e2_id = static_cast<uint32_t>(entt::to_integral(e2));

    // Manhattan distance between (0,0) and (3,4) = 7 tiles = 7*256 raw
    std::string script = R"lua(
        function OnInit(entity_id)
            test_dist = Transform.GetDistance(entity_id, XXX)
        end
    )lua";
    // Inject the target entity ID into the Lua script
    script.replace(script.find("XXX"), 3, std::to_string(e2_id));

    bool ok = g_lua.LoadScript(e1, "OnInit", script);
    CHECK(ok);

    g_lua.FireEvent(e1, "OnInit");

    int dist = g_lua.State()["test_dist"];
    CHECK(dist == FixedPoint::FromInt(7).Raw());
}

TEST_CASE("LuaBridge — Script compile error")
{
    auto entity = g_registry.create();
    g_registry.emplace<Transform>(entity, FixedPoint::FromInt(0), FixedPoint::FromInt(0));

    // This Lua has a syntax error
    bool ok = g_lua.LoadScript(entity, "OnInit", R"lua(
        function OnInit(entity_id)
            Transform.SetPosition(entity_id, 1,  -- missing argument
        end
    )lua");
    CHECK(!ok);  // Should fail
}

TEST_CASE("LuaBridge — Missing event handler")
{
    auto entity = g_registry.create();
    g_registry.emplace<Transform>(entity, FixedPoint::FromInt(0), FixedPoint::FromInt(0));

    // Script defines OnTick but we're looking for OnInit
    bool ok = g_lua.LoadScript(entity, "OnInit", R"lua(
        function OnTick(entity_id)
            -- wrong event name
        end
    )lua");
    CHECK(!ok);  // Should fail — OnInit function not found
}

TEST_CASE("LuaBridge — No script for entity")
{
    auto entity = g_registry.create();
    g_registry.emplace<Transform>(entity, FixedPoint::FromInt(0), FixedPoint::FromInt(0));

    // Fire event on entity that has no script — should not crash
    CHECK(!g_lua.HasScript(entity, "OnTick"));
    g_lua.FireEvent(entity, "OnTick");  // no-op
}

TEST_CASE("LuaBridge — Entity isolation")
{
    auto e1 = g_registry.create();
    g_registry.emplace<Transform>(e1, FixedPoint::FromInt(5), FixedPoint::FromInt(5));

    auto e2 = g_registry.create();
    g_registry.emplace<Transform>(e2, FixedPoint::FromInt(0), FixedPoint::FromInt(0));

    // e1 has a script, e2 does not
    bool ok = g_lua.LoadScript(e1, "OnTick", R"lua(
        function OnTick(entity_id)
            test_isolated = 42
        end
    )lua");
    CHECK(ok);

    // Fire on e2 — should NOT set test_isolated
    g_lua.FireEvent(e2, "OnTick");
    auto val = g_lua.State()["test_isolated"];
    CHECK(!val.valid());  // should be nil

    // Fire on e1 — SHOULD set test_isolated
    g_lua.FireEvent(e1, "OnTick");
    int val2 = g_lua.State()["test_isolated"];
    CHECK(val2 == 42);
}
