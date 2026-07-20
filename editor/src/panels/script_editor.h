// editor/src/panels/script_editor.h
// ─────────────────────────────────────────────────────────────
// Script Editor — tabbed Lua code editor with syntax checking,
// find/replace, test run, and hot-reload.
//
// Scripts are stored as standalone .lua files in assets/scripts/.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <string>
#include <vector>
#include <functional>

namespace beigebox {

class LuaBridge;
class AIChatPanel;

class ScriptEditor
{
public:
    ScriptEditor(LuaBridge& lua);
    ~ScriptEditor() = default;

    void SetAIChat(AIChatPanel* c) { aiChat_ = c; }
    void SetRootPath(const std::string& p) { rootPath_ = p; }
    void Draw();

    // ── File I/O (public for project-explorer routing) ──────
    void OpenFile(const std::string& path);

private:
    // ── Tab management ───────────────────────────────────────
    struct Tab
    {
        std::string filePath;    // relative path, e.g. "on_tick.lua"
        std::string fullPath;    // absolute path on disk
        std::string content;     // editor buffer
        std::string originalContent; // for dirty detection
        bool        dirty    = false;
        bool        hasError = false;
        std::string errorMsg;
        int         errorLine = -1;
    };

    void CloseTab(int index);
    void SaveTab(int index);
    bool CheckSyntax(const std::string& code, std::string& errorOut, int& errorLine);
    void TestRun(int index);
    void DrawFindReplace();

    // ── Helpers ──────────────────────────────────────────────
    void Log(const std::string& msg);
    std::string ScriptsDir() const;
    void RefreshFileList();

    // ── State ────────────────────────────────────────────────
    LuaBridge*           lua_;
    AIChatPanel*         aiChat_ = nullptr;
    std::string          rootPath_ = ".";

    std::vector<Tab>     tabs_;
    int                  activeTab_ = -1;

    // File list for open dialog
    std::vector<std::string> scriptFiles_;
    bool  showOpenDialog_ = false;

    // Find/replace
    bool  showFindReplace_ = false;
    char  findBuf_[128]    = {};
    char  replaceBuf_[128] = {};
    bool  matchCase_       = false;
    bool  wholeWord_       = false;
};

} // namespace beigebox
