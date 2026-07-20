// editor/src/panels/ai_chat.cpp
// ─────────────────────────────────────────────────────────────
// AI Chat Panel — command-line MCP tool console.
// ─────────────────────────────────────────────────────────────

#include "ai_chat.h"

#include <imgui.h>

#include <sstream>
#include <cstring>

namespace beigebox {

void AIChatPanel::Draw()
{
    ImGui::Begin("AI Chat");

    // ── Chat Log ─────────────────────────────────────────────
    float footerHeight = ImGui::GetFrameHeightWithSpacing() + 8.0f;
    ImGui::BeginChild("##chatlog", ImVec2(0, -footerHeight), true);

    for (const auto& msg : messages_)
    {
        if (msg.sender == "system")
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.9f, 0.5f, 1.0f));
            ImGui::TextWrapped("%s", msg.text.c_str());
            ImGui::PopStyleColor();
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.7f, 0.3f, 1.0f));
            ImGui::TextWrapped("> %s", msg.text.c_str());
            ImGui::PopStyleColor();
        }
    }

    if (scrollToBottom_)
        ImGui::SetScrollHereY(1.0f);
    scrollToBottom_ = false;

    ImGui::EndChild();

    // ── Input Field ──────────────────────────────────────────
    ImGui::PushItemWidth(-1);
    bool submit = ImGui::InputText("##chatinput", inputBuf_, sizeof(inputBuf_),
                                   ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();

    // Auto-focus the input field
    if (ImGui::IsWindowAppearing())
        ImGui::SetKeyboardFocusHere();

    if (submit && inputBuf_[0] != '\0')
    {
        std::string input(inputBuf_);
        inputBuf_[0] = '\0';
        scrollToBottom_ = true;

        // Echo user input
        messages_.push_back({"user", input});

        // Execute command
        ExecuteCommand(input);

        // Re-focus input
        ImGui::SetKeyboardFocusHere();
    }

    ImGui::End();
}

void AIChatPanel::AppendMessage(const std::string& sender, const std::string& text)
{
    messages_.push_back({sender, text});
    scrollToBottom_ = true;
}

// ── Command Parsing ──────────────────────────────────────────

void AIChatPanel::ExecuteCommand(const std::string& input)
{
    // ── Parse: /tool_name key=value key2=value2 ... ──────────
    if (input.empty() || input[0] != '/')
    {
        AppendMessage("system", "Commands start with '/'. Type /help for available tools.");
        return;
    }

    std::istringstream iss(input.substr(1)); // skip leading '/'
    std::string toolName;
    iss >> toolName;

    // ── Special commands ─────────────────────────────────────
    if (toolName == "help")
    {
        std::string sub;
        iss >> sub;
        if (sub.empty())
            AppendMessage("system", tools_->Help());
        else
            AppendMessage("system", tools_->Help(sub));
        return;
    }

    if (toolName == "clear")
    {
        messages_.clear();
        return;
    }

    if (!tools_->HasTool(toolName))
    {
        AppendMessage("system", "Unknown command: /" + toolName + "\nType /help for available tools.");
        return;
    }

    // ── Parse parameters ─────────────────────────────────────
    ToolRegistry::ParamMap params;
    std::string token;
    while (iss >> token)
    {
        auto eq = token.find('=');
        if (eq != std::string::npos)
        {
            std::string key = token.substr(0, eq);
            std::string val = token.substr(eq + 1);
            // Strip quotes if present
            if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
                val = val.substr(1, val.size() - 2);
            params[key] = val;
        }
        else
        {
            // Bare token — treat as positional or flag
            params[token] = "true";
        }
    }

    // ── Execute ──────────────────────────────────────────────
    std::string result = tools_->Execute(toolName, params);
    AppendMessage("system", result);
}

} // namespace beigebox
