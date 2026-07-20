// editor/src/mcp/llm_client.h
// ─────────────────────────────────────────────────────────────
// LLM Client — connects the AI Chat panel to LLM APIs.
//
// Supports BYOK (Bring Your Own Key): OpenAI, Anthropic, Ollama.
// Sends the tool list + user prompt, parses tool-call responses.
//
// Transport: raw HTTP POST (no external HTTP library — uses
// platform sockets for minimal dependencies).
// ─────────────────────────────────────────────────────────────
#pragma once

#include <string>
#include <functional>

namespace beigebox {

class ToolRegistry;

class LlmClient
{
public:
    using ResponseCallback = std::function<void(const std::string& text)>;
    using ToolCallCallback = std::function<void(const std::string& toolName,
                                                 const std::string& args)>;

    // ── Configuration ────────────────────────────────────────
    enum class Provider { OpenAI, Anthropic, Ollama, DeepSeek };

    void SetProvider(Provider p)     { provider_ = p; }
    void SetApiKey(const std::string& key) { apiKey_ = key; }
    void SetEndpoint(const std::string& url) { endpoint_ = url; }
    void SetModel(const std::string& model)  { model_ = model; }
    void SetSystemPrompt(const std::string& prompt) { systemPrompt_ = prompt; }
    void SetThinkingEnabled(bool enabled) { thinkingEnabled_ = enabled; }
    bool IsThinkingEnabled() const { return thinkingEnabled_; }

    // ── Conversation History ─────────────────────────────────
    // DeepSeek is stateless — we must send full history each request.
    // Clear between independent conversations for fresh context.
    void ClearHistory() { conversation_.clear(); }
    size_t HistorySize() const { return conversation_.size(); }

    // ── Send ─────────────────────────────────────────────────
    // Sends a prompt to the LLM. Response (streamed or complete)
    // comes back via the callback. Tool calls are parsed from
    // the response and dispatched via toolCallback.
    //
    // This is a blocking call. Run on a separate thread or
    // call during idle time.
    void Send(const std::string& userPrompt,
              ResponseCallback onResponse,
              ToolCallCallback onToolCall);

    // Build the system prompt from the tool registry.
    void BuildSystemPrompt(ToolRegistry& tools);

private:
    std::string BuildRequestBody(const std::string& userPrompt) const;
    std::string HttpPost(const std::string& url, const std::string& body) const;
    void ParseResponse(const std::string& json, ResponseCallback onResponse,
                       ToolCallCallback onToolCall);

    Provider    provider_ = Provider::Ollama;
    std::string apiKey_;
    std::string endpoint_ = "http://localhost:11434";
    std::string model_ = "llama3";
    std::string systemPrompt_;
    bool        thinkingEnabled_ = false;

    // Conversation history for multi-turn (DeepSeek is stateless)
    // Stores {role, content} pairs — system prompt sent separately
    using Conversation = std::vector<std::pair<std::string, std::string>>;
    Conversation conversation_;
};

} // namespace beigebox
