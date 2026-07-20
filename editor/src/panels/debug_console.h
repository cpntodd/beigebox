// editor/src/panels/debug_console.h
// ─────────────────────────────────────────────────────────────
// Debug Console — Lua REPL, slash commands, ECS inspector,
// debug overlays, and log viewer.
//
// Access: dockable editor panel + tilde (~) in-game overlay.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <deque>

#include <entt/entt.hpp>

namespace beigebox {

class LuaBridge;
class AIChatPanel;
class Renderer;

class DebugConsole
{
public:
    DebugConsole(entt::registry& ecs, LuaBridge& lua);
    ~DebugConsole() = default;

    void SetAIChat(AIChatPanel* c) { aiChat_ = c; }
    void SetRenderer(Renderer* r) { renderer_ = r; }
    void Draw();

    // ── Overlay toggles ──────────────────────────────────────
    bool showEntityLabels() const { return showLabels_; }
    bool showPathfinding()  const { return showPathfinding_; }
    bool showHeatmap()      const { return showHeatmap_; }

    // ── Log forwarding ───────────────────────────────────────
    void AddLog(const std::string& msg, int level = 0); // 0=info, 1=warn, 2=error

private:
    // ── Command execution ────────────────────────────────────
    void ExecuteCommand(const std::string& cmd);
    void ExecuteLua(const std::string& code);
    void ExecuteSlashCommand(const std::string& cmd);

    // ── ECS inspection ───────────────────────────────────────
    void InspectEntity(uint32_t id);
    void FindComponent(const std::string& compName);
    void ListAllEntities();

    // ── Slash command handlers ───────────────────────────────
    void CmdSpawn(const std::vector<std::string>& args);
    void CmdGive(const std::vector<std::string>& args);
    void CmdKill(const std::vector<std::string>& args);
    void CmdTp(const std::vector<std::string>& args);
    void CmdHeat(const std::vector<std::string>& args);
    void CmdFreeze(const std::vector<std::string>& args);
    void CmdThaw(const std::vector<std::string>& args);

    // ── UI helpers ───────────────────────────────────────────
    void DrawRepl();
    void DrawInspector();
    void DrawOverlayToggles();
    void DrawLogViewer();
    void Log(const std::string& msg, int level = 0);

    // ── State ────────────────────────────────────────────────
    entt::registry*  ecs_;
    LuaBridge*       lua_;
    AIChatPanel*     aiChat_    = nullptr;
    Renderer*        renderer_  = nullptr;

    // Command history
    std::deque<std::string> history_;
    int   historyIdx_ = -1;
    char  inputBuf_[512] = {};

    // Log
    struct LogEntry { std::string msg; int level; };
    std::vector<LogEntry> logEntries_;
    bool  autoScroll_ = true;

    // Inspector
    uint32_t inspectEntityId_ = 0;
    char  inspectIdBuf_[16] = {};

    // Overlay toggles
    bool  showLabels_      = false;
    bool  showPathfinding_  = false;
    bool  showHeatmap_      = false;

    // Panel state
    bool  visible_ = true;
    int   activeTab_ = 0;
};

} // namespace beigebox
