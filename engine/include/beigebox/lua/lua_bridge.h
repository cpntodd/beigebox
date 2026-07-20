// engine/include/beigebox/lua/lua_bridge.h
// ─────────────────────────────────────────────────────────────
// Lua Bridge — connects the C++ engine (EnTT ECS) to Lua 5.4
// via Sol2. Implements the Event-Action script binding model.
//
// Scripts are stored per-entity, per-event. When the engine
// fires an event on an entity, the corresponding Lua function
// is called (if it exists).
//
// Usage:
//   LuaBridge bridge;
//   bridge.Init(registry);
//   bridge.LoadScript(entity, "OnTick", "print('tick')");
//   bridge.FireEvent(entity, "OnTick");
// ─────────────────────────────────────────────────────────────
#pragma once

#include <sol/sol.hpp>
#include <entt/entt.hpp>

#include <string>
#include <unordered_map>
#include <cstdint>

namespace beigebox {

class LuaBridge
{
public:
    LuaBridge() = default;
    ~LuaBridge();

    // Non-copyable (owns Lua VM)
    LuaBridge(const LuaBridge&) = delete;
    LuaBridge& operator=(const LuaBridge&) = delete;

    // ── Lifecycle ────────────────────────────────────────────
    //
    // Initialize the Lua VM and register all C++ API bindings.
    // Optionally attach a ThawGrid for the World/Thaw API.
    bool Init(entt::registry& registry);

    // Attach (or detach) the ThawGrid for World/Thaw Lua API.
    void SetThawGrid(class ThawGrid* grid) { thawGrid_ = grid; }

    // Destroy the Lua VM.
    void Shutdown();

    // ── Script Management ────────────────────────────────────
    //
    // Load (or replace) a Lua script for a specific entity/event.
    // The script should define a function matching the event name,
    // e.g. `function OnTick(entity_id, dt) ... end`
    //
    // Returns true if the script compiled and executed without error.
    bool LoadScript(entt::entity entity,
                    const std::string& eventName,
                    const std::string& luaCode);

    // ── Event Firing ─────────────────────────────────────────
    //
    // Fire an event on an entity. If the entity has a script
    // for this event, it is called. Otherwise, a no-op.
    //
    // Currently supports the `OnTick(entity_id)` event signature.
    // Additional event types are added via overloads as the API grows.
    void FireEvent(entt::entity entity, const std::string& eventName);

    // ── Accessors ────────────────────────────────────────────
    sol::state&       State()    { return lua_; }
    entt::registry&   Registry() { return *registry_; }

    // Check if an entity has a script loaded for an event
    bool HasScript(entt::entity entity, const std::string& eventName) const;

private:
    // ── API Registration ─────────────────────────────────────
    void RegisterTransformAPI();
    void RegisterCombatAPI();
    void RegisterOrdersAPI();
    void RegisterEconomyAPI();
    void RegisterFactoryAPI();
    void RegisterThawAPI();

    // ── Internal Helpers ─────────────────────────────────────
    static uint32_t EntityToID(entt::entity e) { return static_cast<uint32_t>(entt::to_integral(e)); }
    static entt::entity IDFromLua(int id) { return entt::entity(static_cast<uint32_t>(id)); }

    sol::state       lua_;
    entt::registry*  registry_ = nullptr;
    class ThawGrid*  thawGrid_ = nullptr;

    // Per-entity, per-event Lua functions
    // Outer key: entity ID (uint32_t)
    // Inner key: event name → compiled Lua function
    std::unordered_map<
        uint32_t,
        std::unordered_map<std::string, sol::protected_function>
    > scripts_;
};

} // namespace beigebox
