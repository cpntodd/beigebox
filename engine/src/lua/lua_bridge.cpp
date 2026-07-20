// engine/src/lua/lua_bridge.cpp
// ─────────────────────────────────────────────────────────────
// Lua Bridge implementation — Sol2 bindings and script execution.
// ─────────────────────────────────────────────────────────────

#include "beigebox/lua/lua_bridge.h"
#include "beigebox/ecs/components.h"
#include "beigebox/core/fixed_point.h"

#include <SDL2/SDL.h>
#include <cstdio>

namespace beigebox {

// ── Lifecycle ────────────────────────────────────────────────

LuaBridge::~LuaBridge()
{
    Shutdown();
}

bool LuaBridge::Init(entt::registry& registry)
{
    registry_ = &registry;

    // ── Open Lua VM ─────────────────────────────────────────
    lua_.open_libraries(
        sol::lib::base,
        sol::lib::math,
        sol::lib::string,
        sol::lib::table
    );
    // NOTE: sol::lib::package, io, os are NOT loaded.
    // This sandboxes Lua — scripts cannot access the filesystem.

    // ── Register C++ API namespaces ─────────────────────────
    RegisterTransformAPI();
    // Future: RegisterCombatAPI();
    // Future: RegisterWorldAPI();

    SDL_Log("LuaBridge: initialized (Lua %s)", LUA_VERSION);
    return true;
}

void LuaBridge::Shutdown()
{
    scripts_.clear();
    lua_.collect_garbage();
    registry_ = nullptr;
}

// ── Script Management ────────────────────────────────────────

bool LuaBridge::LoadScript(entt::entity entity,
                           const std::string& eventName,
                           const std::string& luaCode)
{
    // ── Compile the Lua chunk ────────────────────────────────
    auto result = lua_.safe_script(luaCode, sol::script_pass_on_error);
    if (!result.valid())
    {
        sol::error err = result;
        SDL_Log("LuaBridge: script compile error [entity %u, event '%s']: %s",
                EntityToID(entity), eventName.c_str(), err.what());
        return false;
    }

    // ── Extract the event handler function ──────────────────
    sol::protected_function fn = lua_[eventName];
    if (!fn.valid())
    {
        SDL_Log("LuaBridge: script missing function '%s' [entity %u]",
                eventName.c_str(), EntityToID(entity));
        return false;
    }

    // ── Clear the global name to prevent cross-entity pollution ──
    // The function is now owned by `fn` and stored in `scripts_`.
    lua_[eventName] = sol::nil;

    // ── Store for later invocation ───────────────────────────
    scripts_[EntityToID(entity)][eventName] = fn;
    SDL_Log("LuaBridge: loaded '%s' script for entity %u",
            eventName.c_str(), EntityToID(entity));
    return true;
}

// ── Event Firing ─────────────────────────────────────────────

void LuaBridge::FireEvent(entt::entity entity, const std::string& eventName)
{
    auto entityIt = scripts_.find(EntityToID(entity));
    if (entityIt == scripts_.end())
        return;

    auto eventIt = entityIt->second.find(eventName);
    if (eventIt == entityIt->second.end())
        return;

    // ── Call the Lua function ────────────────────────────────
    // Signature: function(entity_id)  or  function(entity_id, dt)
    sol::protected_function& fn = eventIt->second;
    auto result = fn(EntityToID(entity));
    if (!result.valid())
    {
        sol::error err = result;
        SDL_Log("LuaBridge: runtime error [entity %u, event '%s']: %s",
                EntityToID(entity), eventName.c_str(), err.what());
    }
}

bool LuaBridge::HasScript(entt::entity entity, const std::string& eventName) const
{
    auto entityIt = scripts_.find(EntityToID(entity));
    if (entityIt == scripts_.end())
        return false;
    return entityIt->second.find(eventName) != entityIt->second.end();
}

// ── Transform API Registration ───────────────────────────────
//
// Exposes the following Lua namespace:
//
//   Transform.GetPosition(entity_id) -> x_raw, y_raw
//   Transform.SetPosition(entity_id, x_raw, y_raw)
//   Transform.GetDistance(e1, e2) -> raw_distance
//
// All values are passed as raw int32_t (Q24.8). Lua sees them
// as plain integers. Conversion to/from FixedPoint happens at
// the C++ boundary — Lua never touches floating-point.
// ─────────────────────────────────────────────────────────────

void LuaBridge::RegisterTransformAPI()
{
    auto transform = lua_.create_named_table("Transform");

    // Transform.GetPosition(entity_id) → x_raw, y_raw
    transform.set_function("GetPosition",
        [this](int entityId) -> std::tuple<int, int>
        {
            auto entity = IDFromLua(entityId);
            if (!registry_->valid(entity) || !registry_->all_of<Transform>(entity))
                return {0, 0};

            auto& t = registry_->get<Transform>(entity);
            return {t.x.Raw(), t.y.Raw()};
        });

    // Transform.SetPosition(entity_id, x_raw, y_raw)
    transform.set_function("SetPosition",
        [this](int entityId, int xRaw, int yRaw)
        {
            auto entity = IDFromLua(entityId);
            if (!registry_->valid(entity) || !registry_->all_of<Transform>(entity))
                return;

            auto& t = registry_->get<Transform>(entity);
            t.x = FixedPoint(xRaw);
            t.y = FixedPoint(yRaw);
        });

    // Transform.GetDistance(e1, e2) → raw_distance
    // Uses Manhattan distance for now (cheap, deterministic).
    transform.set_function("GetDistance",
        [this](int e1Id, int e2Id) -> int
        {
            auto e1 = IDFromLua(e1Id);
            auto e2 = IDFromLua(e2Id);

            if (!registry_->valid(e1) || !registry_->all_of<Transform>(e1))
                return -1;
            if (!registry_->valid(e2) || !registry_->all_of<Transform>(e2))
                return -1;

            auto& t1 = registry_->get<Transform>(e1);
            auto& t2 = registry_->get<Transform>(e2);

            FixedPoint dx = Abs(t1.x - t2.x);
            FixedPoint dy = Abs(t1.y - t2.y);
            return (dx + dy).Raw();
        });
}

} // namespace beigebox
