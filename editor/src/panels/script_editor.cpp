// editor/src/panels/script_editor.cpp
// ─────────────────────────────────────────────────────────────
// Script Editor — tabbed Lua editor with syntax checking,
// find/replace, test run, hot-reload.
// ─────────────────────────────────────────────────────────────

#include "script_editor.h"
#include "ai_chat.h"
#include "beigebox/lua/lua_bridge.h"

#include <imgui.h>
#include <SDL2/SDL.h>

#include <sol/sol.hpp>

#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace beigebox {

// ── Constructor ─────────────────────────────────────────────

ScriptEditor::ScriptEditor(LuaBridge& lua)
    : lua_(&lua)
{
}

// ── Helpers ─────────────────────────────────────────────────

void ScriptEditor::Log(const std::string& msg)
{
    if (aiChat_) aiChat_->AppendMessage("system", "[Script] " + msg);
    SDL_Log("[Script] %s", msg.c_str());
}

std::string ScriptEditor::ScriptsDir() const
{
    return rootPath_ + "/assets/scripts";
}

void ScriptEditor::RefreshFileList()
{
    scriptFiles_.clear();
    std::string dir = ScriptsDir();
    mkdir(dir.c_str(), 0755);

    DIR* d = opendir(dir.c_str());
    if (!d) return;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr)
    {
        std::string name(entry->d_name);
        if (name == "." || name == "..") continue;
        if (entry->d_type != DT_REG) continue;

        // Only .lua files
        if (name.size() < 4 || name.substr(name.size() - 4) != ".lua") continue;

        scriptFiles_.push_back(name);
    }
    closedir(d);

    std::sort(scriptFiles_.begin(), scriptFiles_.end());
}

// ── File Operations ─────────────────────────────────────────

void ScriptEditor::OpenFile(const std::string& filename)
{
    // Check if already open
    for (int i = 0; i < static_cast<int>(tabs_.size()); ++i)
    {
        if (tabs_[i].filePath == filename)
        {
            activeTab_ = i;
            return;
        }
    }

    // Absolute path → use as-is; relative → resolve under ScriptsDir
    std::string fullPath;
    if (!filename.empty() && filename[0] == '/')
        fullPath = filename;
    else
        fullPath = ScriptsDir() + "/" + filename;

    Tab tab;
    tab.filePath = filename;
    tab.fullPath = fullPath;

    // Read file
    std::ifstream f(fullPath);
    if (f.good())
    {
        std::stringstream ss;
        ss << f.rdbuf();
        tab.content = ss.str();
        tab.originalContent = tab.content;
    }
    else
    {
        // New file — create minimal stub
        tab.content = "-- " + filename + "\nfunction OnTick(entity_id)\n    -- Your code here\nend\n";
        tab.originalContent = tab.content;
        tab.dirty = true;
    }

    tabs_.push_back(tab);
    activeTab_ = static_cast<int>(tabs_.size()) - 1;

    Log("Opened: " + filename);
}

void ScriptEditor::CloseTab(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return;

    if (tabs_[index].dirty)
    {
        // Auto-save dirty tabs? Or warn? For now, silently discard.
        // Future: confirmation dialog
    }

    tabs_.erase(tabs_.begin() + index);
    if (activeTab_ >= static_cast<int>(tabs_.size()))
        activeTab_ = static_cast<int>(tabs_.size()) - 1;
    if (activeTab_ < 0 && !tabs_.empty())
        activeTab_ = 0;
}

void ScriptEditor::SaveTab(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return;
    auto& tab = tabs_[index];

    // Syntax check
    std::string err;
    int errLine;
    tab.hasError = !CheckSyntax(tab.content, err, errLine);
    tab.errorMsg = err;
    tab.errorLine = errLine;

    if (tab.hasError)
    {
        Log("Syntax error: " + err);
        return; // Don't save broken code
    }

    // Ensure directory exists
    mkdir(ScriptsDir().c_str(), 0755);

    // Write to disk
    std::ofstream f(tab.fullPath);
    if (f.good())
    {
        f << tab.content;
        tab.dirty = false;
        tab.originalContent = tab.content;
        Log("Saved: " + tab.filePath);

        // Hot-reload: if this is an entity script, reload it
        // The LuaBridge handles this via LoadScript
    }
    else
    {
        Log("Error writing: " + tab.fullPath);
    }
}

// ── Syntax Checking ─────────────────────────────────────────

bool ScriptEditor::CheckSyntax(const std::string& code,
                                std::string& errorOut, int& errorLine)
{
    lua_State* L = luaL_newstate();
    if (!L) return false;

    int rc = luaL_loadstring(L, code.c_str());
    bool ok = (rc == LUA_OK);

    if (!ok)
    {
        const char* msg = lua_tostring(L, -1);
        errorOut = msg ? msg : "Unknown syntax error";

        // Parse line number from Lua error: "[string "..."]:line: message"
        errorLine = -1;
        std::string errStr(errorOut);
        auto colon1 = errStr.find(':');
        if (colon1 != std::string::npos)
        {
            auto colon2 = errStr.find(':', colon1 + 1);
            if (colon2 != std::string::npos)
            {
                std::string lineStr = errStr.substr(colon1 + 1, colon2 - colon1 - 1);
                errorLine = atoi(lineStr.c_str());
            }
        }
    }

    lua_close(L);
    return ok;
}

// ── Test Run ────────────────────────────────────────────────

void ScriptEditor::TestRun(int index)
{
    if (index < 0 || index >= static_cast<int>(tabs_.size())) return;
    auto& tab = tabs_[index];

    // Syntax check first
    std::string err;
    int errLine;
    if (!CheckSyntax(tab.content, err, errLine))
    {
        Log("Test Run failed — syntax error: " + err);
        return;
    }

    // Execute in the editor's Lua VM
    try
    {
        lua_->State().safe_script(tab.content);
        Log("Test Run OK: " + tab.filePath);
    }
    catch (const sol::error& e)
    {
        Log(std::string("Test Run error: ") + e.what());

        // Try to extract line number
        tab.hasError = true;
        tab.errorMsg = e.what();
        tab.errorLine = -1;
    }
}

// ── Find/Replace ────────────────────────────────────────────

void ScriptEditor::DrawFindReplace()
{
    if (!showFindReplace_) return;

    ImGui::SetNextWindowSize(ImVec2(400, 120), ImGuiCond_FirstUseEver);
    ImGui::Begin("Find & Replace", &showFindReplace_,
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse);

    ImGui::InputTextWithHint("##find", "Find...", findBuf_, sizeof(findBuf_));
    ImGui::InputTextWithHint("##replace", "Replace with...", replaceBuf_, sizeof(replaceBuf_));

    ImGui::Checkbox("Match case", &matchCase_);
    ImGui::SameLine();
    ImGui::Checkbox("Whole word", &wholeWord_);

    if (activeTab_ >= 0 && activeTab_ < static_cast<int>(tabs_.size()))
    {
        auto& tab = tabs_[activeTab_];

        if (ImGui::Button("Find Next") && findBuf_[0])
        {
            std::string search(findBuf_);
            std::string content = tab.content;
            if (!matchCase_)
            {
                std::transform(search.begin(), search.end(), search.begin(), ::tolower);
                std::transform(content.begin(), content.end(), content.begin(), ::tolower);
            }

            auto pos = content.find(search);
            if (pos != std::string::npos)
            {
                // Count newlines to find line number
                int line = 1;
                for (size_t i = 0; i < pos; ++i)
                    if (tab.content[i] == '\n') line++;
                Log("Found \"" + std::string(findBuf_) + "\" at line " + std::to_string(line));
            }
            else
            {
                Log("Not found: " + std::string(findBuf_));
            }
        }

        ImGui::SameLine();
        if (ImGui::Button("Replace All") && findBuf_[0])
        {
            std::string from(findBuf_);
            std::string to(replaceBuf_);
            std::string& content = tab.content;

            if (!matchCase_)
            {
                // Case-insensitive replace
                std::string lower = content;
                std::string lowerFrom = from;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                std::transform(lowerFrom.begin(), lowerFrom.end(), lowerFrom.begin(), ::tolower);

                size_t pos = 0;
                int count = 0;
                while ((pos = lower.find(lowerFrom, pos)) != std::string::npos)
                {
                    content.replace(pos, from.size(), to);
                    lower.replace(pos, from.size(), to);
                    pos += to.size();
                    count++;
                }
                if (count > 0) { tab.dirty = true; Log("Replaced " + std::to_string(count) + " occurrences."); }
            }
            else
            {
                size_t pos = 0;
                int count = 0;
                while ((pos = content.find(from, pos)) != std::string::npos)
                {
                    content.replace(pos, from.size(), to);
                    pos += to.size();
                    count++;
                }
                if (count > 0) { tab.dirty = true; Log("Replaced " + std::to_string(count) + " occurrences."); }
            }
        }
    }

    ImGui::End();
}

// ═════════════════════════════════════════════════════════════
// Main Draw
// ═════════════════════════════════════════════════════════════

void ScriptEditor::Draw()
{
    ImGui::Begin("Script Editor");

    // ── Toolbar ──────────────────────────────────────────────
    if (ImGui::Button("Open..."))
    {
        RefreshFileList();
        showOpenDialog_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("New"))
    {
        char name[64];
        snprintf(name, sizeof(name), "script_%d.lua", static_cast<int>(tabs_.size()) + 1);
        std::string fullPath = ScriptsDir() + "/" + name;
        // Create empty file
        mkdir(ScriptsDir().c_str(), 0755);
        std::ofstream f(fullPath);
        if (f.good())
        {
            f << "-- " << name << "\nfunction OnTick(entity_id)\n    -- Your code here\nend\n";
            f.close();
            OpenFile(name);
        }
    }
    ImGui::SameLine();

    if (ImGui::Button("Save") && activeTab_ >= 0)
        SaveTab(activeTab_);
    ImGui::SameLine();

    if (ImGui::Button("Test Run") && activeTab_ >= 0)
        TestRun(activeTab_);
    ImGui::SameLine();

    if (ImGui::Button("Find/Replace"))
        showFindReplace_ = !showFindReplace_;
    ImGui::SameLine();

    ImGui::TextDisabled("scripts in assets/scripts/");

    ImGui::Separator();

    // ── Tabs ─────────────────────────────────────────────────
    if (ImGui::BeginTabBar("##scriptTabs"))
    {
        for (int i = 0; i < static_cast<int>(tabs_.size()); ++i)
        {
            auto& tab = tabs_[i];

            std::string label = tab.filePath;
            if (tab.dirty) label = "● " + label;
            if (tab.hasError) label = "⚠ " + label;

            bool open = true;
            if (ImGui::BeginTabItem(label.c_str(), &open))
            {
                activeTab_ = i;

                // ── Error banner ─────────────────────────────
                if (tab.hasError && !tab.errorMsg.empty())
                {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
                        "Error: %s", tab.errorMsg.c_str());
                    ImGui::Separator();
                }

                // ── Status bar ───────────────────────────────
                int lineCount = 1;
                for (char c : tab.content) if (c == '\n') lineCount++;
                ImGui::Text("Lines: %d | %s",
                    lineCount, tab.dirty ? "Modified" : "Saved");

                // ── Line numbers + Editor ────────────────────
                ImGui::BeginChild("##editorArea", ImVec2(0, 0), false);

                // Line number gutter
                float gutterWidth = 40.0f;
                ImGui::BeginChild("##gutter", ImVec2(gutterWidth, 0), false);

                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.4f, 0.5f, 1.0f));
                for (int ln = 1; ln <= lineCount; ++ln)
                {
                    // Highlight error line
                    if (tab.hasError && ln == tab.errorLine)
                        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%4d", ln);
                    else
                        ImGui::Text("%4d", ln);
                }
                ImGui::PopStyleColor();

                ImGui::EndChild();
                ImGui::SameLine();

                // Editor
                ImGui::BeginChild("##editor", ImVec2(0, 0), false);

                // Use InputTextMultiline for editing
                // We manage the buffer ourselves
                ImGuiInputTextFlags flags = ImGuiInputTextFlags_AllowTabInput;

                // Dynamic buffer — resize if needed
                static std::string editBuf;
                if (ImGui::IsWindowAppearing() || editBuf != tab.content)
                {
                    editBuf = tab.content;
                    // Ensure buffer has extra space
                    editBuf.reserve(editBuf.size() + 1024);
                }

                // Render as a large text area
                ImVec2 editorSize = ImGui::GetContentRegionAvail();
                if (ImGui::InputTextMultiline("##code", &editBuf[0],
                        editBuf.capacity(), editorSize, flags))
                {
                    tab.content = editBuf.c_str(); // trim to actual length
                    tab.dirty = (tab.content != tab.originalContent);
                    tab.hasError = false;
                }

                ImGui::EndChild();
                ImGui::EndChild();

                ImGui::EndTabItem();
            }

            if (!open)
                CloseTab(i);
        }

        if (tabs_.empty())
        {
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                "No scripts open.\nUse Open... to load a .lua file, or New to create one.\n\n"
                "Scripts are stored in assets/scripts/ as standalone .lua files.\n"
                "Ctrl+S to save & check syntax | Test Run to execute in editor VM.");
        }

        ImGui::EndTabBar();
    }

    // ── Open File Dialog ─────────────────────────────────────
    if (showOpenDialog_)
    {
        ImGui::OpenPopup("Open Script");
        if (ImGui::BeginPopupModal("Open Script", &showOpenDialog_))
        {
            if (ImGui::Button("Refresh"))
                RefreshFileList();

            ImGui::Separator();
            ImGui::BeginChild("##fileList", ImVec2(0, 200), true);

            for (auto& name : scriptFiles_)
            {
                if (ImGui::Selectable(name.c_str()))
                {
                    OpenFile(name);
                    showOpenDialog_ = false;
                }
            }

            if (scriptFiles_.empty())
            {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                    "No .lua files in assets/scripts/");
            }

            ImGui::EndChild();
            ImGui::Separator();
            if (ImGui::Button("Cancel", ImVec2(80, 0)))
                showOpenDialog_ = false;

            ImGui::EndPopup();
        }
    }

    ImGui::End();

    // Draw find/replace as a separate window
    DrawFindReplace();
}

} // namespace beigebox
