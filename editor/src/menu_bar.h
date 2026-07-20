// editor/src/menu_bar.h
// ─────────────────────────────────────────────────────────────
// Main Menu Bar — File, Edit, View, AI, Scripts, Events,
// Assets, World, Help. All items wired to functional callbacks.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <entt/entt.hpp>

#include "settings.h"
#include "git_manager.h"

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
class UndoManager;

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
    void SetUndoManager(UndoManager* um) { undoManager_ = um; }
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
    void DrawNewProjectDialog();
    void DrawGitDialog();

    // ── Project serialization ────────────────────────────────
    void SaveProject(const std::string& path);
    void LoadProject(const std::string& path);

    // ── Helpers ──────────────────────────────────────────────
    void LogToChat(const std::string& msg);
    void FireMapGeneration();
    void BrowseFolder(char* buf, size_t bufSize);
    void ApplySettings();
    void EnsureStringCapacities();

    entt::registry*  ecs_;
    LuaBridge*       lua_;
    ToolRegistry*    tools_;
    LlmClient*       llm_;
    AIChatPanel*     aiChat_    = nullptr;
    SpriteRegistry*  spriteReg_ = nullptr;
    ThawGrid*        thawGrid_  = nullptr;
    UndoManager*     undoManager_ = nullptr;
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
    bool showNewProject_     = false;
    bool showPreferences_    = false;
    bool showGit_            = false;

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

    // ── Persistent Settings ──────────────────────────────────
    Settings settings_;
    GitManager git_;
    bool settingsLoaded_ = false;

    // New Project
    char  newProjectName_[128] = "Untitled";
    char  newProjectPath_[256] = "./projects/Untitled";
};

} // namespace beigebox

