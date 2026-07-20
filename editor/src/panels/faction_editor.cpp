// editor/src/panels/faction_editor.cpp
// ─────────────────────────────────────────────────────────────
// Faction Editor + Build/Export Manager — implementation.
// ─────────────────────────────────────────────────────────────

#include "faction_editor.h"
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

FactionEditor::FactionEditor()
{
    faction_.name = "ThawCorp";
    faction_.displayName = "Thaw Corporation";
    faction_.colorR = 0.3f; faction_.colorG = 0.7f; faction_.colorB = 0.3f;
    faction_.uiColorR = 0.15f; faction_.uiColorG = 0.4f; faction_.uiColorB = 0.15f;
}

void FactionEditor::Log(const std::string& msg)
{
    if (aiChat_) aiChat_->AppendMessage("system", "[Faction] " + msg);
    SDL_Log("[Faction] %s", msg.c_str());
}

std::string FactionEditor::FactionsDir() const { return rootPath_ + "/data/factions"; }

// ═════════════════════════════════════════════════════════════
// Main Draw
// ═════════════════════════════════════════════════════════════

void FactionEditor::Draw()
{
    ImGui::Begin("Faction Editor");

    if (ImGui::BeginTabBar("##factionTabs"))
    {
        if (ImGui::BeginTabItem("Faction")) { DrawFactionTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Tech Tree")) { DrawTechTreeTab(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Build/Export")) { DrawBuildTab(); ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ── Faction Tab ─────────────────────────────────────────────

void FactionEditor::DrawFactionTab()
{
    // Toolbar
    if (ImGui::Button("New"))
        showNewDialog_ = true;
    ImGui::SameLine();
    if (ImGui::Button("Save"))
        SaveFaction();
    ImGui::SameLine();
    if (ImGui::Button("Load..."))
        RefreshFactionList();
    ImGui::SameLine();
    ImGui::Text("Faction: %s", faction_.displayName.c_str());

    ImGui::Separator();

    // Two-column layout
    ImGui::BeginChild("##factionLeft", ImVec2(200, 0), true);
    for (auto& name : factionList_)
    {
        if (ImGui::Selectable(name.c_str(), faction_.name == name))
            LoadFaction(name);
    }
    ImGui::EndChild();
    ImGui::SameLine();

    ImGui::BeginChild("##factionRight", ImVec2(0, 0), false);

    // Identity
    ImGui::InputText("Internal Name", &faction_.name[0], faction_.name.size() + 1);
    if (faction_.name.size() < 64) faction_.name.resize(64);
    ImGui::InputText("Display Name", &faction_.displayName[0], faction_.displayName.size() + 1);
    if (faction_.displayName.size() < 64) faction_.displayName.resize(64);

    ImGui::ColorEdit3("Faction Color", &faction_.colorR, ImGuiColorEditFlags_NoInputs);
    ImGui::SameLine();
    ImGui::ColorEdit3("UI Theme", &faction_.uiColorR, ImGuiColorEditFlags_NoInputs);

    ImGui::InputText("Music Theme", &faction_.musicTheme[0], faction_.musicTheme.size() + 1);
    if (faction_.musicTheme.size() < 128) faction_.musicTheme.resize(128);

    ImGui::InputTextMultiline("Lore", &faction_.lore[0], faction_.lore.size() + 1,
        ImVec2(0, 60));
    if (faction_.lore.size() < 256) faction_.lore.resize(256);

    ImGui::Separator();

    // Starting state
    ImGui::InputInt("Start Salvage", &faction_.startSalvage);
    ImGui::InputInt("Start Heat", &faction_.startHeat);

    const char* aiOpts[] = {"Aggressive", "Defensive", "Explorer", "Coward"};
    int aiIdx = 0;
    for (int i = 0; i < 4; ++i)
        if (faction_.defaultAI == aiOpts[i]) { aiIdx = i; break; }
    ImGui::Combo("Default AI", &aiIdx, aiOpts, 4);
    faction_.defaultAI = aiOpts[aiIdx];

    // Starting units
    ImGui::Text("Starting Units:");
    ImGui::InputText("##addStartUnit", unitAddBuf_, sizeof(unitAddBuf_));
    ImGui::SameLine();
    if (ImGui::Button("+##su") && unitAddBuf_[0])
    {
        faction_.startingUnits.push_back(unitAddBuf_);
        unitAddBuf_[0] = '\0';
    }
    for (int i = 0; i < static_cast<int>(faction_.startingUnits.size()); ++i)
    {
        ImGui::BulletText("%s", faction_.startingUnits[i].c_str());
        ImGui::SameLine();
        ImGui::PushID(1000 + i);
        if (ImGui::SmallButton("X"))
            faction_.startingUnits.erase(faction_.startingUnits.begin() + i);
        ImGui::PopID();
    }

    ImGui::Separator();

    // Buildable units
    ImGui::Text("Buildable Units:");
    ImGui::InputText("##addUnit", unitAddBuf_, sizeof(unitAddBuf_));
    ImGui::SameLine();
    if (ImGui::Button("+##bu") && unitAddBuf_[0])
    {
        faction_.buildableUnits.push_back(unitAddBuf_);
        unitAddBuf_[0] = '\0';
    }
    for (int i = 0; i < static_cast<int>(faction_.buildableUnits.size()); ++i)
    {
        ImGui::BulletText("%s", faction_.buildableUnits[i].c_str());
        ImGui::SameLine();
        ImGui::PushID(2000 + i);
        if (ImGui::SmallButton("X"))
            faction_.buildableUnits.erase(faction_.buildableUnits.begin() + i);
        ImGui::PopID();
    }

    ImGui::Separator();

    // Name pools
    if (ImGui::CollapsingHeader("Hero Name Pools"))
    {
        ImGui::Text("First Names:");
        ImGui::InputText("##addFirst", unitAddBuf_, sizeof(unitAddBuf_));
        ImGui::SameLine();
        if (ImGui::Button("+##fn") && unitAddBuf_[0])
        {
            faction_.firstNamePool.push_back(unitAddBuf_);
            unitAddBuf_[0] = '\0';
        }
        int fnRemove = -1;
        for (int i = 0; i < static_cast<int>(faction_.firstNamePool.size()); ++i)
        {
            ImGui::BulletText("%s", faction_.firstNamePool[i].c_str());
            ImGui::SameLine();
            ImGui::PushID(3000 + i);
            if (ImGui::SmallButton("X")) fnRemove = i;
            ImGui::PopID();
        }
        if (fnRemove >= 0) faction_.firstNamePool.erase(faction_.firstNamePool.begin() + fnRemove);

        ImGui::Text("Last Names:");
        ImGui::InputText("##addLast", unitAddBuf_, sizeof(unitAddBuf_));
        ImGui::SameLine();
        if (ImGui::Button("+##ln") && unitAddBuf_[0])
        {
            faction_.lastNamePool.push_back(unitAddBuf_);
            unitAddBuf_[0] = '\0';
        }
        int lnRemove = -1;
        for (int i = 0; i < static_cast<int>(faction_.lastNamePool.size()); ++i)
        {
            ImGui::BulletText("%s", faction_.lastNamePool[i].c_str());
            ImGui::SameLine();
            ImGui::PushID(4000 + i);
            if (ImGui::SmallButton("X")) lnRemove = i;
            ImGui::PopID();
        }
        if (lnRemove >= 0) faction_.lastNamePool.erase(faction_.lastNamePool.begin() + lnRemove);
    }

    ImGui::Separator();

    // Init script
    ImGui::Text("Init Script (Lua):");
    ImGui::InputTextMultiline("##initScript", &faction_.initScript[0],
        faction_.initScript.size() + 1, ImVec2(0, 80));
    if (faction_.initScript.size() < 512) faction_.initScript.resize(512);

    ImGui::EndChild();

    // New dialog
    if (showNewDialog_)
    {
        ImGui::OpenPopup("New Faction");
        if (ImGui::BeginPopupModal("New Faction", &showNewDialog_))
        {
            ImGui::InputText("Name", newFactionName_, sizeof(newFactionName_));
            if (ImGui::Button("Create") && newFactionName_[0])
            {
                faction_ = FactionDef();
                faction_.name = newFactionName_;
                faction_.displayName = newFactionName_;
                SaveFaction();
                showNewDialog_ = false;
                RefreshFactionList();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel")) showNewDialog_ = false;
            ImGui::EndPopup();
        }
    }
}

// ── Tech Tree Tab ───────────────────────────────────────────

void FactionEditor::DrawTechTreeTab()
{
    ImGui::Text("Tech Tree: %s", faction_.displayName.c_str());

    // Add tech
    ImGui::InputText("Tech Name", techNameBuf_, sizeof(techNameBuf_));
    ImGui::SameLine();
    ImGui::InputText("Cost", techCostBuf_, sizeof(techCostBuf_));
    ImGui::SameLine();
    ImGui::InputText("Time", techTimeBuf_, sizeof(techTimeBuf_));
    ImGui::SameLine();
    if (ImGui::Button("+ Add Tech") && techNameBuf_[0])
    {
        TechNode tn;
        tn.name = techNameBuf_;
        tn.cost = atoi(techCostBuf_);
        tn.researchTime = atoi(techTimeBuf_);
        tn.graphX = 100 + faction_.techTree.size() * 180;
        tn.graphY = 100 + (faction_.techTree.size() % 3) * 120;
        faction_.techTree.push_back(tn);
        techNameBuf_[0] = '\0';
        techCostBuf_[0] = '\0';
        techTimeBuf_[0] = '\0';
    }

    ImGui::Separator();

    // Tech list (left) + graph (right)
    ImGui::BeginChild("##techList", ImVec2(200, 0), true);
    for (int i = 0; i < static_cast<int>(faction_.techTree.size()); ++i)
    {
        auto& tn = faction_.techTree[i];
        ImGui::PushID(i);
        if (ImGui::Selectable(tn.name.c_str(), selectedTech_ == i))
            selectedTech_ = i;

        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Delete"))
            {
                faction_.techTree.erase(faction_.techTree.begin() + i);
                if (selectedTech_ >= static_cast<int>(faction_.techTree.size()))
                    selectedTech_ = -1;
            }
            ImGui::EndPopup();
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // Tech detail + graph
    ImGui::BeginChild("##techDetail", ImVec2(0, 0), false);
    if (selectedTech_ >= 0 && selectedTech_ < static_cast<int>(faction_.techTree.size()))
    {
        auto& tn = faction_.techTree[selectedTech_];
        ImGui::InputText("Name", &tn.name[0], tn.name.size() + 1);
        if (tn.name.size() < 64) tn.name.resize(64);
        ImGui::InputInt("Cost", &tn.cost);
        ImGui::InputInt("Research Time", &tn.researchTime);

        ImGui::Text("Prerequisites:");
        ImGui::InputText("##addPrereq", unitAddBuf_, sizeof(unitAddBuf_));
        ImGui::SameLine();
        if (ImGui::Button("+##pr") && unitAddBuf_[0])
        {
            tn.prerequisites.push_back(unitAddBuf_);
            unitAddBuf_[0] = '\0';
        }
        for (int i = 0; i < static_cast<int>(tn.prerequisites.size()); ++i)
        {
            ImGui::BulletText("%s", tn.prerequisites[i].c_str());
            ImGui::SameLine();
            ImGui::PushID(5000 + i);
            if (ImGui::SmallButton("X"))
                tn.prerequisites.erase(tn.prerequisites.begin() + i);
            ImGui::PopID();
        }

        ImGui::Text("Unlocks:");
        ImGui::InputText("##addUnlock", unitAddBuf_, sizeof(unitAddBuf_));
        ImGui::SameLine();
        if (ImGui::Button("+##un") && unitAddBuf_[0])
        {
            tn.unlocks.push_back(unitAddBuf_);
            unitAddBuf_[0] = '\0';
        }
        for (int i = 0; i < static_cast<int>(tn.unlocks.size()); ++i)
        {
            ImGui::BulletText("%s", tn.unlocks[i].c_str());
            ImGui::SameLine();
            ImGui::PushID(6000 + i);
            if (ImGui::SmallButton("X"))
                tn.unlocks.erase(tn.unlocks.begin() + i);
            ImGui::PopID();
        }
    }

    ImGui::Separator();
    DrawTechGraph(faction_);
    ImGui::EndChild();
}

// ── Tech Tree Graph ─────────────────────────────────────────

void FactionEditor::DrawTechGraph(FactionDef& faction)
{
    ImVec2 canvasSize(ImGui::GetContentRegionAvail().x, 300);
    ImGui::BeginChild("##graphCanvas", canvasSize, true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 cp = ImGui::GetCursorScreenPos();
    ImVec2 canvasEnd(cp.x + canvasSize.x, cp.y + canvasSize.y);

    // Background
    dl->AddRectFilled(cp, canvasEnd, IM_COL32(18, 18, 28, 255));

    // Grid
    for (float x = cp.x; x < canvasEnd.x; x += 50)
        dl->AddLine(ImVec2(x, cp.y), ImVec2(x, canvasEnd.y), IM_COL32(30, 30, 45, 60));
    for (float y = cp.y; y < canvasEnd.y; y += 50)
        dl->AddLine(ImVec2(cp.x, y), ImVec2(canvasEnd.x, y), IM_COL32(30, 30, 45, 60));

    // Arrows between prerequisites and their dependents
    for (auto& tn : faction.techTree)
    {
        for (auto& prereqName : tn.prerequisites)
        {
            // Find prerequisite node
            for (auto& pn : faction.techTree)
            {
                if (pn.name == prereqName)
                {
                    float x1 = cp.x + pn.graphX + 70;
                    float y1 = cp.y + pn.graphY + 15;
                    float x2 = cp.x + tn.graphX;
                    float y2 = cp.y + tn.graphY + 15;
                    dl->AddBezierCubic(
                        ImVec2(x1, y1), ImVec2(x1 + 40, y1),
                        ImVec2(x2 - 40, y2), ImVec2(x2, y2),
                        IM_COL32(200, 180, 60, 150), 1.5f);
                    // Arrowhead
                    dl->AddTriangleFilled(
                        ImVec2(x2, y2), ImVec2(x2 - 8, y2 - 4), ImVec2(x2 - 8, y2 + 4),
                        IM_COL32(200, 180, 60, 180));
                }
            }
        }
    }

    // Nodes
    for (int i = 0; i < static_cast<int>(faction.techTree.size()); ++i)
    {
        auto& tn = faction.techTree[i];
        float nx = cp.x + tn.graphX;
        float ny = cp.y + tn.graphY;
        float nw = 140, nh = 30;

        bool isSelected = (i == selectedTech_);
        ImU32 bgCol = isSelected ? IM_COL32(60, 120, 200, 255) : IM_COL32(40, 70, 130, 220);
        ImU32 borderCol = isSelected ? IM_COL32(255, 200, 50, 255) : IM_COL32(80, 120, 200, 150);

        dl->AddRectFilled(ImVec2(nx, ny), ImVec2(nx + nw, ny + nh), bgCol, 4.0f);
        dl->AddRect(ImVec2(nx, ny), ImVec2(nx + nw, ny + nh), borderCol, 4.0f);
        dl->AddText(ImVec2(nx + 6, ny + 6), IM_COL32(255, 255, 255, 255), tn.name.c_str());

        // Click to select
        if (ImGui::IsMouseHoveringRect(ImVec2(nx, ny), ImVec2(nx + nw, ny + nh)))
        {
            if (ImGui::IsMouseClicked(0))
            {
                selectedTech_ = i;
                draggingTech_ = i;
            }
        }

        // Drag to move
        if (draggingTech_ == i && ImGui::IsMouseDragging(0))
        {
            ImVec2 delta = ImGui::GetMouseDragDelta(0);
            tn.graphX += delta.x;
            tn.graphY += delta.y;
            ImGui::ResetMouseDragDelta(0);
        }
    }
    if (ImGui::IsMouseReleased(0)) draggingTech_ = -1;

    ImGui::EndChild();
    ImGui::TextDisabled("Drag nodes to arrange. Right-click for context menu.");
}

// ── Build/Export Tab ────────────────────────────────────────

void FactionEditor::DrawBuildTab()
{
    ImGui::Text("Build & Export Manager");
    ImGui::Separator();

    ImGui::InputText("Output Path", buildOutputPath_, sizeof(buildOutputPath_));
    ImGui::InputText("Version", buildVersion_, sizeof(buildVersion_));

    ImGui::Checkbox("Linux", &buildLinux_);
    ImGui::SameLine();
    ImGui::Checkbox("Windows (cross)", &buildWindows_);

    ImGui::Separator();
    ImGui::Text("Package contents:");
    ImGui::BulletText("beigebox_runtime (executable)");
    ImGui::BulletText("assets/ (sprites, sounds, music, scripts)");
    ImGui::BulletText("data/ (units, buildings, factions, scenarios)");
    ImGui::BulletText("ui/ (menus)");
    ImGui::BulletText("launch.sh (Linux) / launch.bat (Windows)");
    ImGui::BulletText("README.txt");

    ImGui::Separator();
    if (ImGui::Button("Build & Export", ImVec2(160, 40)))
        DoBuild();

    ImGui::SameLine();
    if (ImGui::Button("Generate Launch Script Only"))
    {
        std::string script = GenerateLaunchScript();
        std::string path = std::string(buildOutputPath_) + "/launch.sh";
        std::ofstream f(path);
        f << script;
        f.close();
        Log("Generated: " + path);
    }
}

// ═════════════════════════════════════════════════════════════
// Faction I/O
// ═════════════════════════════════════════════════════════════

void FactionEditor::LoadFaction(const std::string& name)
{
    std::string path = FactionsDir() + "/" + name + ".json";
    std::ifstream f(path);
    if (!f.good()) { Log("Not found: " + path); return; }

    json j = json::parse(f);
    faction_.name = j.value("name", name);
    faction_.displayName = j.value("displayName", name);
    faction_.lore = j.value("lore", "");
    faction_.musicTheme = j.value("musicTheme", "");
    faction_.initScript = j.value("initScript", "");
    if (j.contains("color")) {
        faction_.colorR = j["color"][0]; faction_.colorG = j["color"][1]; faction_.colorB = j["color"][2];
    }
    if (j.contains("uiColor")) {
        faction_.uiColorR = j["uiColor"][0]; faction_.uiColorG = j["uiColor"][1]; faction_.uiColorB = j["uiColor"][2];
    }
    faction_.startSalvage = j.value("startSalvage", 1000);
    faction_.startHeat = j.value("startHeat", 500);
    faction_.defaultAI = j.value("defaultAI", "Aggressive");

    faction_.startingUnits.clear();
    if (j.contains("startingUnits"))
        for (auto& u : j["startingUnits"]) faction_.startingUnits.push_back(u.get<std::string>());

    faction_.buildableUnits.clear();
    if (j.contains("buildableUnits"))
        for (auto& u : j["buildableUnits"]) faction_.buildableUnits.push_back(u.get<std::string>());

    faction_.buildableBuildings.clear();
    if (j.contains("buildableBuildings"))
        for (auto& u : j["buildableBuildings"]) faction_.buildableBuildings.push_back(u.get<std::string>());

    faction_.superweapons.clear();
    if (j.contains("superweapons"))
        for (auto& u : j["superweapons"]) faction_.superweapons.push_back(u.get<std::string>());

    faction_.techTree.clear();
    if (j.contains("techTree"))
    {
        for (auto& tj : j["techTree"])
        {
            TechNode tn;
            tn.name = tj.value("name", "");
            tn.description = tj.value("description", "");
            tn.cost = tj.value("cost", 100);
            tn.researchTime = tj.value("researchTime", 30);
            tn.graphX = tj.value("graphX", 0.0f);
            tn.graphY = tj.value("graphY", 0.0f);
            if (tj.contains("prerequisites"))
                for (auto& p : tj["prerequisites"]) tn.prerequisites.push_back(p.get<std::string>());
            if (tj.contains("unlocks"))
                for (auto& u : tj["unlocks"]) tn.unlocks.push_back(u.get<std::string>());
            faction_.techTree.push_back(tn);
        }
    }

    faction_.firstNamePool.clear();
    if (j.contains("firstNamePool"))
        for (auto& n : j["firstNamePool"]) faction_.firstNamePool.push_back(n.get<std::string>());

    faction_.lastNamePool.clear();
    if (j.contains("lastNamePool"))
        for (auto& n : j["lastNamePool"]) faction_.lastNamePool.push_back(n.get<std::string>());

    faction_.titleProgression.clear();
    if (j.contains("titleProgression"))
        for (auto& t : j["titleProgression"]) faction_.titleProgression.push_back(t.get<std::string>());

    selectedTech_ = -1;
    Log("Loaded: " + path);
}

void FactionEditor::SaveFaction()
{
    std::string dir = FactionsDir();
    mkdir(dir.c_str(), 0755);

    json j;
    j["name"] = faction_.name;
    j["displayName"] = faction_.displayName;
    j["lore"] = faction_.lore;
    j["musicTheme"] = faction_.musicTheme;
    j["initScript"] = faction_.initScript;
    j["color"] = {faction_.colorR, faction_.colorG, faction_.colorB};
    j["uiColor"] = {faction_.uiColorR, faction_.uiColorG, faction_.uiColorB};
    j["startSalvage"] = faction_.startSalvage;
    j["startHeat"] = faction_.startHeat;
    j["defaultAI"] = faction_.defaultAI;

    j["startingUnits"] = faction_.startingUnits;
    j["buildableUnits"] = faction_.buildableUnits;
    j["buildableBuildings"] = faction_.buildableBuildings;
    j["superweapons"] = faction_.superweapons;

    json techArr = json::array();
    for (auto& tn : faction_.techTree)
    {
        json tj;
        tj["name"] = tn.name;
        tj["description"] = tn.description;
        tj["cost"] = tn.cost;
        tj["researchTime"] = tn.researchTime;
        tj["graphX"] = tn.graphX;
        tj["graphY"] = tn.graphY;
        tj["prerequisites"] = tn.prerequisites;
        tj["unlocks"] = tn.unlocks;
        techArr.push_back(tj);
    }
    j["techTree"] = techArr;

    j["firstNamePool"] = faction_.firstNamePool;
    j["lastNamePool"] = faction_.lastNamePool;
    j["titleProgression"] = faction_.titleProgression;

    std::string path = dir + "/" + faction_.name + ".json";
    std::ofstream f(path);
    f << j.dump(2);
    f.close();
    Log("Saved: " + path);
}

void FactionEditor::RefreshFactionList()
{
    factionList_.clear();
    std::string dir = FactionsDir();
    mkdir(dir.c_str(), 0755);

    DIR* d = opendir(dir.c_str());
    if (!d) return;
    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr)
    {
        std::string name(entry->d_name);
        if (entry->d_type == DT_REG && name.size() > 5 &&
            name.substr(name.size() - 5) == ".json")
            factionList_.push_back(name.substr(0, name.size() - 5));
    }
    closedir(d);
    std::sort(factionList_.begin(), factionList_.end());
}

// ═════════════════════════════════════════════════════════════
// Build/Export
// ═════════════════════════════════════════════════════════════

std::string FactionEditor::GenerateLaunchScript() const
{
    std::string script = "#!/bin/sh\n";
    script += "# M.A.D. Runtime Launcher — v" + std::string(buildVersion_) + "\n";
    script += "cd \"$(dirname \"$0\")\"\n";
    script += "export LD_LIBRARY_PATH=\"./lib:$LD_LIBRARY_PATH\"\n";
    script += "exec ./beigebox_runtime \"$@\"\n";
    return script;
}

void FactionEditor::DoBuild()
{
    std::string dest(buildOutputPath_);
    std::string cmd = "rm -rf " + dest + " && mkdir -p " + dest;

    // Create directory structure
    mkdir(dest.c_str(), 0755);
    std::string subdirs[] = {"/assets", "/assets/sprites", "/assets/sounds",
        "/assets/music", "/assets/scripts", "/data", "/data/units",
        "/data/buildings", "/data/heroes", "/data/factions",
        "/scenarios", "/ui", "/ui/menus"};
    for (auto& sd : subdirs)
    {
        std::string full = dest + sd;
        mkdir(full.c_str(), 0755);
    }

    // Copy runtime executable
    std::string srcExe = rootPath_ + "/build/engine/beigebox_runtime";
    std::string dstExe = dest + "/beigebox_runtime";
    std::ifstream exeIn(srcExe, std::ios::binary);
    std::ofstream exeOut(dstExe, std::ios::binary);
    if (exeIn.good() && exeOut.good())
    {
        exeOut << exeIn.rdbuf();
        chmod(dstExe.c_str(), 0755);
        Log("Copied runtime executable.");
    }
    else
        Log("Warning: could not copy runtime (build first).");

    // Copy asset directories
    auto copyDir = [&](const std::string& srcSub, const std::string& dstSub) {
        std::string src = rootPath_ + "/" + srcSub;
        std::string dst = dest + "/" + dstSub;
        std::string cpCmd = "cp -r " + src + "/* " + dst + "/ 2>/dev/null";
        system(cpCmd.c_str());
    };

    copyDir("assets/sprites", "assets/sprites");
    copyDir("assets/sounds", "assets/sounds");
    copyDir("assets/music", "assets/music");
    copyDir("assets/scripts", "assets/scripts");
    copyDir("data", "data");
    copyDir("scenarios", "scenarios");
    copyDir("ui/menus", "ui/menus");

    // Generate launch script
    std::string script = GenerateLaunchScript();
    std::string lp = dest + "/launch.sh";
    std::ofstream lf(lp);
    lf << script;
    lf.close();
    chmod(lp.c_str(), 0755);

    // Version stamp
    std::string vp = dest + "/VERSION";
    std::ofstream vf(vp);
    vf << buildVersion_ << "\n";
    vf.close();

    Log("Build complete: " + dest);
    Log("  Run with: " + dest + "/launch.sh");
}

} // namespace beigebox
