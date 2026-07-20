// editor/src/panels/faction_editor.h
// ─────────────────────────────────────────────────────────────
// Faction Editor + Build/Export Manager — final phase.
//
// Faction Editor:
//   - Per-faction JSON: data/factions/{name}.json
//   - Node graph tech tree visualization with drag-to-arrange
//   - Full properties: names, lore, colors, music, units, tech,
//     diplomacy defaults, init script
//
// Build/Export Manager:
//   - One-click build for Linux target
//   - Asset packaging, version stamp, launch script
//   - Output to dist/ directory
// ─────────────────────────────────────────────────────────────
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <imgui.h>
#include <nlohmann/json.hpp>

namespace beigebox {

class AIChatPanel;

// ── Tech node (for graph visualization) ─────────────────────
struct TechNode
{
    std::string name;
    std::string description;
    int  cost = 100;
    int  researchTime = 30;
    std::vector<std::string> prerequisites;
    std::vector<std::string> unlocks; // unit/building/tech names this unlocks
    // Graph layout (saved, user-editable via drag)
    float graphX = 0, graphY = 0;
};

// ── Faction definition ──────────────────────────────────────
struct FactionDef
{
    std::string name;
    std::string displayName;
    std::string lore;
    std::string musicTheme;
    std::string initScript; // Lua run at game start for this faction

    // Visual identity
    float colorR = 1.0f, colorG = 1.0f, colorB = 1.0f;
    float uiColorR = 0.2f, uiColorG = 0.5f, uiColorB = 0.8f;

    // Starting state
    int    startSalvage = 1000;
    int    startHeat    = 500;
    std::vector<std::string> startingUnits;
    std::string defaultAI; // "Aggressive", "Defensive", "Explorer", "Coward"

    // Buildable
    std::vector<std::string> buildableUnits;
    std::vector<std::string> buildableBuildings;

    // Superweapons
    std::vector<std::string> superweapons;

    // Tech tree
    std::vector<TechNode> techTree;

    // Diplomacy defaults
    std::vector<int> defaultDiplomacy; // per-other-faction: 0=ally, 1=neutral, 2=enemy

    // Majesty-style name pools
    std::vector<std::string> firstNamePool;
    std::vector<std::string> lastNamePool;
    std::vector<std::string> titleProgression; // per-level titles
};

class FactionEditor
{
public:
    FactionEditor();
    ~FactionEditor() = default;

    void SetAIChat(AIChatPanel* c) { aiChat_ = c; }
    void SetRootPath(const std::string& p) { rootPath_ = p; }
    void Draw();

private:
    // ── Tabs ─────────────────────────────────────────────────
    void DrawFactionTab();
    void DrawTechTreeTab();
    void DrawBuildTab();

    // ── Tech tree graph ──────────────────────────────────────
    void DrawTechGraph(FactionDef& faction);
    void DrawTechNodeBox(const TechNode& node, bool selected);
    void DrawTechArrow(float x1, float y1, float x2, float y2);

    // ── Faction I/O ──────────────────────────────────────────
    void NewFaction();
    void LoadFaction(const std::string& name);
    void SaveFaction();
    void RefreshFactionList();
    std::string FactionsDir() const;
    void Log(const std::string& msg);

    // ── Build ────────────────────────────────────────────────
    void DoBuild();
    std::string GenerateLaunchScript() const;

    // ── State ────────────────────────────────────────────────
    AIChatPanel* aiChat_ = nullptr;
    std::string  rootPath_ = ".";

    FactionDef   faction_;
    std::vector<std::string> factionList_;
    int          activeTab_ = 0;
    int          selectedTech_ = -1;
    int          draggingTech_ = -1;

    // Graph navigation
    float graphScrollX_ = 0, graphScrollY_ = 0;
    float graphZoom_ = 1.0f;

    // UI
    char  newFactionName_[64] = {};
    bool  showNewDialog_ = false;
    char  techNameBuf_[64]   = {};
    char  techCostBuf_[16]   = {};
    char  techTimeBuf_[16]   = {};
    char  unitAddBuf_[64]    = {};

    // Build
    char  buildOutputPath_[256] = "./dist/MAD_Export";
    char  buildVersion_[32]     = "0.1.0";
    bool  buildWindows_ = false;
    bool  buildLinux_   = true;
};

} // namespace beigebox
