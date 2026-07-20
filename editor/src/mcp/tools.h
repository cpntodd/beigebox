// editor/src/mcp/tools.h
// ─────────────────────────────────────────────────────────────
// MCP Tool Implementations — ECS manipulation, Lua scripting,
// and world management tools for the AI vibe-coding pipeline.
// ─────────────────────────────────────────────────────────────
#pragma once

#include "tool_registry.h"

namespace beigebox {

// Register all built-in tools on the given registry.
void RegisterAllTools(ToolRegistry& registry);

} // namespace beigebox
