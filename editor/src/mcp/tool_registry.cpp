// editor/src/mcp/tool_registry.cpp
// ─────────────────────────────────────────────────────────────
// Tool registry implementation.
// ─────────────────────────────────────────────────────────────

#include "tool_registry.h"
#include <sstream>

namespace beigebox {

void ToolRegistry::RegisterTool(const std::string& name,
                                const std::string& description,
                                const std::string& usage,
                                ToolFunc func)
{
    tools_[name] = {description, usage, std::move(func)};
}

std::string ToolRegistry::Execute(const std::string& name, const ParamMap& params) const
{
    auto it = tools_.find(name);
    if (it == tools_.end())
        return "Unknown tool: /" + name + "\nType /help for available tools.";

    return it->second.func(params);
}

std::string ToolRegistry::Help() const
{
    std::ostringstream oss;
    oss << "Available tools:\n\n";
    for (const auto& [name, entry] : tools_)
    {
        oss << "  /" << name << " — " << entry.description << "\n";
    }
    oss << "\nType /help <tool> for detailed usage.";
    return oss.str();
}

std::string ToolRegistry::Help(const std::string& name) const
{
    auto it = tools_.find(name);
    if (it == tools_.end())
        return "Unknown tool: /" + name;

    std::ostringstream oss;
    oss << "/" << name << " — " << it->second.description << "\n\n";
    oss << "Usage: " << it->second.usage << "\n";
    return oss.str();
}

} // namespace beigebox
