// editor/src/panels/level_editor.h
// ─────────────────────────────────────────────────────────────
// Level Editor — mission/scenario designer.
//
// Directory bundle format: scenarios/{name}/
//   scenario.json, map.ogm.json, players.json, triggers.json,
//   objectives.json, diplomacy.json, init.lua
//
// Features:
//   - New Mission wizard + detail tabs
//   - Full ECA trigger system (24+ types)
//   - Game-integrated objectives with progress tracking
//   - Diplomacy matrix (N×N, fixed teams, dynamic changes)
// ─────────────────────────────────────────────────────────────
#pragma once

#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>

namespace beigebox {

class AIChatPanel;

// ── Player Config ───────────────────────────────────────────
struct PlayerConfig
{
    int    factionId   = 0;
    std::string factionName;
    int    team        = 0;
    int    startX      = 0;
    int    startY      = 0;
    int    startSalvage = 1000;
    int    startHeat    = 500;
    std::string aiPersonality = "Aggressive"; // Aggressive/Defensive/Explorer/Coward
    bool   isHuman     = false;
    float  colorR = 1.0f, colorG = 1.0f, colorB = 1.0f;
};

// ── Trigger Types ───────────────────────────────────────────
enum class TriggerEvent
{
    MapStart, TimerExpired, UnitKilled, UnitBuilt, UnitEntersRegion,
    ResearchComplete, ResourceDepleted, SuperweaponFired,
    DiplomacyChanged, AllEnemiesDead, UnitCountReached, BuildingDestroyed,
    HeroLevelUp, MissionTimerTick, PlayerDefeated
};

enum class TriggerCondition
{
    Always, HasResource, UnitCount, IsAlive, IsResearched,
    TimerCompare, DiplomacyIs, UnitInRegion, RandomChance,
    VariableEquals, VariableGreater, VariableLess
};

enum class TriggerAction
{
    SpawnUnit, Victory, Defeat, DisplayMessage, GiveResource,
    SetObjective, PlaySound, ChangeDiplomacy, SetTimer,
    TeleportUnit, RemoveUnit, ResearchTech, FireSuperweapon,
    SetVariable, ModifyVariable, CameraMove, DialogBox,
    WeatherChange, SpawnWave, EnableTrigger, DisableTrigger
};

// ── Trigger Definition ──────────────────────────────────────
struct TriggerDef
{
    int  id = 0;
    std::string name;
    bool enabled = true;
    bool repeatable = false;
    int  repeatInterval = 0; // ticks, 0 = once

    // Event
    TriggerEvent event = TriggerEvent::MapStart;
    nlohmann::json eventParams; // e.g. {"timer": 300, "unitType": "Driller"}

    // Conditions (ALL must be true)
    std::vector<std::pair<TriggerCondition, nlohmann::json>> conditions;

    // Actions (executed in order when triggered)
    std::vector<std::pair<TriggerAction, nlohmann::json>> actions;
};

// ── Objective ───────────────────────────────────────────────
struct ObjectiveDef
{
    int  id = 0;
    std::string name;
    std::string description;
    bool isPrimary = true;
    bool completed = false;
    bool failed    = false;
    bool hidden     = false;  // revealed when condition met
    std::string revealCondition;
    std::string completeCondition;
    std::string onCompleteScript; // Lua callback
};

// ── Diplomacy ───────────────────────────────────────────────
enum class DiplomacyState { Ally, Neutral, Enemy };

// ── Scenario Manifest ───────────────────────────────────────
struct ScenarioManifest
{
    std::string name;
    std::string description;
    std::string mapFile;         // relative to scenario dir
    int    maxPlayers = 2;
    std::string initScript;     // Lua script run at mission start
};

class LevelEditor
{
public:
    LevelEditor();
    ~LevelEditor() = default;

    void SetAIChat(AIChatPanel* c) { aiChat_ = c; }
    void SetRootPath(const std::string& p) { rootPath_ = p; }
    void Draw();

private:
    // ── Tabs / Wizard ────────────────────────────────────────
    void DrawWizard();
    void DrawScenarioSettings();
    void DrawPlayerEditor();
    void DrawTriggerEditor();
    void DrawObjectivesEditor();
    void DrawDiplomacyEditor();

    // ── Trigger helpers ──────────────────────────────────────
    void DrawTriggerRow(TriggerDef& trigger, int index);
    static const char* EventName(TriggerEvent e);
    static const char* ConditionName(TriggerCondition c);
    static const char* ActionName(TriggerAction a);
    void DrawEventParams(TriggerDef& trigger);
    void DrawConditionParams(TriggerCondition cond, nlohmann::json& params);
    void DrawActionParams(TriggerAction action, nlohmann::json& params);

    // ── I/O ──────────────────────────────────────────────────
    void NewScenario();
    void LoadScenario(const std::string& dir);
    void SaveScenario();
    void RefreshScenarioList();
    std::string ScenarioDir() const;
    void Log(const std::string& msg);

    // ── State ────────────────────────────────────────────────
    AIChatPanel* aiChat_ = nullptr;
    std::string  rootPath_ = ".";

    // Scenario data
    ScenarioManifest          manifest_;
    std::vector<PlayerConfig> players_;
    std::vector<TriggerDef>   triggers_;
    std::vector<ObjectiveDef> objectives_;
    std::vector<std::vector<DiplomacyState>> diplomacyMatrix_;

    // UI state
    int   activeTab_ = 0;
    int   wizardStep_ = 0;
    bool  showWizard_ = true;
    char  newScenarioName_[64] = {};
    char  scenarioFilter_[64]  = {};
    std::vector<std::string> scenarioList_;
    std::string currentScenarioPath_;
    bool  useFixedTeams_ = false;
    int   numTeams_ = 2;

    // Trigger editing
    int   selectedTrigger_ = -1;
    int   triggerIdCounter_ = 1;
    int   selectedObj_ = -1;
    int   objIdCounter_ = 1;

    // Diplomacy
    int   diplomacySize_ = 2;
};

} // namespace beigebox
