// editor/src/panels/level_editor.cpp
// ─────────────────────────────────────────────────────────────
// Level Editor — mission/scenario designer implementation.
// ─────────────────────────────────────────────────────────────

#include "level_editor.h"
#include "ai_chat.h"

#include <imgui.h>
#include <SDL2/SDL.h>
#include <nlohmann/json.hpp>

#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>

namespace beigebox {

using json = nlohmann::json;

// ═════════════════════════════════════════════════════════════
// Helpers
// ═════════════════════════════════════════════════════════════

LevelEditor::LevelEditor()
{
    // Default: 2 players, neutral diplomacy
    players_.resize(2);
    players_[0].isHuman = true;
    players_[0].factionId = 1;
    players_[0].factionName = "Player";
    players_[1].factionId = 2;
    players_[1].factionName = "Enemy";
    players_[1].aiPersonality = "Aggressive";

    diplomacyMatrix_ = {{DiplomacyState::Ally, DiplomacyState::Enemy},
                        {DiplomacyState::Enemy, DiplomacyState::Ally}};
    diplomacySize_ = 2;
}

void LevelEditor::Log(const std::string& msg)
{
    if (aiChat_) aiChat_->AppendMessage("system", "[Level] " + msg);
    SDL_Log("[Level] %s", msg.c_str());
}

std::string LevelEditor::ScenarioDir() const
{
    return rootPath_ + "/scenarios";
}

// ── Trigger name tables ─────────────────────────────────────

const char* LevelEditor::EventName(TriggerEvent e)
{
    switch (e) {
        case TriggerEvent::MapStart: return "Map Start";
        case TriggerEvent::TimerExpired: return "Timer Expired";
        case TriggerEvent::UnitKilled: return "Unit Killed";
        case TriggerEvent::UnitBuilt: return "Unit Built";
        case TriggerEvent::UnitEntersRegion: return "Unit Enters Region";
        case TriggerEvent::ResearchComplete: return "Research Complete";
        case TriggerEvent::ResourceDepleted: return "Resource Depleted";
        case TriggerEvent::SuperweaponFired: return "Superweapon Fired";
        case TriggerEvent::DiplomacyChanged: return "Diplomacy Changed";
        case TriggerEvent::AllEnemiesDead: return "All Enemies Dead";
        case TriggerEvent::UnitCountReached: return "Unit Count Reached";
        case TriggerEvent::BuildingDestroyed: return "Building Destroyed";
        case TriggerEvent::HeroLevelUp: return "Hero Level Up";
        case TriggerEvent::MissionTimerTick: return "Mission Timer Tick";
        case TriggerEvent::PlayerDefeated: return "Player Defeated";
        default: return "???";
    }
}

const char* LevelEditor::ConditionName(TriggerCondition c)
{
    switch (c) {
        case TriggerCondition::Always: return "Always";
        case TriggerCondition::HasResource: return "Has Resource";
        case TriggerCondition::UnitCount: return "Unit Count";
        case TriggerCondition::IsAlive: return "Is Alive";
        case TriggerCondition::IsResearched: return "Is Researched";
        case TriggerCondition::TimerCompare: return "Timer Compare";
        case TriggerCondition::DiplomacyIs: return "Diplomacy Is";
        case TriggerCondition::UnitInRegion: return "Unit In Region";
        case TriggerCondition::RandomChance: return "Random Chance";
        case TriggerCondition::VariableEquals: return "Variable ==";
        case TriggerCondition::VariableGreater: return "Variable >";
        case TriggerCondition::VariableLess: return "Variable <";
        default: return "???";
    }
}

const char* LevelEditor::ActionName(TriggerAction a)
{
    switch (a) {
        case TriggerAction::SpawnUnit: return "Spawn Unit";
        case TriggerAction::Victory: return "Victory";
        case TriggerAction::Defeat: return "Defeat";
        case TriggerAction::DisplayMessage: return "Display Message";
        case TriggerAction::GiveResource: return "Give Resource";
        case TriggerAction::SetObjective: return "Set Objective";
        case TriggerAction::PlaySound: return "Play Sound";
        case TriggerAction::ChangeDiplomacy: return "Change Diplomacy";
        case TriggerAction::SetTimer: return "Set Timer";
        case TriggerAction::TeleportUnit: return "Teleport Unit";
        case TriggerAction::RemoveUnit: return "Remove Unit";
        case TriggerAction::ResearchTech: return "Research Tech";
        case TriggerAction::FireSuperweapon: return "Fire Superweapon";
        case TriggerAction::SetVariable: return "Set Variable";
        case TriggerAction::ModifyVariable: return "Modify Variable";
        case TriggerAction::CameraMove: return "Camera Move";
        case TriggerAction::DialogBox: return "Dialog Box";
        case TriggerAction::WeatherChange: return "Weather Change";
        case TriggerAction::SpawnWave: return "Spawn Wave";
        case TriggerAction::EnableTrigger: return "Enable Trigger";
        case TriggerAction::DisableTrigger: return "Disable Trigger";
        default: return "???";
    }
}

// ═════════════════════════════════════════════════════════════
// Main Draw
// ═════════════════════════════════════════════════════════════

void LevelEditor::Draw()
{
    ImGui::Begin("Level Editor");

    if (showWizard_)
    {
        DrawWizard();
    }
    else
    {
        if (ImGui::BeginTabBar("##levelTabs"))
        {
            if (ImGui::BeginTabItem("Scenario")) { DrawScenarioSettings(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Players"))   { DrawPlayerEditor(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Triggers"))  { DrawTriggerEditor(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Objectives")){ DrawObjectivesEditor(); ImGui::EndTabItem(); }
            if (ImGui::BeginTabItem("Diplomacy")) { DrawDiplomacyEditor(); ImGui::EndTabItem(); }
            ImGui::EndTabBar();
        }
    }

    ImGui::End();
}

// ── Wizard ──────────────────────────────────────────────────

void LevelEditor::DrawWizard()
{
    ImGui::Text("New Mission Wizard");
    ImGui::Separator();

    const char* steps[] = {"Name & Map", "Players", "Diplomacy", "Ready"};
    ImGui::Text("Step %d/4: %s", wizardStep_ + 1, steps[wizardStep_]);

    ImGui::Spacing();

    switch (wizardStep_)
    {
        case 0:
            ImGui::InputText("Mission Name", newScenarioName_, sizeof(newScenarioName_));
            ImGui::InputText("Description", &manifest_.description[0], manifest_.description.size() + 1);
            if (manifest_.description.size() < 128) manifest_.description.resize(128);
            ImGui::InputText("Map File", &manifest_.mapFile[0], manifest_.mapFile.size() + 1);
            if (manifest_.mapFile.size() < 128) manifest_.mapFile.resize(128);
            ImGui::InputInt("Max Players", &manifest_.maxPlayers);
            if (manifest_.maxPlayers < 2) manifest_.maxPlayers = 2;
            if (manifest_.maxPlayers > 8) manifest_.maxPlayers = 8;
            break;

        case 1:
            if (static_cast<int>(players_.size()) != manifest_.maxPlayers)
                players_.resize(manifest_.maxPlayers);
            for (int i = 0; i < static_cast<int>(players_.size()); ++i)
            {
                auto& p = players_[i];
                ImGui::PushID(i);
                ImGui::Text("Player %d", i + 1);
                ImGui::Checkbox("Human", &p.isHuman);
                ImGui::InputText("Name", &p.factionName[0], p.factionName.size() + 1);
                if (p.factionName.size() < 64) p.factionName.resize(64);
                if (!p.isHuman)
                {
                    const char* pers[] = {"Aggressive", "Defensive", "Explorer", "Coward"};
                    int persIdx = 0;
                    for (int j = 0; j < 4; ++j)
                        if (p.aiPersonality == pers[j]) { persIdx = j; break; }
                    ImGui::Combo("AI", &persIdx, pers, 4);
                    p.aiPersonality = pers[persIdx];
                }
                ImGui::InputInt("Start Salvage", &p.startSalvage);
                ImGui::InputInt("Start Heat", &p.startHeat);
                ImGui::Separator();
                ImGui::PopID();
            }
            break;

        case 2:
            ImGui::Checkbox("Use Fixed Teams", &useFixedTeams_);
            if (useFixedTeams_)
            {
                ImGui::InputInt("Number of Teams", &numTeams_);
                if (numTeams_ < 2) numTeams_ = 2;
                if (numTeams_ > manifest_.maxPlayers) numTeams_ = manifest_.maxPlayers;
                for (int i = 0; i < static_cast<int>(players_.size()); ++i)
                {
                    ImGui::PushID(i);
                    ImGui::Text("Player %d Team:", i + 1);
                    ImGui::SameLine();
                    ImGui::InputInt("##team", &players_[i].team);
                    if (players_[i].team < 0) players_[i].team = 0;
                    if (players_[i].team >= numTeams_) players_[i].team = numTeams_ - 1;
                    ImGui::PopID();
                }
            }
            break;

        case 3:
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "Ready to create!");
            ImGui::BulletText("Mission: %s", newScenarioName_);
            ImGui::BulletText("Players: %d", manifest_.maxPlayers);
            ImGui::BulletText("Map: %s", manifest_.mapFile.c_str());
            break;
    }

    ImGui::Spacing();
    if (wizardStep_ > 0)
    {
        if (ImGui::Button("Back")) wizardStep_--;
        ImGui::SameLine();
    }
    if (wizardStep_ < 3)
    {
        if (ImGui::Button("Next")) wizardStep_++;
    }
    else
    {
        if (ImGui::Button("Create Mission"))
        {
            manifest_.name = newScenarioName_;
            NewScenario();
            showWizard_ = false;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Skip Wizard"))
        showWizard_ = false;
}

// ── Scenario Settings Tab ───────────────────────────────────

void LevelEditor::DrawScenarioSettings()
{
    ImGui::Text("Scenario: %s", manifest_.name.c_str());
    ImGui::InputText("Name", &manifest_.name[0], manifest_.name.size() + 1);
    if (manifest_.name.size() < 64) manifest_.name.resize(64);
    ImGui::InputText("Map File", &manifest_.mapFile[0], manifest_.mapFile.size() + 1);
    if (manifest_.mapFile.size() < 128) manifest_.mapFile.resize(128);

    if (ImGui::Button("Save Scenario"))
        SaveScenario();
    ImGui::SameLine();
    if (ImGui::Button("Load..."))
        RefreshScenarioList();

    // Load dialog popup
    static bool showLoad = false;
    if (ImGui::Button("Open Existing..."))
    {
        showLoad = true;
        RefreshScenarioList();
    }

    if (showLoad)
    {
        ImGui::OpenPopup("Load Scenario");
        if (ImGui::BeginPopupModal("Load Scenario", &showLoad))
        {
            ImGui::BeginChild("##scenarioList", ImVec2(0, 200), true);
            for (auto& s : scenarioList_)
            {
                if (ImGui::Selectable(s.c_str()))
                {
                    LoadScenario(s);
                    showLoad = false;
                    showWizard_ = false;
                }
            }
            ImGui::EndChild();
            if (ImGui::Button("Cancel")) showLoad = false;
            ImGui::EndPopup();
        }
    }
}

// ── Player Editor Tab ───────────────────────────────────────

void LevelEditor::DrawPlayerEditor()
{
    if (ImGui::Button("+ Add Player") && static_cast<int>(players_.size()) < 8)
    {
        PlayerConfig p;
        p.factionId = static_cast<int>(players_.size());
        players_.push_back(p);
    }

    for (int i = 0; i < static_cast<int>(players_.size()); ++i)
    {
        auto& p = players_[i];
        ImGui::PushID(i);
        if (ImGui::TreeNode(p.factionName.empty() ? "Player" : p.factionName.c_str()))
        {
            ImGui::Checkbox("Human", &p.isHuman);
            ImGui::InputText("Faction Name", &p.factionName[0], p.factionName.size() + 1);
            if (p.factionName.size() < 64) p.factionName.resize(64);
            ImGui::InputInt("Faction ID", &p.factionId);
            ImGui::InputInt("Team", &p.team);
            ImGui::InputInt("Start X", &p.startX);
            ImGui::InputInt("Start Y", &p.startY);
            ImGui::InputInt("Start Salvage", &p.startSalvage);
            ImGui::InputInt("Start Heat", &p.startHeat);
            if (!p.isHuman)
            {
                const char* pers[] = {"Aggressive", "Defensive", "Explorer", "Coward"};
                int persIdx = 0;
                for (int j = 0; j < 4; ++j)
                    if (p.aiPersonality == pers[j]) { persIdx = j; break; }
                ImGui::Combo("AI Personality", &persIdx, pers, 4);
                p.aiPersonality = pers[persIdx];
            }
            ImGui::ColorEdit3("Color", &p.colorR, ImGuiColorEditFlags_NoInputs);

            if (players_.size() > 2 && ImGui::Button("Remove"))
            {
                players_.erase(players_.begin() + i);
                ImGui::TreePop();
                ImGui::PopID();
                break;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }
}

// ── Trigger Editor Tab ──────────────────────────────────────

void LevelEditor::DrawTriggerEditor()
{
    if (ImGui::Button("+ Add Trigger"))
    {
        TriggerDef t;
        t.id = triggerIdCounter_++;
        t.name = "Trigger " + std::to_string(t.id);
        triggers_.push_back(t);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save"))
        SaveScenario();

    ImGui::Separator();

    // Left panel: trigger list
    ImGui::BeginChild("##triggerList", ImVec2(250, 0), true);
    for (int i = 0; i < static_cast<int>(triggers_.size()); ++i)
    {
        auto& t = triggers_[i];
        ImGui::PushID(i);
        std::string label = (t.enabled ? "" : "[X] ") + t.name;
        if (ImGui::Selectable(label.c_str(), selectedTrigger_ == i))
            selectedTrigger_ = i;

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Delete"))
            {
                triggers_.erase(triggers_.begin() + i);
                if (selectedTrigger_ >= static_cast<int>(triggers_.size()))
                    selectedTrigger_ = static_cast<int>(triggers_.size()) - 1;
            }
            if (ImGui::MenuItem("Duplicate"))
            {
                TriggerDef dup = t;
                dup.id = triggerIdCounter_++;
                dup.name += " (copy)";
                triggers_.push_back(dup);
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // Right panel: trigger detail
    ImGui::BeginChild("##triggerDetail", ImVec2(0, 0), false);
    if (selectedTrigger_ >= 0 && selectedTrigger_ < static_cast<int>(triggers_.size()))
    {
        DrawTriggerRow(triggers_[selectedTrigger_], selectedTrigger_);
    }
    else
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
            "Select a trigger or add a new one.\n\n"
            "Triggers follow the Event → Conditions → Actions model.\n"
            "All conditions must be true for actions to fire.");
    }
    ImGui::EndChild();
}

void LevelEditor::DrawTriggerRow(TriggerDef& trigger, int index)
{
    ImGui::PushID(index);
    ImGui::Checkbox("Enabled", &trigger.enabled);
    ImGui::SameLine();
    ImGui::InputText("Name", &trigger.name[0], trigger.name.size() + 1);
    if (trigger.name.size() < 64) trigger.name.resize(64);
    ImGui::Checkbox("Repeatable", &trigger.repeatable);

    ImGui::Separator();

    // Event
    ImGui::Text("EVENT:");
    int evtIdx = static_cast<int>(trigger.event);
    if (ImGui::Combo("##event", &evtIdx,
            [](void*, int idx, const char** out) {
                *out = EventName(static_cast<TriggerEvent>(idx)); return true;
            }, nullptr, 15))
        trigger.event = static_cast<TriggerEvent>(evtIdx);

    DrawEventParams(trigger);

    ImGui::Separator();

    // Conditions
    ImGui::Text("CONDITIONS (ALL must be true):");
    int condRemove = -1;
    for (int i = 0; i < static_cast<int>(trigger.conditions.size()); ++i)
    {
        ImGui::PushID(1000 + i);
        auto& [cond, params] = trigger.conditions[i];
        int cIdx = static_cast<int>(cond);
        ImGui::SetNextItemWidth(150);
        if (ImGui::Combo("##condType", &cIdx,
                [](void*, int idx, const char** out) {
                    *out = ConditionName(static_cast<TriggerCondition>(idx)); return true;
                }, nullptr, 12))
            cond = static_cast<TriggerCondition>(cIdx);
        ImGui::SameLine();
        DrawConditionParams(cond, params);
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) condRemove = i;
        ImGui::PopID();
    }
    if (ImGui::Button("+ Condition"))
        trigger.conditions.push_back({TriggerCondition::Always, {}});
    if (condRemove >= 0)
        trigger.conditions.erase(trigger.conditions.begin() + condRemove);

    ImGui::Separator();

    // Actions
    ImGui::Text("ACTIONS:");
    int actRemove = -1;
    for (int i = 0; i < static_cast<int>(trigger.actions.size()); ++i)
    {
        ImGui::PushID(2000 + i);
        auto& [action, params] = trigger.actions[i];
        int aIdx = static_cast<int>(action);
        ImGui::SetNextItemWidth(160);
        if (ImGui::Combo("##actType", &aIdx,
                [](void*, int idx, const char** out) {
                    *out = ActionName(static_cast<TriggerAction>(idx)); return true;
                }, nullptr, 21))
            action = static_cast<TriggerAction>(aIdx);
        ImGui::SameLine();
        DrawActionParams(action, params);
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) actRemove = i;
        ImGui::PopID();
    }
    if (ImGui::Button("+ Action"))
        trigger.actions.push_back({TriggerAction::DisplayMessage, {{"text", "Hello World"}}});
    if (actRemove >= 0)
        trigger.actions.erase(trigger.actions.begin() + actRemove);

    ImGui::PopID();
}

// ── Event/Condition/Action param editors ────────────────────

void LevelEditor::DrawEventParams(TriggerDef& trigger)
{
    auto& p = trigger.eventParams;
    switch (trigger.event)
    {
        case TriggerEvent::TimerExpired:
        case TriggerEvent::MissionTimerTick: {
            int t = p.value("timer", 300);
            ImGui::InputInt("Timer (ticks)", &t); p["timer"] = t;
            break;
        }
        case TriggerEvent::UnitKilled:
        case TriggerEvent::UnitBuilt:
        case TriggerEvent::UnitEntersRegion: {
            static char ut[64] = {};
            strncpy(ut, p.value("unitType", "").c_str(), 63);
            ImGui::InputText("Unit Type", ut, sizeof(ut)); p["unitType"] = ut;
            break;
        }
        case TriggerEvent::UnitCountReached: {
            int c = p.value("count", 10);
            ImGui::InputInt("Count", &c); p["count"] = c;
            break;
        }
        default: break;
    }
}

void LevelEditor::DrawConditionParams(TriggerCondition cond, json& params)
{
    switch (cond)
    {
        case TriggerCondition::HasResource: {
            int amt = params.value("amount", 100);
            ImGui::InputInt("Amount", &amt); params["amount"] = amt;
            break;
        }
        case TriggerCondition::UnitCount: {
            int c = params.value("count", 5);
            ImGui::InputInt("Count", &c); params["count"] = c;
            break;
        }
        case TriggerCondition::TimerCompare: {
            int t = params.value("timer", 300);
            ImGui::InputInt("Timer", &t); params["timer"] = t;
            break;
        }
        case TriggerCondition::RandomChance: {
            float ch = params.value("chance", 0.5f);
            ImGui::SliderFloat("Chance", &ch, 0.0f, 1.0f); params["chance"] = ch;
            break;
        }
        case TriggerCondition::VariableEquals:
        case TriggerCondition::VariableGreater:
        case TriggerCondition::VariableLess: {
            static char vn[64] = {};
            strncpy(vn, params.value("var", "").c_str(), 63);
            ImGui::InputText("Variable", vn, sizeof(vn)); params["var"] = vn;
            int val = params.value("value", 0);
            ImGui::InputInt("Value", &val); params["value"] = val;
            break;
        }
        default: ImGui::TextDisabled("(no params)"); break;
    }
}

void LevelEditor::DrawActionParams(TriggerAction action, json& params)
{
    switch (action)
    {
        case TriggerAction::SpawnUnit: {
            static char ut[64] = {}; strncpy(ut, params.value("unitType", "Driller").c_str(), 63);
            ImGui::InputText("Type", ut, sizeof(ut)); params["unitType"] = ut;
            int x = params.value("x", 0); ImGui::InputInt("X", &x); params["x"] = x;
            int y = params.value("y", 0); ImGui::InputInt("Y", &y); params["y"] = y;
            break;
        }
        case TriggerAction::DisplayMessage: {
            static char txt[256] = {}; strncpy(txt, params.value("text", "").c_str(), 255);
            ImGui::InputText("Text", txt, sizeof(txt)); params["text"] = txt;
            break;
        }
        case TriggerAction::GiveResource: {
            int amt = params.value("amount", 500);
            ImGui::InputInt("Amount", &amt); params["amount"] = amt;
            break;
        }
        case TriggerAction::PlaySound: {
            static char sn[128] = {}; strncpy(sn, params.value("sound", "").c_str(), 127);
            ImGui::InputText("Sound", sn, sizeof(sn)); params["sound"] = sn;
            break;
        }
        case TriggerAction::SetTimer: {
            int t = params.value("timer", 300);
            ImGui::InputInt("Timer", &t); params["timer"] = t;
            break;
        }
        case TriggerAction::TeleportUnit: {
            int x = params.value("x", 0); ImGui::InputInt("X", &x); params["x"] = x;
            int y = params.value("y", 0); ImGui::InputInt("Y", &y); params["y"] = y;
            break;
        }
        case TriggerAction::SetObjective: {
            int oid = params.value("objectiveId", 0);
            ImGui::InputInt("Obj ID", &oid); params["objectiveId"] = oid;
            break;
        }
        default: ImGui::TextDisabled("(no params)"); break;
    }
}

// ── Objectives Editor Tab ───────────────────────────────────

void LevelEditor::DrawObjectivesEditor()
{
    if (ImGui::Button("+ Add Objective"))
    {
        ObjectiveDef obj;
        obj.id = objIdCounter_++;
        obj.name = "Objective " + std::to_string(obj.id);
        objectives_.push_back(obj);
    }

    ImGui::Separator();
    ImGui::BeginChild("##objList", ImVec2(0, 0), false);

    int removeIdx = -1;
    for (int i = 0; i < static_cast<int>(objectives_.size()); ++i)
    {
        auto& obj = objectives_[i];
        ImGui::PushID(i);
        ImGui::Checkbox("##completed", &obj.completed);
        ImGui::SameLine();
        ImGui::Checkbox("Primary", &obj.isPrimary);
        ImGui::SameLine();
        ImGui::InputText("##name", &obj.name[0], obj.name.size() + 1);
        if (obj.name.size() < 64) obj.name.resize(64);
        ImGui::InputText("Description", &obj.description[0], obj.description.size() + 1);
        if (obj.description.size() < 128) obj.description.resize(128);
        ImGui::Checkbox("Hidden", &obj.hidden);
        ImGui::InputText("Complete Cond.", &obj.completeCondition[0], obj.completeCondition.size() + 1);
        if (obj.completeCondition.size() < 128) obj.completeCondition.resize(128);
        if (ImGui::Button("Remove")) removeIdx = i;
        ImGui::Separator();
        ImGui::PopID();
    }
    if (removeIdx >= 0)
        objectives_.erase(objectives_.begin() + removeIdx);

    ImGui::EndChild();
}

// ── Diplomacy Editor Tab ────────────────────────────────────

void LevelEditor::DrawDiplomacyEditor()
{
    ImGui::Text("Diplomacy Matrix (%d players)", static_cast<int>(players_.size()));

    ImGui::Checkbox("Use Fixed Teams", &useFixedTeams_);
    ImGui::SameLine();
    if (ImGui::Button("Resize Matrix"))
    {
        diplomacySize_ = static_cast<int>(players_.size());
        diplomacyMatrix_.resize(diplomacySize_);
        for (auto& row : diplomacyMatrix_) row.resize(diplomacySize_, DiplomacyState::Ally);
        // Set diagonal to Ally, others to Enemy
        for (int i = 0; i < diplomacySize_; ++i)
            for (int j = 0; j < diplomacySize_; ++j)
                diplomacyMatrix_[i][j] = (i == j) ? DiplomacyState::Ally : DiplomacyState::Enemy;
    }

    ImGui::Separator();

    if (useFixedTeams_)
    {
        for (int i = 0; i < static_cast<int>(players_.size()); ++i)
        {
            ImGui::Text("Player %d (%s): Team %d",
                i + 1, players_[i].factionName.c_str(), players_[i].team);
        }
    }
    else
    {
        // N×N matrix
        if (ImGui::BeginTable("##diploMatrix", diplomacySize_ + 1,
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
        {
            ImGui::TableSetupColumn("P\\P");
            for (int j = 0; j < diplomacySize_; ++j)
            {
                char hdr[8]; snprintf(hdr, sizeof(hdr), "P%d", j + 1);
                ImGui::TableSetupColumn(hdr);
            }
            ImGui::TableHeadersRow();

            for (int i = 0; i < diplomacySize_; ++i)
            {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("P%d", i + 1);

                for (int j = 0; j < diplomacySize_; ++j)
                {
                    ImGui::TableSetColumnIndex(j + 1);
                    ImGui::PushID(i * 8 + j);
                    int state = static_cast<int>(diplomacyMatrix_[i][j]);
                    const char* states[] = {"Ally", "Neutral", "Enemy"};
                    ImGui::SetNextItemWidth(70);
                    ImGui::Combo("##dip", &state, states, 3);
                    diplomacyMatrix_[i][j] = static_cast<DiplomacyState>(state);
                    ImGui::PopID();
                }
            }
            ImGui::EndTable();
        }
    }
}

// ═════════════════════════════════════════════════════════════
// Scenario I/O
// ═════════════════════════════════════════════════════════════

void LevelEditor::NewScenario()
{
    std::string dir = ScenarioDir() + "/" + manifest_.name;
    mkdir(dir.c_str(), 0755);
    currentScenarioPath_ = dir;
    SaveScenario();
    Log("Created scenario: " + dir);
}

void LevelEditor::SaveScenario()
{
    if (currentScenarioPath_.empty())
    {
        currentScenarioPath_ = ScenarioDir() + "/" + manifest_.name;
        mkdir(currentScenarioPath_.c_str(), 0755);
    }

    // scenario.json
    json sj;
    sj["name"] = manifest_.name;
    sj["description"] = manifest_.description;
    sj["mapFile"] = manifest_.mapFile;
    sj["maxPlayers"] = manifest_.maxPlayers;

    std::ofstream f(currentScenarioPath_ + "/scenario.json");
    f << sj.dump(2); f.close();

    // players.json
    json pj = json::array();
    for (auto& p : players_)
    {
        json pjo;
        pjo["factionId"] = p.factionId;
        pjo["factionName"] = p.factionName;
        pjo["team"] = p.team;
        pjo["startX"] = p.startX;
        pjo["startY"] = p.startY;
        pjo["startSalvage"] = p.startSalvage;
        pjo["startHeat"] = p.startHeat;
        pjo["aiPersonality"] = p.aiPersonality;
        pjo["isHuman"] = p.isHuman;
        pj.push_back(pjo);
    }
    std::ofstream pf(currentScenarioPath_ + "/players.json");
    pf << pj.dump(2); pf.close();

    // triggers.json
    json tj = json::array();
    for (auto& t : triggers_)
    {
        json tjo;
        tjo["id"] = t.id;
        tjo["name"] = t.name;
        tjo["enabled"] = t.enabled;
        tjo["event"] = static_cast<int>(t.event);
        tjo["eventParams"] = t.eventParams;
        json conds = json::array();
        for (auto& [c, cp] : t.conditions)
            conds.push_back({{"type", static_cast<int>(c)}, {"params", cp}});
        tjo["conditions"] = conds;
        json acts = json::array();
        for (auto& [a, ap] : t.actions)
            acts.push_back({{"type", static_cast<int>(a)}, {"params", ap}});
        tjo["actions"] = acts;
        tj.push_back(tjo);
    }
    std::ofstream tf(currentScenarioPath_ + "/triggers.json");
    tf << tj.dump(2); tf.close();

    // objectives.json
    json oj = json::array();
    for (auto& o : objectives_)
    {
        json ojo;
        ojo["id"] = o.id;
        ojo["name"] = o.name;
        ojo["description"] = o.description;
        ojo["isPrimary"] = o.isPrimary;
        ojo["completeCondition"] = o.completeCondition;
        oj.push_back(ojo);
    }
    std::ofstream of(currentScenarioPath_ + "/objectives.json");
    of << oj.dump(2); of.close();

    // diplomacy.json
    json dj;
    dj["useFixedTeams"] = useFixedTeams_;
    dj["numTeams"] = numTeams_;
    json matrix = json::array();
    for (auto& row : diplomacyMatrix_)
    {
        json r = json::array();
        for (auto& cell : row) r.push_back(static_cast<int>(cell));
        matrix.push_back(r);
    }
    dj["matrix"] = matrix;
    std::ofstream df(currentScenarioPath_ + "/diplomacy.json");
    df << dj.dump(2); df.close();

    Log("Saved: " + currentScenarioPath_);
}

void LevelEditor::LoadScenario(const std::string& dir)
{
    currentScenarioPath_ = ScenarioDir() + "/" + dir;

    auto loadJson = [](const std::string& path) -> json {
        std::ifstream f(path);
        if (!f.good()) return {};
        return json::parse(f);
    };

    // scenario.json
    auto sj = loadJson(currentScenarioPath_ + "/scenario.json");
    manifest_.name = sj.value("name", "Untitled");
    manifest_.description = sj.value("description", "");
    manifest_.mapFile = sj.value("mapFile", "");
    manifest_.maxPlayers = sj.value("maxPlayers", 2);

    // players.json
    players_.clear();
    auto pj = loadJson(currentScenarioPath_ + "/players.json");
    for (auto& pjo : pj)
    {
        PlayerConfig pc;
        pc.factionId = pjo.value("factionId", 0);
        pc.factionName = pjo.value("factionName", "");
        pc.team = pjo.value("team", 0);
        pc.startX = pjo.value("startX", 0);
        pc.startY = pjo.value("startY", 0);
        pc.startSalvage = pjo.value("startSalvage", 1000);
        pc.startHeat = pjo.value("startHeat", 500);
        pc.aiPersonality = pjo.value("aiPersonality", "Aggressive");
        pc.isHuman = pjo.value("isHuman", false);
        players_.push_back(pc);
    }

    Log("Loaded: " + currentScenarioPath_ + " (" + std::to_string(players_.size()) + " players)");
    showWizard_ = false;
}

void LevelEditor::RefreshScenarioList()
{
    scenarioList_.clear();
    std::string dir = ScenarioDir();
    mkdir(dir.c_str(), 0755);

    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr)
    {
        if (entry->d_type == DT_DIR && entry->d_name[0] != '.')
            scenarioList_.push_back(entry->d_name);
    }
    closedir(d);
    std::sort(scenarioList_.begin(), scenarioList_.end());
}

} // namespace beigebox
