// editor/src/mcp/llm_client.cpp
// ─────────────────────────────────────────────────────────────
// LLM Client — HTTP POST to LLM API, parse tool-call responses.
// ─────────────────────────────────────────────────────────────

#include "llm_client.h"
#include "tool_registry.h"

#include <SDL2/SDL.h>
#include <nlohmann/json.hpp>

#include <cstdio>
#include <cstring>
#include <sstream>
#include <regex>

#ifdef _WIN32
  #include <winsock2.h>
  #pragma comment(lib, "ws2_32.lib")
#else
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  #include <netdb.h>
  #define closesocket close
#endif

namespace beigebox {

using json = nlohmann::json;

void LlmClient::BuildSystemPrompt(ToolRegistry& tools)
{
    std::ostringstream oss;
    oss << "You are an AI assistant for M.A.D. Editor, a custom RTS game engine.\n";
    oss << "You can control the game engine by calling tools using this format:\n";
    oss << "  TOOL: /tool_name key=value key2=value2\n\n";
    oss << tools.Help();
    oss << "\n\nWhen the user asks you to do something in the game, respond with ";
    oss << "TOOL: commands followed by a brief explanation.\n";
    oss << "Example: User says 'create a TARG driller'\n";
    oss << "You respond: TOOL: /create_entity faction=2 name=TARG_Driller\n";
    oss << "Then explain what you did.\n";
    systemPrompt_ = oss.str();
}

void LlmClient::Send(const std::string& userPrompt,
                     ResponseCallback onResponse,
                     ToolCallCallback onToolCall)
{
    // Add user message to conversation history
    conversation_.push_back({"user", userPrompt});

    std::string body = BuildRequestBody(userPrompt);
    std::string response = HttpPost(endpoint_, body);

    if (response.empty())
    {
        onResponse("Error: no response from LLM. Check API key, endpoint, and network.");
        return;
    }

    ParseResponse(response, onResponse, onToolCall);
}

// ── Request Building (nlohmann::json — proper escaping) ──────

std::string LlmClient::BuildRequestBody(const std::string& userPrompt) const
{
    json body;
    body["model"] = model_;
    body["stream"] = false;

    // Build messages array with conversation history (multi-turn)
    json messages = json::array();

    // System prompt always first (identical across calls → KV cache hits)
    if (!systemPrompt_.empty())
    {
        messages.push_back({
            {"role", "system"},
            {"content", systemPrompt_}
        });
    }

    // Append conversation history (excludes current user message)
    for (size_t i = 0; i + 1 < conversation_.size(); ++i)
    {
        messages.push_back({
            {"role", conversation_[i].first},
            {"content", conversation_[i].second}
        });
    }

    // Current user message
    messages.push_back({
        {"role", "user"},
        {"content", userPrompt}
    });

    if (provider_ == Provider::Anthropic)
    {
        body["system"] = systemPrompt_;
        body["max_tokens"] = 1024;
        json anthropicMessages = json::array();
        for (size_t i = 0; i + 1 < conversation_.size(); ++i)
            anthropicMessages.push_back({{"role", conversation_[i].first}, {"content", conversation_[i].second}});
        anthropicMessages.push_back({{"role", "user"}, {"content", userPrompt}});
        body["messages"] = anthropicMessages;
    }
    else if (provider_ == Provider::Ollama)
    {
        body["messages"] = messages;
    }
    else // OpenAI or DeepSeek
    {
        body["messages"] = messages;

        if (provider_ == Provider::DeepSeek)
        {
            body["thinking"] = {{"type", thinkingEnabled_ ? "enabled" : "disabled"}};
            if (thinkingEnabled_)
                body["reasoning_effort"] = "high";
        }
    }

    return body.dump();
}

// ── HTTP POST — raw sockets for HTTP, curl for HTTPS ─────────

std::string LlmClient::HttpPost(const std::string& url, const std::string& body) const
{
    // Detect scheme
    auto schemeEnd = url.find("://");
    bool isHttps = (schemeEnd != std::string::npos && url.substr(0, schemeEnd) == "https");

    // ── HTTPS: use curl via popen (temp file avoids shell escaping) ──
    if (isHttps)
    {
        // Write body to temp file to avoid shell injection/escaping issues
        std::string tmpPath = "/tmp/beigebox_llm_req.json";
        FILE* tmpf = fopen(tmpPath.c_str(), "w");
        if (!tmpf) return "Error: cannot create temp file for LLM request.";
        fwrite(body.c_str(), 1, body.size(), tmpf);
        fclose(tmpf);

        std::string cmd = "curl -s -X POST \"" + url + "\"";
        cmd += " -H \"Content-Type: application/json\"";
        if (!apiKey_.empty())
            cmd += " -H \"Authorization: Bearer " + apiKey_ + "\"";
        cmd += " -d @" + tmpPath + " 2>/dev/null";

        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) { remove(tmpPath.c_str()); return "Error: curl not available."; }

        std::string response;
        char buf[4096];
        while (fgets(buf, sizeof(buf), pipe))
            response += buf;
        int rc = pclose(pipe);
        remove(tmpPath.c_str());

        if (rc != 0 || response.empty())
            return "Error: no response from LLM. Check API key, endpoint, and network.";

        return response;
    }

    // ── HTTP: raw sockets (for localhost Ollama) ─────────────
    std::string host = "localhost";
    int port = 11434;
    std::string path = "/api/generate";

    size_t hostStart = (schemeEnd != std::string::npos) ? schemeEnd + 3 : 0;
    auto hostEnd = url.find(':', hostStart);
    auto pathStart = url.find('/', hostStart);

    if (hostEnd != std::string::npos && (pathStart == std::string::npos || hostEnd < pathStart))
    {
        host = url.substr(hostStart, hostEnd - hostStart);
        port = std::stoi(url.substr(hostEnd + 1, pathStart - hostEnd - 1));
    }
    else if (pathStart != std::string::npos)
    {
        host = url.substr(hostStart, pathStart - hostStart);
    }
    else
    {
        host = url.substr(hostStart);
    }

    if (pathStart != std::string::npos)
        path = url.substr(pathStart);

    // ── Resolve host ────────────────────────────────────────
    struct hostent* he = gethostbyname(host.c_str());
    if (!he) return "";

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return "";

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0)
    {
        closesocket(sock);
        return "";
    }

    // ── Build HTTP request ──────────────────────────────────
    std::ostringstream req;
    req << "POST " << path << " HTTP/1.1\r\n"
        << "Host: " << host << "\r\n"
        << "Content-Type: application/json\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Connection: close\r\n";

    if (!apiKey_.empty())
        req << "Authorization: Bearer " << apiKey_ << "\r\n";

    req << "\r\n" << body;

    std::string reqStr = req.str();
    send(sock, reqStr.c_str(), reqStr.size(), 0);

    // ── Read response ───────────────────────────────────────
    std::string response;
    char buf[4096];
    int n;
    while ((n = recv(sock, buf, sizeof(buf) - 1, 0)) > 0)
    {
        buf[n] = '\0';
        response += buf;
    }

    closesocket(sock);

    // Extract body from HTTP response (after \r\n\r\n)
    auto bodyStart = response.find("\r\n\r\n");
    if (bodyStart != std::string::npos)
        return response.substr(bodyStart + 4);

    return response;
}

// ── Response Parsing (nlohmann::json — robust) ───────────────

void LlmClient::ParseResponse(const std::string& rawJson,
                               ResponseCallback onResponse,
                               ToolCallCallback onToolCall)
{
    try
    {
        json resp = json::parse(rawJson);

        std::string content;
        std::string reasoning;

        // ── OpenAI / DeepSeek format ─────────────────────────
        if (resp.contains("choices") && resp["choices"].is_array() && !resp["choices"].empty())
        {
            auto& choice = resp["choices"][0];
            auto& message = choice["message"];

            // Extract content
            if (message.contains("content") && !message["content"].is_null())
                content = message["content"].get<std::string>();

            // Extract reasoning_content (DeepSeek thinking mode)
            if (message.contains("reasoning_content") && !message["reasoning_content"].is_null())
                reasoning = message["reasoning_content"].get<std::string>();

            // Handle tool_calls (DeepSeek function calling)
            if (message.contains("tool_calls") && message["tool_calls"].is_array())
            {
                for (auto& tc : message["tool_calls"])
                {
                    std::string toolName = tc["function"]["name"].get<std::string>();
                    std::string toolArgs = tc["function"]["arguments"].get<std::string>();
                    onToolCall(toolName, toolArgs);
                }
            }

            // If no content but has reasoning, use reasoning as display
            if (content.empty() && !reasoning.empty())
                content = "[thinking] " + reasoning.substr(0, 200) + "...";
        }
        // ── Ollama format ────────────────────────────────────
        else if (resp.contains("response"))
        {
            content = resp["response"].get<std::string>();
        }
        else if (resp.contains("message"))
        {
            content = resp["message"]["content"].get<std::string>();
        }
        // ── Fallback ─────────────────────────────────────────
        else
        {
            content = rawJson.substr(0, 500);
            if (rawJson.size() > 500) content += "...";
        }

        // Unescape newlines for display
        for (size_t pos = 0; (pos = content.find("\\n", pos)) != std::string::npos; pos++)
            content.replace(pos, 2, "\n");

        if (!content.empty())
            onResponse(content);
        else
            onResponse("(empty response from LLM)");
    }
    catch (const std::exception& e)
    {
        // Not valid JSON — return raw truncated
        std::string fallback = rawJson.substr(0, 500);
        if (rawJson.size() > 500) fallback += "...";
        onResponse(fallback);
    }
}

// ── DeepSeek Utility APIs ────────────────────────────────────

std::string LlmClient::FetchModels()
{
    // Build the base URL from the configured endpoint
    // e.g. https://api.deepseek.com/chat/completions → https://api.deepseek.com/models
    std::string baseUrl = endpoint_;
    auto pos = baseUrl.find("/chat/completions");
    if (pos != std::string::npos)
        baseUrl = baseUrl.substr(0, pos);
    std::string url = baseUrl + "/models";

    std::string cmd = "curl -s -X GET \"" + url + "\"";
    cmd += " -H \"Accept: application/json\"";
    if (!apiKey_.empty())
        cmd += " -H \"Authorization: Bearer " + apiKey_ + "\"";
    cmd += " 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    std::string response;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe))
        response += buf;
    pclose(pipe);

    // Parse and extract model IDs
    try
    {
        json j = json::parse(response);
        if (j.contains("data") && j["data"].is_array())
        {
            json result = json::array();
            for (auto& m : j["data"])
                if (m.contains("id"))
                    result.push_back(m["id"].get<std::string>());
            return result.dump();
        }
    }
    catch (...) {}

    return response; // return raw if parsing fails
}

std::string LlmClient::FetchBalance()
{
    std::string baseUrl = endpoint_;
    auto pos = baseUrl.find("/chat/completions");
    if (pos != std::string::npos)
        baseUrl = baseUrl.substr(0, pos);
    std::string url = baseUrl + "/user/balance";

    std::string cmd = "curl -s -X GET \"" + url + "\"";
    cmd += " -H \"Accept: application/json\"";
    if (!apiKey_.empty())
        cmd += " -H \"Authorization: Bearer " + apiKey_ + "\"";
    cmd += " 2>/dev/null";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    std::string response;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe))
        response += buf;
    pclose(pipe);

    return response;
}

} // namespace beigebox
