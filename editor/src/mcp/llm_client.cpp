// editor/src/mcp/llm_client.cpp
// ─────────────────────────────────────────────────────────────
// LLM Client — HTTP POST to LLM API, parse tool-call responses.
// ─────────────────────────────────────────────────────────────

#include "llm_client.h"
#include "tool_registry.h"

#include <SDL2/SDL.h>

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
    std::string body = BuildRequestBody(userPrompt);
    std::string response = HttpPost(endpoint_, body);

    if (response.empty())
    {
        onResponse("Error: no response from LLM. Is the server running?");
        return;
    }

    ParseResponse(response, onResponse, onToolCall);
}

// ── Request Building ─────────────────────────────────────────

std::string LlmClient::BuildRequestBody(const std::string& userPrompt) const
{
    std::ostringstream json;

    if (provider_ == Provider::Ollama)
    {
        // Ollama API format
        json << "{"
             << "\"model\":\"" << model_ << "\","
             << "\"system\":\"" << systemPrompt_ << "\","
             << "\"prompt\":\"" << userPrompt << "\","
             << "\"stream\":false"
             << "}";
    }
    else if (provider_ == Provider::OpenAI || provider_ == Provider::DeepSeek)
    {
        json << "{"
             << "\"model\":\"" << model_ << "\","
             << "\"messages\":["
             << "{\"role\":\"system\",\"content\":\"" << systemPrompt_ << "\"},"
             << "{\"role\":\"user\",\"content\":\"" << userPrompt << "\"}"
             << "],"
             << "\"stream\":false"
             << "}";
    }
    else // Anthropic
    {
        json << "{"
             << "\"model\":\"" << model_ << "\","
             << "\"max_tokens\":1024,"
             << "\"system\":\"" << systemPrompt_ << "\","
             << "\"messages\":["
             << "{\"role\":\"user\",\"content\":\"" << userPrompt << "\"}"
             << "]"
             << "}";
    }

    return json.str();
}

// ── HTTP POST (minimal, no libcurl dependency) ───────────────

std::string LlmClient::HttpPost(const std::string& url, const std::string& body) const
{
    // Parse host and port from URL
    std::string host = "localhost";
    int port = 11434;
    std::string path = "/api/generate"; // Ollama default

    // Simple URL parse: scheme://host:port/path
    auto schemeEnd = url.find("://");
    bool isHttps = (schemeEnd != std::string::npos && url.substr(0, schemeEnd) == "https");
    size_t hostStart = (schemeEnd != std::string::npos) ? schemeEnd + 3 : 0;
    auto hostEnd = url.find(':', hostStart);
    auto pathStart = url.find('/', hostStart);

    if (isHttps && port == 11434) port = 443; // HTTPS default

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

// ── Response Parsing ─────────────────────────────────────────

void LlmClient::ParseResponse(const std::string& json,
                               ResponseCallback onResponse,
                               ToolCallCallback onToolCall)
{
    // Simple extraction: look for "response" or "content" field
    // in the JSON. This is a minimal parser — a real implementation
    // would use nlohmann/json for proper parsing.

    // Try Ollama format: "response":"..."
    std::regex ollamaResp("\"response\"\\s*:\\s*\"([^\"]+)\"");
    std::smatch match;
    if (std::regex_search(json, match, ollamaResp) && match.size() > 1)
    {
        std::string text = match[1].str();

        // Unescape common escape sequences
        auto unescape = [](std::string& s) {
            size_t pos = 0;
            while ((pos = s.find("\\n", pos)) != std::string::npos) { s.replace(pos, 2, "\n"); pos++; }
            pos = 0;
            while ((pos = s.find("\\\"", pos)) != std::string::npos) { s.replace(pos, 2, "\""); pos++; }
        };
        unescape(text);

        // Check for TOOL: commands in the response
        std::regex toolRx("TOOL:\\s*/?(\\w+)\\s+(.*)");
        std::istringstream iss(text);
        std::string line;
        std::string displayText;

        while (std::getline(iss, line))
        {
            std::smatch toolMatch;
            if (std::regex_search(line, toolMatch, toolRx))
            {
                std::string toolName = toolMatch[1].str();
                std::string toolArgs = toolMatch[2].str();
                onToolCall(toolName, toolArgs);

                // Replace TOOL: line with executed indicator
                displayText += "> Executed: /" + toolName + " " + toolArgs + "\n";
            }
            else
            {
                displayText += line + "\n";
            }
        }

        onResponse(displayText);
        return;
    }

    // Try OpenAI format: "content":"..."
    std::regex openaiResp("\"content\"\\s*:\\s*\"([^\"]+)\"");
    if (std::regex_search(json, match, openaiResp) && match.size() > 1)
    {
        onResponse(match[1].str());
        return;
    }

    // Fallback: return raw (truncated)
    onResponse(json.substr(0, 500) + (json.size() > 500 ? "..." : ""));
}

} // namespace beigebox
