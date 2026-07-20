// tests/test_jsonrpc.cpp
// ─────────────────────────────────────────────────────────────
// Unit tests for the MCP JSON-RPC transport.
// ─────────────────────────────────────────────────────────────

#include <doctest/doctest.h>

// We test the JSON-RPC server in isolation by creating a minimal
// ToolRegistry with a test tool and sending JSON-RPC requests.

#include <entt/entt.hpp>
#include "beigebox/lua/lua_bridge.h"

// Include editor sources for testing
#include "mcp/tool_registry.h"
#include "mcp/json_rpc.h"

#include <nlohmann/json.hpp>

using namespace beigebox;
using json = nlohmann::json;

static entt::registry g_registry3;
static LuaBridge      g_lua3;
static ToolRegistry*  g_tools = nullptr;
static JsonRpcServer* g_jsonRpc = nullptr;

TEST_CASE("JSON-RPC — Setup")
{
    g_lua3.Init(g_registry3);
    g_tools = new ToolRegistry(g_registry3, g_lua3);

    g_tools->RegisterTool("echo",
        "Echoes back the message parameter.",
        "/echo message=<text>",
        [](const ToolRegistry::ParamMap& p) -> std::string {
            auto it = p.find("message");
            return (it != p.end()) ? it->second : "(no message)";
        });

    g_jsonRpc = new JsonRpcServer(*g_tools);
}

TEST_CASE("JSON-RPC — tools/call success")
{
    std::string request = R"({
        "jsonrpc": "2.0",
        "id": 1,
        "method": "tools/call",
        "params": {
            "name": "echo",
            "arguments": {
                "message": "hello world"
            }
        }
    })";

    std::string response = g_jsonRpc->HandleRequest(request);

    auto resp = json::parse(response);
    CHECK(resp["jsonrpc"] == "2.0");
    CHECK(resp["id"] == 1);
    CHECK(resp["result"]["content"][0]["text"] == "hello world");
}

TEST_CASE("JSON-RPC — tools/call unknown tool")
{
    std::string request = R"({
        "jsonrpc": "2.0",
        "id": 2,
        "method": "tools/call",
        "params": {
            "name": "nonexistent_tool",
            "arguments": {}
        }
    })";

    std::string response = g_jsonRpc->HandleRequest(request);

    auto resp = json::parse(response);
    CHECK(resp["jsonrpc"] == "2.0");
    CHECK(resp["id"] == 2);
    CHECK(resp.contains("error"));
    CHECK(resp["error"]["code"] == -32602);
}

TEST_CASE("JSON-RPC — tools/list")
{
    std::string request = R"({
        "jsonrpc": "2.0",
        "id": 3,
        "method": "tools/list"
    })";

    std::string response = g_jsonRpc->HandleRequest(request);

    auto resp = json::parse(response);
    CHECK(resp["result"]["content"][0]["text"].get<std::string>().find("echo") != std::string::npos);
}

TEST_CASE("JSON-RPC — parse error")
{
    std::string response = g_jsonRpc->HandleRequest("not json");

    auto resp = json::parse(response);
    CHECK(resp.contains("error"));
    CHECK(resp["error"]["code"] == -32700);
}

TEST_CASE("JSON-RPC — cleanup")
{
    delete g_jsonRpc;
    delete g_tools;
}
