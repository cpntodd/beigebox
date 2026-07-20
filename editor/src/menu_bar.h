// editor/src/menu_bar.h
// ─────────────────────────────────────────────────────────────
// Main Menu Bar — File, Edit, View, AI, Scripts, Events,
// Assets, World, Help. Standard editor/IDE menu structure.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <entt/entt.hpp>

#include <string>
#include <functional>

namespace beigebox {

class ToolRegistry;
class LuaBridge;
class LlmClient;

class MainMenuBar
{
public:
    MainMenuBar(entt::registry& ecs, LuaBridge& lua,
                ToolRegistry& tools, LlmClient& llm)
        : ecs_(&ecs), lua_(&lua), tools_(&tools), llm_(&llm) {}

    // Call once per frame inside the dockspace (before EndMainMenuBar).
    void Draw();

    // ── Callbacks ────────────────────────────────────────────
    // Set by the editor app to respond to menu actions.
    using VoidCallback = std::function<void()>;
    VoidCallback onExportGame;
    VoidCallback onGenerateMap;
    VoidCallback onQuit;

private:
    void DrawFileMenu();
    void DrawEditMenu();
    void DrawViewMenu();
    void DrawAiMenu();
    void DrawScriptsMenu();
    void DrawEventsMenu();
    void DrawAssetsMenu();
    void DrawWorldMenu();
    void DrawHelpMenu();

    // ── Modal dialogs ────────────────────────────────────────
    void DrawAboutDialog();
    void DrawAIConfigDialog();
    void DrawExportDialog();

    entt::registry*  ecs_;
    LuaBridge*       lua_;
    ToolRegistry*    tools_;
    LlmClient*       llm_;

    // Dialog state
    bool showAbout_        = false;
    bool showAIConfig_     = false;
    bool showExport_       = false;
    char exportPath_[512]  = "./dist/MAD_Export";
    char apiKeyBuf_[128]   = {};
    char endpointBuf_[256] = "http://localhost:11434";
    char modelBuf_[64]     = "llama3";
    int  providerIdx_      = 0;
};

} // namespace beigebox
