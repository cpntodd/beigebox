// editor/src/mcp/tools.cpp
// ─────────────────────────────────────────────────────────────
// Built-in MCP tool implementations.
//
// Each tool function receives a parameter map and returns a
// human-readable result string. Tools have full access to the
// ECS registry and Lua bridge via the ToolRegistry.
// ─────────────────────────────────────────────────────────────

#include "tools.h"

#include "beigebox/ecs/components.h"
#include "beigebox/core/fixed_point.h"

#include <sstream>
#include <cstdio>
#include <cstdlib>

namespace beigebox {

// ── Helpers ──────────────────────────────────────────────────

static int ParseInt(const std::string& s, int fallback = 0)
{
    if (s.empty()) return fallback;
    return std::atoi(s.c_str());
}

static std::string ParamStr(const ToolRegistry::ParamMap& p, const std::string& key,
                            const std::string& fallback = "")
{
    auto it = p.find(key);
    return (it != p.end()) ? it->second : fallback;
}

// ═════════════════════════════════════════════════════════════
// Tool: query_entities
// Returns a list of entity IDs matching optional filters.
// ═════════════════════════════════════════════════════════════

static std::string QueryEntities(const ToolRegistry::ParamMap& params, ToolRegistry& reg)
{
    std::string faction = ParamStr(params, "faction");
    std::string component = ParamStr(params, "component");

    int factionFilter = faction.empty() ? -1 : ParseInt(faction, -1);
    std::ostringstream oss;
    int count = 0;

    auto view = reg.Registry().view<entt::entity>();
    for (auto entity : view)
    {
        uint32_t id = static_cast<uint32_t>(entt::to_integral(entity));

        // ── Apply filters ────────────────────────────────────
        if (factionFilter >= 0)
        {
            auto* player = reg.Registry().try_get<Player>(entity);
            if (!player || player->factionId != factionFilter)
                continue;
        }

        // List entity with its components
        oss << "  entity " << id << ":";

        if (auto* t = reg.Registry().try_get<Transform>(entity))
            oss << " pos=(" << t->x.ToInt() << "," << t->y.ToInt() << ")";

        if (auto* h = reg.Registry().try_get<Health>(entity))
            oss << " hp=" << h->current.ToInt() << "/" << h->max.ToInt();

        if (auto* p = reg.Registry().try_get<Player>(entity))
            oss << " faction=" << p->factionId;

        // Check for scripts
        bool hasScripts = false;
        for (auto eventName : {"OnInit", "OnTick", "OnDeath"})
        {
            if (reg.Lua().HasScript(entity, eventName))
            {
                if (!hasScripts) { oss << " scripts={"; hasScripts = true; }
                else oss << ", ";
                oss << eventName;
            }
        }
        if (hasScripts) oss << "}";

        oss << "\n";
        ++count;
    }

    if (count == 0)
        oss << "  (no entities match)\n";
    else
        oss << "\n" << count << " entities found.\n";

    return oss.str();
}

// ═════════════════════════════════════════════════════════════
// Tool: create_entity
// Spawns a new blank entity, optionally assigning faction + name.
// ═════════════════════════════════════════════════════════════

static std::string CreateEntity(const ToolRegistry::ParamMap& params, ToolRegistry& reg)
{
    int faction = ParseInt(ParamStr(params, "faction"), 0);
    std::string name = ParamStr(params, "name", "Unnamed");

    auto entity = reg.Registry().create();
    uint32_t id = static_cast<uint32_t>(entt::to_integral(entity));

    // Default components
    reg.Registry().emplace<Transform>(entity, FixedPoint::FromInt(0), FixedPoint::FromInt(0));
    reg.Registry().emplace<Health>(entity, FixedPoint::FromInt(100), FixedPoint::FromInt(100));
    if (faction > 0)
        reg.Registry().emplace<Player>(entity, faction);

    std::ostringstream oss;
    oss << "Created entity " << id << " (\"" << name << "\")";
    if (faction > 0) oss << " faction=" << faction;
    oss << "\n  Components: Transform, Health" << (faction > 0 ? ", Player" : "") << "\n";
    return oss.str();
}

// ═════════════════════════════════════════════════════════════
// Tool: set_component
// Modifies a component property on an entity.
// Syntax: /set_component entity=5 component=Transform x=3 y=7
// ═════════════════════════════════════════════════════════════

static std::string SetComponent(const ToolRegistry::ParamMap& params, ToolRegistry& reg)
{
    int entityId = ParseInt(ParamStr(params, "entity"), -1);
    if (entityId < 0)
        return "Error: 'entity' parameter required.\n";

    auto entity = entt::entity(static_cast<uint32_t>(entityId));
    if (!reg.Registry().valid(entity))
        return "Error: entity " + std::to_string(entityId) + " does not exist.\n";

    std::string comp = ParamStr(params, "component");
    if (comp.empty())
        return "Error: 'component' parameter required (Transform, Health, Player).\n";

    std::ostringstream oss;
    oss << "Updated entity " << entityId << ":\n";

    if (comp == "Transform")
    {
        auto& t = reg.Registry().get_or_emplace<Transform>(entity);
        if (params.count("x")) t.x = FixedPoint::FromInt(ParseInt(ParamStr(params, "x")));
        if (params.count("y")) t.y = FixedPoint::FromInt(ParseInt(ParamStr(params, "y")));
        oss << "  Transform → (" << t.x.ToInt() << ", " << t.y.ToInt() << ")\n";
    }
    else if (comp == "Health")
    {
        auto& h = reg.Registry().get_or_emplace<Health>(entity,
            FixedPoint::FromInt(100), FixedPoint::FromInt(100));
        if (params.count("current")) h.current = FixedPoint::FromInt(ParseInt(ParamStr(params, "current")));
        if (params.count("max"))     h.max     = FixedPoint::FromInt(ParseInt(ParamStr(params, "max")));
        oss << "  Health → " << h.current.ToInt() << "/" << h.max.ToInt() << "\n";
    }
    else if (comp == "Player")
    {
        auto& p = reg.Registry().get_or_emplace<Player>(entity);
        if (params.count("faction")) p.factionId = ParseInt(ParamStr(params, "faction"));
        oss << "  Player → faction=" << p.factionId << "\n";
    }
    else
    {
        oss << "  Unknown component: " << comp << "\n";
    }

    return oss.str();
}

// ═════════════════════════════════════════════════════════════
// Tool: get_script
// Reads the Lua source for an entity's event handler.
// ═════════════════════════════════════════════════════════════

static std::string GetScript(const ToolRegistry::ParamMap& params, ToolRegistry& reg)
{
    int entityId = ParseInt(ParamStr(params, "entity"), -1);
    std::string eventName = ParamStr(params, "event", "OnTick");

    if (entityId < 0)
        return "Error: 'entity' parameter required.\n";

    auto entity = entt::entity(static_cast<uint32_t>(entityId));
    if (!reg.Registry().valid(entity))
        return "Error: entity " + std::to_string(entityId) + " does not exist.\n";

    if (!reg.Lua().HasScript(entity, eventName))
        return "Entity " + std::to_string(entityId) + " has no '" + eventName + "' script.\n";

    std::ostringstream oss;
    oss << "Entity " << entityId << " '" << eventName << "' script is loaded.\n";
    oss << "(Lua source retrieval available in Phase 5 — MCP JSON-RPC transport)\n";
    return oss.str();
}

// ═════════════════════════════════════════════════════════════
// Tool: write_script
// Writes (or overwrites) a Lua script for an entity's event.
// ═════════════════════════════════════════════════════════════

static std::string WriteScript(const ToolRegistry::ParamMap& params, ToolRegistry& reg)
{
    int entityId = ParseInt(ParamStr(params, "entity"), -1);
    std::string eventName = ParamStr(params, "event", "OnTick");
    std::string luaCode = ParamStr(params, "code");

    if (entityId < 0)
        return "Error: 'entity' parameter required.\n";
    if (luaCode.empty())
        return "Error: 'code' parameter required.\n";

    auto entity = entt::entity(static_cast<uint32_t>(entityId));
    if (!reg.Registry().valid(entity))
        return "Error: entity " + std::to_string(entityId) + " does not exist.\n";

    bool ok = reg.Lua().LoadScript(entity, eventName, luaCode);
    if (ok)
        return "Script '" + eventName + "' written for entity " + std::to_string(entityId) + ".\n";
    else
        return "Error: script compilation failed. Check the Lua console for details.\n";
}

// ═════════════════════════════════════════════════════════════
// Tool: fire_event
// Manually fires a named event on an entity (for testing).
// ═════════════════════════════════════════════════════════════

static std::string FireEvent(const ToolRegistry::ParamMap& params, ToolRegistry& reg)
{
    int entityId = ParseInt(ParamStr(params, "entity"), -1);
    std::string eventName = ParamStr(params, "event", "OnTick");

    if (entityId < 0)
        return "Error: 'entity' parameter required.\n";

    auto entity = entt::entity(static_cast<uint32_t>(entityId));
    if (!reg.Registry().valid(entity))
        return "Error: entity " + std::to_string(entityId) + " does not exist.\n";

    if (!reg.Lua().HasScript(entity, eventName))
        return "Entity " + std::to_string(entityId) + " has no '" + eventName + "' script.\n";

    reg.Lua().FireEvent(entity, eventName);
    return "Fired '" + eventName + "' on entity " + std::to_string(entityId) + ".\n";
}

// ═════════════════════════════════════════════════════════════
// Register All Tools
// ═════════════════════════════════════════════════════════════

void RegisterAllTools(ToolRegistry& registry)
{
    registry.RegisterTool("query_entities",
        "List entities matching optional filters.",
        "/query_entities [faction=<id>] [component=<name>]",
        [&](const auto& p) { return QueryEntities(p, registry); });

    registry.RegisterTool("create_entity",
        "Spawn a new entity with default components.",
        "/create_entity [faction=<id>] [name=<string>]",
        [&](const auto& p) { return CreateEntity(p, registry); });

    registry.RegisterTool("set_component",
        "Set component properties on an entity.",
        "/set_component entity=<id> component=<name> [key=value ...]",
        [&](const auto& p) { return SetComponent(p, registry); });

    registry.RegisterTool("get_script",
        "Check if an entity has a script for an event.",
        "/get_script entity=<id> [event=OnTick]",
        [&](const auto& p) { return GetScript(p, registry); });

    registry.RegisterTool("write_script",
        "Write a Lua script for an entity's event handler.",
        "/write_script entity=<id> event=<name> code=<lua_source>",
        [&](const auto& p) { return WriteScript(p, registry); });

    registry.RegisterTool("fire_event",
        "Manually fire an event on an entity (for testing).",
        "/fire_event entity=<id> [event=OnTick]",
        [&](const auto& p) { return FireEvent(p, registry); });
}

} // namespace beigebox
