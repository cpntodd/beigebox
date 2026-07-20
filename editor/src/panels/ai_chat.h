// editor/src/panels/ai_chat.h
// ─────────────────────────────────────────────────────────────
// AI Chat Panel — command-line interface for the MCP tool
// registry. Serves as the vibe-coding console until the full
// MCP JSON-RPC AI integration is built in Phase 5.
//
// Supported commands:
//   /<tool_name> key=value key2=value2 ...
//   /help [tool_name]
//   /clear
// ─────────────────────────────────────────────────────────────
#pragma once

#include "../mcp/tool_registry.h"

#include <string>
#include <vector>

namespace beigebox {

class AIChatPanel
{
public:
    explicit AIChatPanel(ToolRegistry& tools)
        : tools_(&tools) {}

    // Render the chat panel. Call once per frame.
    void Draw();

    // Programmatically append a message to the chat log.
    void AppendMessage(const std::string& sender, const std::string& text);

private:
    struct Message {
        std::string sender;  // "user" or "system"
        std::string text;
    };

    void ExecuteCommand(const std::string& input);

    ToolRegistry*     tools_;
    std::vector<Message> messages_;
    char                 inputBuf_[1024] = {};
    bool                 scrollToBottom_ = true;
};

} // namespace beigebox
