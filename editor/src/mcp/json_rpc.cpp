// editor/src/mcp/json_rpc.cpp
// ─────────────────────────────────────────────────────────────
// MCP JSON-RPC 2.0 Transport implementation.
// ─────────────────────────────────────────────────────────────

#include "json_rpc.h"

#include <nlohmann/json.hpp>

#include <sstream>
#include <stdexcept>

namespace beigebox {

using json = nlohmann::json;

// ── Public API ───────────────────────────────────────────────

std::string JsonRpcServer::HandleRequest(const std::string& jsonRequest)
{
    try
    {
        auto req = json::parse(jsonRequest);

        // Validate JSON-RPC version
        if (!req.contains("jsonrpc") || req["jsonrpc"] != "2.0")
            return ErrorResponse(0, -32600, "Invalid Request: jsonrpc must be '2.0'");

        if (!req.contains("method"))
            return ErrorResponse(0, -32600, "Invalid Request: missing 'method'");

        int id = req.value("id", 0);
        std::string method = req["method"];

        // ── tools/call ───────────────────────────────────────
        if (method == "tools/call")
        {
            if (!req.contains("params") || !req["params"].contains("name"))
                return ErrorResponse(id, -32602, "Invalid params: missing 'name'");

            std::string toolName = req["params"]["name"];
            ToolRegistry::ParamMap params;

            if (req["params"].contains("arguments"))
            {
                for (auto& [key, val] : req["params"]["arguments"].items())
                {
                    if (val.is_string())
                        params[key] = val.get<std::string>();
                    else if (val.is_number_integer())
                        params[key] = std::to_string(val.get<int>());
                    else if (val.is_number_float())
                        params[key] = std::to_string(val.get<double>());
                    else if (val.is_boolean())
                        params[key] = val.get<bool>() ? "true" : "false";
                    else
                        params[key] = val.dump(); // fallback: JSON string
                }
            }

            return DispatchToolCall(id, toolName, params);
        }

        // ── tools/list ───────────────────────────────────────
        if (method == "tools/list")
        {
            return SuccessResponse(id, tools_->Help());
        }

        // ── Unknown method ───────────────────────────────────
        return ErrorResponse(id, -32601, "Method not found: " + method);
    }
    catch (const json::parse_error& e)
    {
        return ErrorResponse(0, -32700, std::string("Parse error: ") + e.what());
    }
    catch (const std::exception& e)
    {
        return ErrorResponse(0, -32603, std::string("Internal error: ") + e.what());
    }
}

// ── Private Helpers ──────────────────────────────────────────

std::string JsonRpcServer::DispatchToolCall(int id, const std::string& toolName,
                                             const ToolRegistry::ParamMap& params)
{
    if (!tools_->HasTool(toolName))
        return ErrorResponse(id, -32602, "Unknown tool: " + toolName);

    if (logger_)
        logger_("MCP: calling tool '" + toolName + "'");

    std::string result = tools_->Execute(toolName, params);
    return SuccessResponse(id, result);
}

std::string JsonRpcServer::SuccessResponse(int id, const std::string& text)
{
    json resp;
    resp["jsonrpc"] = "2.0";
    resp["id"] = id;
    resp["result"]["content"] = json::array({
        {{"type", "text"}, {"text", text}}
    });
    return resp.dump();
}

std::string JsonRpcServer::ErrorResponse(int id, int code, const std::string& message)
{
    json resp;
    resp["jsonrpc"] = "2.0";
    resp["id"] = id;
    resp["error"]["code"] = code;
    resp["error"]["message"] = message;
    return resp.dump();
}

} // namespace beigebox
