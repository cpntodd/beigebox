// editor/src/menu_bar.h
// ─────────────────────────────────────────────────────────────
// Main Menu Bar — File, Edit, View, AI, Scripts, Events,
// Assets, World, Help. All items wired to functional callbacks.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <entt/entt.hpp>

#include <string>
#include <functional>
#include <vector>

namespace beigebox {

class ToolRegistry;
class LuaBridge;
class LlmClient;
class AIChatPanel;
class ThawGrid;
class SpriteRegistry;

class MainMenuBar
{
public:
    MainMenuBar(entt::registry& ecs, LuaBridge& lua,
                ToolRegistry& tools, LlmClient& llm);
    void Draw();

    // Set the AI Chat panel reference for logging
    void SetAIChat(AIChatPanel* chat) { aiChat_ = chat; }
    void SetThawGrid(ThawGrid* grid) { thawGrid_ = grid; }
    void SetSpriteRegistry(SpriteRegistry* reg) { spriteReg_ = reg; }
    void SetSelectedEntity(entt::entity e) { selectedEntity_ = e; }
    void SetProjectPath(const std::string& p) { projectPath_ = p; }

    // ── Callbacks ────────────────────────────────────────────
    using VoidCallback = std::function<void()>;
    VoidCallback onQuit;
    VoidCallback onResetLayout;
    VoidCallback onNewProject;

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
    void DrawLuaApiRefDialog();
    void DrawEventManagerDialog();
    void DrawValidateResultDialog();
    void DrawNewScriptDialog();
    void DrawFireEventDialog();
    void DrawPreferencesDialog();

    // ── Project serialization ────────────────────────────────
    void SaveProject(const std::string& path);
    void LoadProject(const std::string& path);

    // ── Helpers ──────────────────────────────────────────────
    void LogToChat(const std::string& msg);
    void FireMapGeneration();

    entt::registry*  ecs_;
    LuaBridge*       lua_;
    ToolRegistry*    tools_;
    LlmClient*       llm_;
    AIChatPanel*     aiChat_    = nullptr;
    SpriteRegistry*  spriteReg_ = nullptr;
    ThawGrid*        thawGrid_  = nullptr;
    entt::entity     selectedEntity_ = entt::null;
    std::string      projectPath_    = "untitled.madproj";

    // Dialog state
    bool showAbout_          = false;
    bool showAIConfig_       = false;
    bool showExport_         = false;
    bool showLuaApiRef_      = false;
    bool showEventManager_   = false;
    bool showValidateResult_ = false;
    bool showNewScript_      = false;
    bool showFireEvent_      = false;
    bool showPreferences_    = false;

    char exportPath_[512]    = "./dist/MAD_Export";
    char apiKeyBuf_[128]     = {};
    char endpointBuf_[256]   = "http://localhost:11434";
    char modelBuf_[64]       = "llama3";
    int  providerIdx_        = 0;

    // Validate results
    std::string validateResultText_;

    // New Script dialog
    int  newScriptEntityId_  = 0;
    int  newScriptEventIdx_  = 0;

    // Fire Event dialog
    int  fireEventEntityId_  = 0;
    int  fireEventEventIdx_  = 1; // default: OnTick

    // Event Manager table data cache
    std::vector<std::string> eventMgrCache_;

    // Preferences
    float editorFontScale_   = 1.0f;
    int   editorThemeIdx_    = 0;

    // Map generation params
    int   mapWidth_          = 32;
    int   mapHeight_         = 32;
    int   mapSeed_           = 42;
    float mapSalvageDensity_ = 0.15f;
    float mapGeothermalFreq_ = 0.05f;
};

} // namespace beigebox

