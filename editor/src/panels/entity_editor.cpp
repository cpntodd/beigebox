// editor/src/panels/entity_editor.cpp
// ─────────────────────────────────────────────────────────────
// Entity Editor — unit/building/hero designer + balance sheet.
// ─────────────────────────────────────────────────────────────

#include "entity_editor.h"
#include "ai_chat.h"
#include "beigebox/lua/lua_bridge.h"
#include "beigebox/render/renderer.h"
#include "beigebox/world/thaw_grid.h"

#include <imgui.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

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
// EntityTemplate serialization
// ═════════════════════════════════════════════════════════════

void EntityTemplate::ToJson(json& j) const
{
    j["name"]     = name;
    j["category"] = category;
    j["stats"]["hp"]         = hp;
    j["stats"]["armor"]      = armor;
    j["stats"]["speed"]      = speed;
    j["stats"]["sight"]      = sight;
    j["stats"]["cost"]       = cost;
    j["stats"]["buildTime"]  = buildTime;
    j["stats"]["population"] = population;

    j["sprite"]      = sprite;
    j["sounds"]["move"]   = soundMove;
    j["sounds"]["attack"] = soundAttack;
    j["sounds"]["death"]  = soundDeath;
    j["sounds"]["select"] = soundSelect;

    json weaponsArr = json::array();
    for (auto& w : weapons)
    {
        json wj;
        wj["name"]       = w.name;
        wj["damage"]     = w.damage;
        wj["range"]      = w.range;
        wj["attackRate"] = w.attackRate;
        wj["damageType"] = w.damageType;
        weaponsArr.push_back(wj);
    }
    j["weapons"] = weaponsArr;

    j["abilities"]     = abilities;
    j["abilityParams"] = abilityParams;

    j["techRequired"] = techRequired;
    j["builtFrom"]    = builtFrom;
    j["faction"]      = faction;
    j["veterancyLevel"] = veterancyLevel;
    j["personality"]  = personality;

    if (!superweaponType.empty())
    {
        j["superweapon"]["type"]     = superweaponType;
        j["superweapon"]["cooldown"] = superweaponCooldown;
    }
}

void EntityTemplate::FromJson(const json& j)
{
    name     = j.value("name", "Unknown");
    category = j.value("category", "units");

    if (j.contains("stats"))
    {
        auto& s = j["stats"];
        hp         = s.value("hp", 100);
        armor      = s.value("armor", 0);
        speed      = s.value("speed", 2);
        sight      = s.value("sight", 6);
        cost       = s.value("cost", 100);
        buildTime  = s.value("buildTime", 15);
        population = s.value("population", 1);
    }

    sprite = j.value("sprite", "");
    if (j.contains("sounds"))
    {
        auto& snd = j["sounds"];
        soundMove   = snd.value("move", "");
        soundAttack = snd.value("attack", "");
        soundDeath  = snd.value("death", "");
        soundSelect = snd.value("select", "");
    }

    weapons.clear();
    if (j.contains("weapons"))
    {
        for (auto& wj : j["weapons"])
        {
            WeaponDef w;
            w.name       = wj.value("name", "Weapon");
            w.damage     = wj.value("damage", 25);
            w.range      = wj.value("range", 2);
            w.attackRate = wj.value("attackRate", 1.0f);
            w.damageType = wj.value("damageType", 0);
            weapons.push_back(w);
        }
    }

    abilities = j.value("abilities", std::vector<std::string>{});
    abilityParams = j.value("abilityParams", json::object());

    techRequired = j.value("techRequired", "");
    builtFrom    = j.value("builtFrom", "");
    faction      = j.value("faction", 0);
    veterancyLevel = j.value("veterancyLevel", 0);
    personality  = j.value("personality", "Aggressive");

    if (j.contains("superweapon"))
    {
        superweaponType     = j["superweapon"].value("type", "");
        superweaponCooldown = j["superweapon"].value("cooldown", 120);
    }
}

// ═════════════════════════════════════════════════════════════
// EntityEditor implementation
// ═════════════════════════════════════════════════════════════

EntityEditor::EntityEditor(LuaBridge& lua, Renderer& renderer, ThawGrid& thaw)
    : lua_(&lua), renderer_(&renderer), thaw_(&thaw)
{
    LoadAbilityRegistry();
    ScanCategories();
    LoadAllTemplates();
}

// ── Helpers ─────────────────────────────────────────────────

void EntityEditor::Log(const std::string& msg)
{
    if (aiChat_) aiChat_->AppendMessage("system", "[Entity] " + msg);
    SDL_Log("[Entity] %s", msg.c_str());
}

std::string EntityEditor::DataDir() const
{
    return rootPath_ + "/data";
}

std::vector<std::string> EntityEditor::UnitTypeNames() const
{
    std::vector<std::string> names;
    for (auto& t : templates_)
        names.push_back(t.name);
    return names;
}

// ── Ability Registry ────────────────────────────────────────

void EntityEditor::LoadAbilityRegistry()
{
    abilityRegistry_.clear();

    // Built-in ability definitions
    // Each ability maps to a set of ECS component names
    // When a unit with this ability spawns, those components are attached

    abilityRegistry_.push_back({
        "ThawAura",
        "Passive heat aura that thaws frozen terrain",
        {"HeatSource"},
        {{"radius", 3}, {"intensity", 10}}
    });
    abilityRegistry_.push_back({
        "Cloak",
        "Unit becomes invisible to enemies not in range",
        {},  // Cloak component TBD
        {{"drainRate", 2.0f}}
    });
    abilityRegistry_.push_back({
        "Repair",
        "Can repair nearby friendly units and buildings",
        {},  // Repair component TBD
        {{"rate", 5.0f}, {"range", 2}}
    });
    abilityRegistry_.push_back({
        "Build",
        "Can construct buildings",
        {},  // Builder component TBD
        {{"speed", 1.0f}}
    });
    abilityRegistry_.push_back({
        "Teleport",
        "Short-range teleport with cooldown",
        {},  // Teleport component TBD
        {{"range", 8}, {"cooldown", 30}}
    });
    abilityRegistry_.push_back({
        "Heal",
        "Heals nearby friendly organic units",
        {},  // HealAura component TBD
        {{"amount", 5}, {"range", 3}, {"rate", 1.0f}}
    });
}

// ── Data Scanning ───────────────────────────────────────────

void EntityEditor::ScanCategories()
{
    categories_ = {"units", "buildings", "heroes", "projectiles"};
}

void EntityEditor::LoadAllTemplates()
{
    templates_.clear();
    std::string dataDir = DataDir();

    for (auto& cat : categories_)
    {
        std::string catDir = dataDir + "/" + cat;
        mkdir(catDir.c_str(), 0755);

        DIR* d = opendir(catDir.c_str());
        if (!d) continue;

        struct dirent* entry;
        while ((entry = readdir(d)) != nullptr)
        {
            std::string name(entry->d_name);
            if (name == "." || name == "..") continue;
            if (entry->d_type != DT_REG) continue;
            if (name.size() < 5 || name.substr(name.size() - 5) != ".json") continue;

            std::string filePath = catDir + "/" + name;
            std::ifstream f(filePath);
            if (!f.good()) continue;

            try
            {
                json j = json::parse(f);
                EntityTemplate tmpl;
                tmpl.FromJson(j);
                tmpl.filePath = "data/" + cat + "/" + name;
                templates_.push_back(tmpl);
            }
            catch (const std::exception& e)
            {
                Log(std::string("Error loading ") + filePath + ": " + e.what());
            }
        }
        closedir(d);
    }

    // Sort by category then name
    std::sort(templates_.begin(), templates_.end(),
        [](const EntityTemplate& a, const EntityTemplate& b) {
            if (a.category != b.category) return a.category < b.category;
            return a.name < b.name;
        });

    Log("Loaded " + std::to_string(templates_.size()) + " templates");
}

void EntityEditor::SaveTemplate(const EntityTemplate& tmpl)
{
    json j;
    tmpl.ToJson(j);

    std::string path = rootPath_ + "/" + tmpl.filePath;
    std::ofstream f(path);
    if (f.good())
    {
        f << j.dump(2);
        Log("Saved: " + tmpl.filePath);
    }
    else
    {
        Log("Error writing: " + path);
    }
}

void EntityEditor::DeleteTemplate(const std::string& filePath)
{
    std::string path = rootPath_ + "/" + filePath;
    if (remove(path.c_str()) == 0)
        Log("Deleted: " + filePath);
}

// ═════════════════════════════════════════════════════════════
// Main Draw
// ═════════════════════════════════════════════════════════════

void EntityEditor::Draw()
{
    ImGui::Begin("Entity Editor");

    if (ImGui::BeginTabBar("##entityTabs"))
    {
        if (ImGui::BeginTabItem("Designer"))
        {
            DrawDesignerTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Balance Sheet"))
        {
            DrawSpreadsheetTab();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ── Designer Tab ────────────────────────────────────────────

void EntityEditor::DrawDesignerTab()
{
    // ── Toolbar ──────────────────────────────────────────────
    if (ImGui::Button("New"))
        showNewDialog_ = true;
    ImGui::SameLine();
    if (ImGui::Button("Reload"))
        LoadAllTemplates();
    ImGui::SameLine();

    ImGui::SetNextItemWidth(120);
    ImGui::InputTextWithHint("##search", "Search...", searchBuf_, sizeof(searchBuf_));
    ImGui::SameLine();

    ImGui::SetNextItemWidth(120);
    if (ImGui::BeginCombo("##catFilter",
            selectedCategory_ < static_cast<int>(categories_.size())
                ? categories_[selectedCategory_].c_str() : "All"))
    {
        if (ImGui::Selectable("All", selectedCategory_ == -1))
            selectedCategory_ = -1;
        for (int i = 0; i < static_cast<int>(categories_.size()); ++i)
            if (ImGui::Selectable(categories_[i].c_str(), selectedCategory_ == i))
                selectedCategory_ = i;
        ImGui::EndCombo();
    }

    ImGui::Separator();

    // ── Two-column layout ────────────────────────────────────
    float listWidth = 200;
    ImGui::BeginChild("##templateList", ImVec2(listWidth, 0), true);

    for (int i = 0; i < static_cast<int>(templates_.size()); ++i)
    {
        auto& tmpl = templates_[i];

        // Category filter
        if (selectedCategory_ >= 0 &&
            tmpl.category != categories_[selectedCategory_])
            continue;

        // Search filter
        if (searchBuf_[0])
        {
            std::string nl = tmpl.name;
            std::string sl = searchBuf_;
            std::transform(nl.begin(), nl.end(), nl.begin(), ::tolower);
            std::transform(sl.begin(), sl.end(), sl.begin(), ::tolower);
            if (nl.find(sl) == std::string::npos) continue;
        }

        std::string label = tmpl.name + " [" + tmpl.category + "]";
        if (ImGui::Selectable(label.c_str(), activeTemplate_ == i))
            activeTemplate_ = i;

        // Right-click delete
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Delete"))
            {
                DeleteTemplate(tmpl.filePath);
                templates_.erase(templates_.begin() + i);
                if (activeTemplate_ >= static_cast<int>(templates_.size()))
                    activeTemplate_ = static_cast<int>(templates_.size()) - 1;
                ImGui::EndPopup();
                ImGui::EndChild();
                return;
            }
            if (ImGui::MenuItem("Duplicate"))
            {
                EntityTemplate dup = tmpl;
                dup.name += "_copy";
                dup.filePath = "data/" + dup.category + "/" + dup.name + ".json";
                SaveTemplate(dup);
                templates_.push_back(dup);
            }
            ImGui::EndPopup();
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();

    // ── Editor pane ──────────────────────────────────────────
    ImGui::BeginChild("##editorPane", ImVec2(0, 0), false);

    if (activeTemplate_ >= 0 && activeTemplate_ < static_cast<int>(templates_.size()))
    {
        DrawTemplateEditor(templates_[activeTemplate_]);
    }
    else
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
            "Select a template from the list, or create a New one.\n\n"
            "Templates are stored in data/{units,buildings,heroes,projectiles}/");
    }

    ImGui::EndChild();

    // ── New Template Dialog ──────────────────────────────────
    if (showNewDialog_)
    {
        ImGui::OpenPopup("New Template");
        if (ImGui::BeginPopupModal("New Template", &showNewDialog_))
        {
            ImGui::InputText("Name", newNameBuf_, sizeof(newNameBuf_));
            static int catIdx = 0;
            ImGui::Combo("Category", &catIdx,
                [](void*, int idx, const char** out) {
                    *out = idx == 0 ? "units" : idx == 1 ? "buildings" :
                           idx == 2 ? "heroes" : "projectiles";
                    return true;
                }, nullptr, 4);

            if (ImGui::Button("Create", ImVec2(100, 0)) && newNameBuf_[0])
            {
                EntityTemplate tmpl;
                tmpl.name     = newNameBuf_;
                tmpl.category = (catIdx == 0 ? "units" : catIdx == 1 ? "buildings" :
                                 catIdx == 2 ? "heroes" : "projectiles");
                tmpl.filePath = "data/" + tmpl.category + "/" + tmpl.name + ".json";
                SaveTemplate(tmpl);
                templates_.push_back(tmpl);
                activeTemplate_ = static_cast<int>(templates_.size()) - 1;
                showNewDialog_ = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0)))
                showNewDialog_ = false;
            ImGui::EndPopup();
        }
    }
}

// ── Template Editor ─────────────────────────────────────────

void EntityEditor::DrawTemplateEditor(EntityTemplate& tmpl)
{
    ImGui::Text("Editing: %s", tmpl.name.c_str());
    ImGui::SameLine();
    ImGui::TextDisabled("[%s]", tmpl.category.c_str());
    ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
    if (ImGui::Button("Save"))
        SaveTemplate(tmpl);

    ImGui::Separator();

    if (ImGui::BeginTabBar("##tmplTabs"))
    {
        if (ImGui::BeginTabItem("Stats"))
        {
            DrawStatsSection(tmpl);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Weapons"))
        {
            DrawWeaponsSection(tmpl);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Abilities"))
        {
            DrawAbilitiesSection(tmpl);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Preview"))
        {
            DrawPreviewViewport(tmpl);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
}

// ── Stats Section ───────────────────────────────────────────

void EntityEditor::DrawStatsSection(EntityTemplate& tmpl)
{
    ImGui::InputInt("HP", &tmpl.hp);
    ImGui::InputInt("Armor", &tmpl.armor);
    ImGui::InputInt("Speed", &tmpl.speed);
    ImGui::InputInt("Sight Range", &tmpl.sight);
    ImGui::Separator();
    ImGui::InputInt("Cost", &tmpl.cost);
    ImGui::InputInt("Build Time (ticks)", &tmpl.buildTime);
    ImGui::InputInt("Population", &tmpl.population);
    ImGui::Separator();

    ImGui::InputText("Sprite", &tmpl.sprite[0], tmpl.sprite.size() + 1);
    if (tmpl.sprite.size() < 128) tmpl.sprite.resize(128);

    ImGui::InputText("Sound: Move", &tmpl.soundMove[0], tmpl.soundMove.size() + 1);
    if (tmpl.soundMove.size() < 128) tmpl.soundMove.resize(128);

    ImGui::InputText("Sound: Attack", &tmpl.soundAttack[0], tmpl.soundAttack.size() + 1);
    if (tmpl.soundAttack.size() < 128) tmpl.soundAttack.resize(128);

    ImGui::InputText("Sound: Death", &tmpl.soundDeath[0], tmpl.soundDeath.size() + 1);
    if (tmpl.soundDeath.size() < 128) tmpl.soundDeath.resize(128);

    ImGui::InputText("Sound: Select", &tmpl.soundSelect[0], tmpl.soundSelect.size() + 1);
    if (tmpl.soundSelect.size() < 128) tmpl.soundSelect.resize(128);

    ImGui::Separator();

    ImGui::InputText("Built From", &tmpl.builtFrom[0], tmpl.builtFrom.size() + 1);
    if (tmpl.builtFrom.size() < 64) tmpl.builtFrom.resize(64);

    ImGui::InputText("Tech Required", &tmpl.techRequired[0], tmpl.techRequired.size() + 1);
    if (tmpl.techRequired.size() < 64) tmpl.techRequired.resize(64);

    ImGui::Separator();

    ImGui::InputInt("Faction", &tmpl.faction);
    ImGui::InputInt("Veterancy Level", &tmpl.veterancyLevel);

    const char* personalities[] = {"Aggressive", "Defensive", "Explorer", "Coward"};
    int persIdx = 0;
    for (int i = 0; i < 4; ++i)
        if (tmpl.personality == personalities[i]) { persIdx = i; break; }
    if (ImGui::Combo("AI Personality", &persIdx, personalities, 4))
        tmpl.personality = personalities[persIdx];

    ImGui::Separator();

    // Superweapon
    static int swTypeIdx = 0;
    const char* swTypes[] = {"None", "Nuke", "OrbitalStrike", "EMP", "NaniteSwarm"};
    if (ImGui::Combo("Superweapon", &swTypeIdx, swTypes, 5))
    {
        if (swTypeIdx == 0)
            tmpl.superweaponType.clear();
        else
            tmpl.superweaponType = swTypes[swTypeIdx];
    }
    if (!tmpl.superweaponType.empty())
        ImGui::InputInt("SW Cooldown", &tmpl.superweaponCooldown);
}

// ── Weapons Section ─────────────────────────────────────────

void EntityEditor::DrawWeaponsSection(EntityTemplate& tmpl)
{
    if (ImGui::Button("+ Add Weapon"))
    {
        WeaponDef w;
        w.name = "Weapon_" + std::to_string(tmpl.weapons.size() + 1);
        tmpl.weapons.push_back(w);
    }

    int removeIdx = -1;
    for (int i = 0; i < static_cast<int>(tmpl.weapons.size()); ++i)
    {
        auto& w = tmpl.weapons[i];
        ImGui::PushID(i);
        ImGui::Separator();
        ImGui::Text("Weapon %d", i + 1);
        ImGui::InputText("Name", &w.name[0], w.name.size() + 1);
        if (w.name.size() < 64) w.name.resize(64);
        ImGui::InputInt("Damage", &w.damage);
        ImGui::InputInt("Range", &w.range);
        ImGui::InputFloat("Attack Rate (s)", &w.attackRate);
        ImGui::InputInt("Damage Type", &w.damageType);
        ImGui::SameLine();
        if (ImGui::SmallButton("Remove"))
            removeIdx = i;
        ImGui::PopID();
    }
    if (removeIdx >= 0)
        tmpl.weapons.erase(tmpl.weapons.begin() + removeIdx);
}

// ── Abilities Section ───────────────────────────────────────

void EntityEditor::DrawAbilitiesSection(EntityTemplate& tmpl)
{
    ImGui::Text("Ability Registry (check to add to this unit):");

    for (auto& ab : abilityRegistry_)
    {
        bool has = std::find(tmpl.abilities.begin(), tmpl.abilities.end(), ab.name)
                   != tmpl.abilities.end();
        if (ImGui::Checkbox(ab.name.c_str(), &has))
        {
            if (has)
                tmpl.abilities.push_back(ab.name);
            else
                tmpl.abilities.erase(
                    std::remove(tmpl.abilities.begin(), tmpl.abilities.end(), ab.name),
                    tmpl.abilities.end());
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("%s\nComponents: %zu",
                ab.description.c_str(), ab.components.size());

        // If checked, show parameters
        if (has && ab.defaultParams.is_object())
        {
            ImGui::Indent(20);
            for (auto& [key, val] : ab.defaultParams.items())
            {
                if (val.is_number_integer())
                {
                    int v = val.get<int>();
                    ImGui::InputInt(key.c_str(), &v);
                }
                else if (val.is_number_float())
                {
                    float v = val.get<float>();
                    ImGui::InputFloat(key.c_str(), &v);
                }
            }
            ImGui::Unindent(20);
        }
    }
}

// ── Preview Viewport ────────────────────────────────────────

void EntityEditor::DrawPreviewViewport(EntityTemplate& tmpl)
{
    ImGui::Text("Sandbox Preview — %s", tmpl.name.c_str());

    // Simple render: show a placeholder colored quad in a child window
    ImVec2 previewSize(sandboxW_, sandboxH_);
    ImGui::BeginChild("##sandbox", previewSize, true,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // Draw a dark grid background
    dl->AddRectFilled(p, ImVec2(p.x + sandboxW_, p.y + sandboxH_),
        IM_COL32(20, 20, 30, 255));

    // Draw some grid lines
    for (int x = 0; x < sandboxW_; x += 32)
        dl->AddLine(ImVec2(p.x + x, p.y), ImVec2(p.x + x, p.y + sandboxH_),
            IM_COL32(40, 40, 55, 100));
    for (int y = 0; y < sandboxH_; y += 32)
        dl->AddLine(ImVec2(p.x, p.y + y), ImVec2(p.x + sandboxW_, p.y + y),
            IM_COL32(40, 40, 55, 100));

    // Draw the unit as a colored diamond (isometric)
    float cx = p.x + sandboxW_ / 2.0f;
    float cy = p.y + sandboxH_ / 2.0f;
    float size = 20.0f;

    // Color based on category
    ImU32 col = IM_COL32(100, 200, 100, 255); // default green
    if (tmpl.category == "buildings") col = IM_COL32(200, 180, 80, 255);
    else if (tmpl.category == "heroes") col = IM_COL32(220, 150, 50, 255);
    else if (tmpl.category == "projectiles") col = IM_COL32(200, 100, 100, 255);

    dl->AddQuadFilled(
        ImVec2(cx, cy - size),           // top
        ImVec2(cx + size, cy),           // right
        ImVec2(cx, cy + size),           // bottom
        ImVec2(cx - size, cy),           // left
        col);

    dl->AddQuad(
        ImVec2(cx, cy - size),
        ImVec2(cx + size, cy),
        ImVec2(cx, cy + size),
        ImVec2(cx - size, cy),
        IM_COL32(255, 255, 255, 100), 1.5f);

    // Label
    dl->AddText(ImVec2(cx - 20, cy + size + 4),
        IM_COL32(200, 200, 200, 255), tmpl.name.c_str());

    ImGui::EndChild();

    ImGui::TextDisabled("Full FBO renderer integration available in Viewport panel.");
}

// ═════════════════════════════════════════════════════════════
// Balance Spreadsheet Tab
// ═════════════════════════════════════════════════════════════

void EntityEditor::DrawSpreadsheetTab()
{
    ImGui::Text("Balance Spreadsheet — all unit/building/hero stats");
    ImGui::Separator();

    if (templates_.empty())
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
            "No templates loaded. Create units in the Designer tab first.");
        return;
    }

    // Sort controls
    static int sortCol = 0;
    static bool sortAsc = true;
    const char* cols[] = {"Name", "Cat", "HP", "Armor", "Speed", "Sight",
                          "Cost", "Build", "Pop", "DPS", "Range", "Faction"};

    ImGui::SetNextItemWidth(150);
    ImGui::Combo("Sort by", &sortCol, cols, 12);
    ImGui::SameLine();
    ImGui::Checkbox("Ascending", &sortAsc);

    // Build sorted list
    std::vector<EntityTemplate*> sorted;
    for (auto& t : templates_) sorted.push_back(&t);

    std::sort(sorted.begin(), sorted.end(),
        [&](EntityTemplate* a, EntityTemplate* b) {
            float va = 0, vb = 0;
            switch (sortCol) {
                case 0: return sortAsc ? a->name < b->name : a->name > b->name;
                case 1: return sortAsc ? a->category < b->category : a->category > b->category;
                case 2: va = a->hp; vb = b->hp; break;
                case 3: va = a->armor; vb = b->armor; break;
                case 4: va = a->speed; vb = b->speed; break;
                case 5: va = a->sight; vb = b->sight; break;
                case 6: va = a->cost; vb = b->cost; break;
                case 7: va = a->buildTime; vb = b->buildTime; break;
                case 8: va = a->population; vb = b->population; break;
                case 9: va = a->weapons.empty() ? 0 : a->weapons[0].damage / a->weapons[0].attackRate;
                         vb = b->weapons.empty() ? 0 : b->weapons[0].damage / b->weapons[0].attackRate; break;
                case 10: va = a->weapons.empty() ? 0 : a->weapons[0].range;
                          vb = b->weapons.empty() ? 0 : b->weapons[0].range; break;
                case 11: va = a->faction; vb = b->faction; break;
            }
            return sortAsc ? va < vb : va > vb;
        });

    ImGui::Separator();

    // Table
    if (ImGui::BeginTable("##balanceSheet", 12,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
            ImGuiTableFlags_Sortable))
    {
        ImGui::TableSetupColumn("Name");
        ImGui::TableSetupColumn("Cat");
        ImGui::TableSetupColumn("HP");
        ImGui::TableSetupColumn("Armor");
        ImGui::TableSetupColumn("Speed");
        ImGui::TableSetupColumn("Sight");
        ImGui::TableSetupColumn("Cost");
        ImGui::TableSetupColumn("Build");
        ImGui::TableSetupColumn("Pop");
        ImGui::TableSetupColumn("DPS");
        ImGui::TableSetupColumn("Range");
        ImGui::TableSetupColumn("Faction");
        ImGui::TableHeadersRow();

        for (auto* tmpl : sorted)
        {
            ImGui::TableNextRow();
            ImGui::TableSetColumnIndex(0); ImGui::Text("%s", tmpl->name.c_str());
            ImGui::TableSetColumnIndex(1); ImGui::Text("%s", tmpl->category.c_str());
            ImGui::TableSetColumnIndex(2); ImGui::Text("%d", tmpl->hp);
            ImGui::TableSetColumnIndex(3); ImGui::Text("%d", tmpl->armor);
            ImGui::TableSetColumnIndex(4); ImGui::Text("%d", tmpl->speed);
            ImGui::TableSetColumnIndex(5); ImGui::Text("%d", tmpl->sight);
            ImGui::TableSetColumnIndex(6); ImGui::Text("%d", tmpl->cost);
            ImGui::TableSetColumnIndex(7); ImGui::Text("%d", tmpl->buildTime);
            ImGui::TableSetColumnIndex(8); ImGui::Text("%d", tmpl->population);
            ImGui::TableSetColumnIndex(9);
            float dps = tmpl->weapons.empty() ? 0 :
                tmpl->weapons[0].damage / tmpl->weapons[0].attackRate;
            ImGui::Text("%.1f", dps);
            ImGui::TableSetColumnIndex(10);
            ImGui::Text("%d", tmpl->weapons.empty() ? 0 : tmpl->weapons[0].range);
            ImGui::TableSetColumnIndex(11); ImGui::Text("%d", tmpl->faction);
        }

        ImGui::EndTable();
    }
}

} // namespace beigebox
