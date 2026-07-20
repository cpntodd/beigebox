// editor/src/mcp/tool_registry.h
// ─────────────────────────────────────────────────────────────
// MCP Tool Registry — maps tool names to executable functions.
//
// Each tool receives a string→string parameter map and returns
// a result string (displayed in the AI chat panel).
//
// This is the internal tool dispatch layer. The external MCP
// JSON-RPC transport wraps this registry in Phase 5.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <entt/entt.hpp>

#include "beigebox/lua/lua_bridge.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

namespace beigebox {

class ToolRegistry
{
public:
    using ParamMap = std::unordered_map<std::string, std::string>;
    using ToolFunc = std::function<std::string(const ParamMap&)>;

    // ── Lifecycle ────────────────────────────────────────────
    explicit ToolRegistry(entt::registry& ecs, LuaBridge& lua)
        : registry_(&ecs), lua_(&lua) {}

    // ── Tool Management ──────────────────────────────────────
    void RegisterTool(const std::string& name,
                      const std::string& description,
                      const std::string& usage,
                      ToolFunc func);

    // Execute a tool by name. Returns the result string,
    // or an error message if the tool is not found.
    std::string Execute(const std::string& name, const ParamMap& params) const;

    // List all registered tool names and their help text.
    std::string Help() const;

    // Look up a single tool's help.
    std::string Help(const std::string& name) const;

    // Check if a tool exists.
    bool HasTool(const std::string& name) const { return tools_.count(name) > 0; }

    // Access the ECS and Lua from tool implementations.
    entt::registry& Registry() { return *registry_; }
    LuaBridge&      Lua()      { return *lua_; }

private:
    struct ToolEntry {
        std::string description;
        std::string usage;
        ToolFunc    func;
    };

    entt::registry* registry_;
    LuaBridge*      lua_;
    std::unordered_map<std::string, ToolEntry> tools_;
};

} // namespace beigebox
