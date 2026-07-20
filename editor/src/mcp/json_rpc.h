// editor/src/mcp/json_rpc.h
// ─────────────────────────────────────────────────────────────
// MCP JSON-RPC 2.0 Transport
//
// Reads JSON-RPC requests from stdin, dispatches to the
// ToolRegistry, and writes JSON-RPC responses to stdout.
//
// This implements the Model Context Protocol (MCP) transport
// layer, allowing an external AI (Claude, GPT, etc.) to call
// engine tools via standard input/output.
//
// The JSON-RPC format follows the MCP specification:
//   → {"jsonrpc":"2.0","id":1,"method":"tools/call",
//      "params":{"name":"query_entities","arguments":{"faction":"1"}}}
//   ← {"jsonrpc":"2.0","id":1,"result":{"content":[...]}}
// ─────────────────────────────────────────────────────────────
#pragma once

#include "tool_registry.h"

#include <string>
#include <functional>

namespace beigebox {

class JsonRpcServer
{
public:
    using LogFunc = std::function<void(const std::string&)>;

    explicit JsonRpcServer(ToolRegistry& tools)
        : tools_(&tools) {}

    // ── Lifecycle ────────────────────────────────────────────
    //
    // Start the server. In embedded mode (editor), call Pump()
    // each frame to process pending requests.
    //
    // In standalone mode, call Run() to block and process stdin
    // until EOF (for external AI integration).

    // Process a single JSON-RPC request string.
    // Returns the JSON-RPC response string.
    std::string HandleRequest(const std::string& jsonRequest);

    // Set a logger for diagnostic output.
    void SetLogger(LogFunc logger) { logger_ = std::move(logger); }

private:
    // Parse a JSON-RPC request and dispatch to the tool registry.
    std::string DispatchToolCall(int id, const std::string& toolName,
                                 const ToolRegistry::ParamMap& params);

    // Build a JSON-RPC success response.
    static std::string SuccessResponse(int id, const std::string& text);

    // Build a JSON-RPC error response.
    static std::string ErrorResponse(int id, int code, const std::string& message);

    ToolRegistry* tools_;
    LogFunc       logger_;
};

} // namespace beigebox
