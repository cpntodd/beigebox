// editor/src/panels/ai_chat.h
// ─────────────────────────────────────────────────────────────
// AI Chat Panel — natural language + command interface.
//
// Natural language: routed to LLM for vibe-coding.
// Commands: /tool_name key=value for direct MCP tool access.
// ─────────────────────────────────────────────────────────────
#pragma once

#include "../mcp/tool_registry.h"

#include <string>
#include <vector>

namespace beigebox {

class LlmClient;

class AIChatPanel
{
public:
    explicit AIChatPanel(ToolRegistry& tools)
        : tools_(&tools) {}

    void SetLlmClient(LlmClient* llm) { llm_ = llm; }

    void Draw();
    void AppendMessage(const std::string& sender, const std::string& text);

private:
    struct Message {
        std::string sender;
        std::string text;
    };

    void ExecuteCommand(const std::string& input);
    void SendToLlm(const std::string& prompt);
    void ExecuteToolCalls(const std::string& response);

    ToolRegistry*     tools_;
    LlmClient*        llm_ = nullptr;
    std::vector<Message> messages_;
    char                 inputBuf_[1024] = {};
    bool                 scrollToBottom_ = true;
};

} // namespace beigebox
