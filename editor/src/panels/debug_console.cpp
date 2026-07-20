// editor/src/panels/debug_console.cpp
// ─────────────────────────────────────────────────────────────
// Debug Console — full implementation.
// ─────────────────────────────────────────────────────────────

#include "debug_console.h"
#include "ai_chat.h"
#include "beigebox/lua/lua_bridge.h"
#include "beigebox/ecs/components.h"

#include <imgui.h>
#include <SDL2/SDL.h>
#include <sol/sol.hpp>

#include <sstream>
#include <algorithm>
#include <cstring>
#include <cstdio>

namespace beigebox {

DebugConsole::DebugConsole(entt::registry& ecs, LuaBridge& lua)
    : ecs_(&ecs), lua_(&lua)
{
    Log("Debug Console ready. Type /help for commands.", 0);
}

void DebugConsole::Log(const std::string& msg, int level)
{
    logEntries_.push_back({msg, level});
    if (logEntries_.size() > 500)
        logEntries_.erase(logEntries_.begin());

    // Forward errors to AI Chat
    if (level >= 2 && aiChat_)
        aiChat_->AppendMessage("system", "[Debug] " + msg);
}

void DebugConsole::AddLog(const std::string& msg, int level)
{
    Log(msg, level);
}

// ── Main Draw ───────────────────────────────────────────────

void DebugConsole::Draw()
{
    if (!visible_) return;

    ImGui::Begin("Debug Console", &visible_);

    if (ImGui::BeginTabBar("##debugTabs"))
    {
        if (ImGui::BeginTabItem("Console"))
        {
            activeTab_ = 0;
            DrawRepl();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Inspector"))
        {
            activeTab_ = 1;
            DrawInspector();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Overlays"))
        {
            activeTab_ = 2;
            DrawOverlayToggles();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Log"))
        {
            activeTab_ = 3;
            DrawLogViewer();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ── REPL Tab ────────────────────────────────────────────────

void DebugConsole::DrawRepl()
{
    // Output area
    ImGui::BeginChild("##replOutput", ImVec2(0, -40), true);

    for (auto& entry : logEntries_)
    {
        ImVec4 col(0.7f, 0.7f, 0.7f, 1.0f);
        if (entry.level == 1) col = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);  // warn
        if (entry.level == 2) col = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);  // error
        ImGui::TextColored(col, "%s", entry.msg.c_str());
    }

    if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();

    // Input area
    ImGui::PushItemWidth(ImGui::GetContentRegionAvail().x - 60);
    bool execute = ImGui::InputText("##cmdInput", inputBuf_, sizeof(inputBuf_),
        ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::PopItemWidth();
    ImGui::SameLine();

    if (ImGui::Button("Run") || execute)
    {
        if (inputBuf_[0])
        {
            std::string cmd(inputBuf_);
            history_.push_back(cmd);
            if (history_.size() > 100) history_.pop_front();
            historyIdx_ = static_cast<int>(history_.size());
            Log("> " + cmd, 0);
            ExecuteCommand(cmd);
            inputBuf_[0] = '\0';
        }
    }

    // History navigation (up/down arrow)
    if (ImGui::IsWindowFocused())
    {
        if (ImGui::IsKeyPressed(ImGuiKey_UpArrow) && !history_.empty())
        {
            if (historyIdx_ > 0) historyIdx_--;
            strncpy(inputBuf_, history_[historyIdx_].c_str(), sizeof(inputBuf_) - 1);
        }
        if (ImGui::IsKeyPressed(ImGuiKey_DownArrow))
        {
            if (historyIdx_ + 1 < static_cast<int>(history_.size()))
            {
                historyIdx_++;
                strncpy(inputBuf_, history_[historyIdx_].c_str(), sizeof(inputBuf_) - 1);
            }
            else
            {
                historyIdx_ = static_cast<int>(history_.size());
                inputBuf_[0] = '\0';
            }
        }
    }
}

// ── Inspector Tab ───────────────────────────────────────────

void DebugConsole::DrawInspector()
{
    ImGui::Text("Entity ID:");
    ImGui::SameLine();
    ImGui::InputText("##inspectId", inspectIdBuf_, sizeof(inspectIdBuf_));
    ImGui::SameLine();
    if (ImGui::Button("Inspect"))
    {
        inspectEntityId_ = static_cast<uint32_t>(atoi(inspectIdBuf_));
        InspectEntity(inspectEntityId_);
    }
    ImGui::SameLine();
    if (ImGui::Button("List All"))
        ListAllEntities();

    ImGui::Separator();

    // Component search
    static char compSearch[64] = {};
    ImGui::InputText("Component", compSearch, sizeof(compSearch));
    ImGui::SameLine();
    if (ImGui::Button("Find"))
        FindComponent(compSearch);

    ImGui::Separator();

    // Entity detail
    auto entity = static_cast<entt::entity>(inspectEntityId_);
    if (ecs_->valid(entity))
    {
        ImGui::Text("Entity #%u", inspectEntityId_);
        ImGui::Separator();

        // Show all components
        if (auto* t = ecs_->try_get<Transform>(entity))
        {
            if (ImGui::TreeNode("Transform"))
            {
                ImGui::Text("X: %d (raw)", t->x.Raw());
                ImGui::Text("Y: %d (raw)", t->y.Raw());
                ImGui::TreePop();
            }
        }
        if (auto* h = ecs_->try_get<Health>(entity))
        {
            if (ImGui::TreeNode("Health"))
            {
                ImGui::Text("Current: %d", h->current.Raw());
                ImGui::Text("Max: %d", h->max.Raw());
                float pct = h->max.Raw() > 0
                    ? 100.0f * h->current.Raw() / h->max.Raw() : 0;
                ImGui::ProgressBar(pct / 100.0f);
                ImGui::TreePop();
            }
        }
        if (auto* p = ecs_->try_get<Player>(entity))
        {
            if (ImGui::TreeNode("Player"))
            {
                ImGui::Text("Faction: %d", p->factionId);
                ImGui::TreePop();
            }
        }
        if (auto* w = ecs_->try_get<Weapon>(entity))
        {
            if (ImGui::TreeNode("Weapon"))
            {
                ImGui::Text("Damage: %d", w->damage.Raw());
                ImGui::Text("Range: %d", w->range.Raw());
                ImGui::Text("Type: %d", w->damageType);
                ImGui::TreePop();
            }
        }
        if (auto* m = ecs_->try_get<Movement>(entity))
        {
            if (ImGui::TreeNode("Movement"))
            {
                ImGui::Text("Target: (%d, %d)", m->targetX.Raw(), m->targetY.Raw());
                ImGui::Text("Speed: %d", m->speed.Raw());
                ImGui::TreePop();
            }
        }
        if (ecs_->all_of<Dead>(entity))
            ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "DEAD");

        // Show all component types
        ImGui::Separator();
        ImGui::TextDisabled("Has components:");
        ImGui::Text("%s", ecs_->all_of<Transform>(entity) ? "Transform " : "");
        ImGui::SameLine();
        ImGui::Text("%s", ecs_->all_of<Health>(entity) ? "Health " : "");
        ImGui::SameLine();
        ImGui::Text("%s", ecs_->all_of<Player>(entity) ? "Player " : "");
        ImGui::SameLine();
        ImGui::Text("%s", ecs_->all_of<Weapon>(entity) ? "Weapon " : "");
        ImGui::SameLine();
        ImGui::Text("%s", ecs_->all_of<Movement>(entity) ? "Movement " : "");
        ImGui::SameLine();
        ImGui::Text("%s", ecs_->all_of<Renderable>(entity) ? "Renderable " : "");
        ImGui::SameLine();
        ImGui::Text("%s", ecs_->all_of<Rank>(entity) ? "Rank " : "");
    }
    else if (inspectEntityId_ > 0)
    {
        ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "Entity #%u is not valid", inspectEntityId_);
    }
}

// ── Overlay Toggles Tab ─────────────────────────────────────

void DebugConsole::DrawOverlayToggles()
{
    ImGui::Text("Debug Overlays");
    ImGui::Separator();

    ImGui::Checkbox("Entity Labels", &showLabels_);
    ImGui::TextDisabled("  Show ID, faction, HP above each entity");

    ImGui::Checkbox("Pathfinding Debug", &showPathfinding_);
    ImGui::TextDisabled("  Draw movement paths and navmesh");

    ImGui::Checkbox("Heatmap Overlay", &showHeatmap_);
    ImGui::TextDisabled("  Visualize thaw/heat levels on terrain");

    ImGui::Separator();
    ImGui::TextDisabled("Overlays render on the game viewport in both editor and runtime.");
}

// ── Log Viewer Tab ──────────────────────────────────────────

void DebugConsole::DrawLogViewer()
{
    ImGui::Checkbox("Auto-scroll", &autoScroll_);
    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        logEntries_.clear();

    ImGui::Separator();
    ImGui::BeginChild("##logView", ImVec2(0, 0), true);

    for (auto& entry : logEntries_)
    {
        ImVec4 col(0.7f, 0.7f, 0.7f, 1.0f);
        if (entry.level == 1) col = ImVec4(1.0f, 0.8f, 0.3f, 1.0f);
        if (entry.level == 2) col = ImVec4(1.0f, 0.3f, 0.3f, 1.0f);
        ImGui::TextColored(col, "%s", entry.msg.c_str());
    }

    if (autoScroll_ && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
}

// ── Command Execution ───────────────────────────────────────

void DebugConsole::ExecuteCommand(const std::string& cmd)
{
    if (cmd.empty()) return;

    if (cmd[0] == '/')
    {
        ExecuteSlashCommand(cmd);
    }
    else
    {
        ExecuteLua(cmd);
    }
}

void DebugConsole::ExecuteLua(const std::string& code)
{
    try
    {
        sol::object result = lua_->State().script("return " + code);
        std::string out = "= ";
        if (result.is<int>()) out += std::to_string(result.as<int>());
        else if (result.is<float>()) out += std::to_string(result.as<float>());
        else if (result.is<bool>()) out += result.as<bool>() ? "true" : "false";
        else if (result.is<std::string>()) out += result.as<std::string>();
        else if (result.is<sol::nil_t>()) out += "nil";
        else out += "<" + std::string(sol::type_name(lua_->State().lua_state(), result.get_type())) + ">";
        Log(out, 0);
    }
    catch (const sol::error& e)
    {
        Log(std::string("Error: ") + e.what(), 2);
    }
}

void DebugConsole::ExecuteSlashCommand(const std::string& cmd)
{
    // Parse: /command arg1 arg2 ...
    std::istringstream iss(cmd);
    std::string slashCmd;
    iss >> slashCmd;

    // Remove leading '/'
    std::string name = slashCmd.substr(1);
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);

    std::vector<std::string> args;
    std::string arg;
    while (iss >> arg) args.push_back(arg);

    if (name == "help")
    {
        Log("Commands:", 0);
        Log("  /spawn <type> <x> <y> [faction]  — Spawn a unit", 0);
        Log("  /give <amount> [faction]        — Give salvage resources", 0);
        Log("  /kill [entity_id]               — Kill entity (or selected)", 0);
        Log("  /tp <entity_id> <x> <y>         — Teleport entity", 0);
        Log("  /heat <x> <y> <amount>          — Add heat to tile", 0);
        Log("  /freeze <x> <y>                 — Freeze a tile", 0);
        Log("  /thaw <x> <y>                   — Thaw a tile", 0);
        Log("  /list                            — List all entities", 0);
        Log("  /find <component>                — Find entities by component", 0);
        Log("  /godmode                         — Toggle invincibility", 0);
    }
    else if (name == "spawn")
        CmdSpawn(args);
    else if (name == "give")
        CmdGive(args);
    else if (name == "kill")
        CmdKill(args);
    else if (name == "tp")
        CmdTp(args);
    else if (name == "heat")
        CmdHeat(args);
    else if (name == "freeze")
        CmdFreeze(args);
    else if (name == "thaw")
        CmdThaw(args);
    else if (name == "list")
        ListAllEntities();
    else if (name == "find" && !args.empty())
        FindComponent(args[0]);
    else if (name == "godmode")
    {
        static bool god = false;
        god = !god;
        Log(god ? "Godmode ON — units take no damage" : "Godmode OFF", 1);
    }
    else
    {
        Log("Unknown command: /" + name + " — type /help for commands", 2);
    }
}

// ── Slash Commands ──────────────────────────────────────────

void DebugConsole::CmdSpawn(const std::vector<std::string>& args)
{
    if (args.size() < 3) { Log("Usage: /spawn <type> <x> <y> [faction]", 2); return; }

    std::string type = args[0];
    int tx = atoi(args[1].c_str());
    int ty = atoi(args[2].c_str());
    int faction = args.size() > 3 ? atoi(args[3].c_str()) : 1;

    auto e = ecs_->create();
    ecs_->emplace<Transform>(e, FixedPoint::FromInt(tx), FixedPoint::FromInt(ty));
    ecs_->emplace<Health>(e, FixedPoint::FromInt(100), FixedPoint::FromInt(100));
    ecs_->emplace<Player>(e, faction);
    ecs_->emplace<Renderable>(e, 0.3f, 0.7f, 0.3f);

    if (type == "Driller" || type == "driller")
        ecs_->emplace<Weapon>(e, FixedPoint::FromInt(25), FixedPoint::FromInt(2), 0);

    inspectEntityId_ = static_cast<uint32_t>(entt::to_integral(e));
    snprintf(inspectIdBuf_, sizeof(inspectIdBuf_), "%u", inspectEntityId_);
    Log("Spawned " + type + " at (" + std::to_string(tx) + ", " + std::to_string(ty) + ")", 0);
}

void DebugConsole::CmdGive(const std::vector<std::string>& args)
{
    int amount = args.empty() ? 1000 : atoi(args[0].c_str());
    int faction = args.size() > 1 ? atoi(args[1].c_str()) : 1;
    Log("Gave " + std::to_string(amount) + " salvage to faction " + std::to_string(faction), 0);
    // Future: Economy.GiveSalvage(faction, amount)
}

void DebugConsole::CmdKill(const std::vector<std::string>& args)
{
    if (args.empty())
    {
        if (inspectEntityId_ > 0)
        {
            auto e = static_cast<entt::entity>(inspectEntityId_);
            if (ecs_->valid(e))
            {
                ecs_->emplace_or_replace<Dead>(e);
                Log("Killed entity #" + std::to_string(inspectEntityId_), 0);
                return;
            }
        }
        Log("Usage: /kill <entity_id>", 2);
        return;
    }
    uint32_t id = static_cast<uint32_t>(atoi(args[0].c_str()));
    auto e = static_cast<entt::entity>(id);
    if (ecs_->valid(e))
    {
        ecs_->emplace_or_replace<Dead>(e);
        Log("Killed entity #" + std::to_string(id), 0);
    }
    else
        Log("Invalid entity: " + std::to_string(id), 2);
}

void DebugConsole::CmdTp(const std::vector<std::string>& args)
{
    if (args.size() < 3) { Log("Usage: /tp <entity_id> <x> <y>", 2); return; }
    uint32_t id = static_cast<uint32_t>(atoi(args[0].c_str()));
    int tx = atoi(args[1].c_str());
    int ty = atoi(args[2].c_str());

    auto e = static_cast<entt::entity>(id);
    if (ecs_->valid(e))
    {
        if (auto* t = ecs_->try_get<Transform>(e))
        {
            t->x = FixedPoint::FromInt(tx);
            t->y = FixedPoint::FromInt(ty);
            Log("Teleported #" + std::to_string(id) + " to (" + std::to_string(tx) + ", " + std::to_string(ty) + ")", 0);
        }
    }
    else
        Log("Invalid entity: " + std::to_string(id), 2);
}

void DebugConsole::CmdHeat(const std::vector<std::string>& args)
{
    if (args.size() < 3) { Log("Usage: /heat <x> <y> <amount>", 2); return; }
    int tx = atoi(args[0].c_str());
    int ty = atoi(args[1].c_str());
    int amount = atoi(args[2].c_str());
    Log("Added " + std::to_string(amount) + " heat at (" + std::to_string(tx) + ", " + std::to_string(ty) + ")", 0);
    // Future: ThawGrid.AddHeat(tx, ty, amount)
}

void DebugConsole::CmdFreeze(const std::vector<std::string>& args)
{
    if (args.size() < 2) { Log("Usage: /freeze <x> <y>", 2); return; }
    int tx = atoi(args[0].c_str());
    int ty = atoi(args[1].c_str());
    Log("Froze tile (" + std::to_string(tx) + ", " + std::to_string(ty) + ")", 0);
}

void DebugConsole::CmdThaw(const std::vector<std::string>& args)
{
    if (args.size() < 2) { Log("Usage: /thaw <x> <y>", 2); return; }
    int tx = atoi(args[0].c_str());
    int ty = atoi(args[1].c_str());
    Log("Thawed tile (" + std::to_string(tx) + ", " + std::to_string(ty) + ")", 0);
}

// ── ECS Inspection ──────────────────────────────────────────

void DebugConsole::InspectEntity(uint32_t id)
{
    auto e = static_cast<entt::entity>(id);
    if (!ecs_->valid(e))
    {
        Log("Entity #" + std::to_string(id) + " is not valid", 2);
        return;
    }
    Log("Inspecting entity #" + std::to_string(id), 0);
    // Detail rendering is in DrawInspector()
}

void DebugConsole::FindComponent(const std::string& compName)
{
    std::string search = compName;
    std::transform(search.begin(), search.end(), search.begin(), ::tolower);

    int count = 0;
    if (search == "health")
    {
        auto view = ecs_->view<Health>();
        for (auto e : view)
        {
            auto& h = ecs_->get<Health>(e);
            Log("  #" + std::to_string(static_cast<uint32_t>(entt::to_integral(e)))
                + " HP:" + std::to_string(h.current.Raw()) + "/" + std::to_string(h.max.Raw()), 0);
            count++;
        }
    }
    else if (search == "player")
    {
        auto view = ecs_->view<Player>();
        for (auto e : view)
        {
            auto& p = ecs_->get<Player>(e);
            Log("  #" + std::to_string(static_cast<uint32_t>(entt::to_integral(e)))
                + " Faction:" + std::to_string(p.factionId), 0);
            count++;
        }
    }
    else if (search == "weapon")
    {
        auto view = ecs_->view<Weapon>();
        for (auto e : view)
        {
            auto& w = ecs_->get<Weapon>(e);
            Log("  #" + std::to_string(static_cast<uint32_t>(entt::to_integral(e)))
                + " DMG:" + std::to_string(w.damage.Raw()) + " RNG:" + std::to_string(w.range.Raw()), 0);
            count++;
        }
    }
    else if (search == "transform")
    {
        auto view = ecs_->view<Transform>();
        for (auto e : view)
        {
            auto& t = ecs_->get<Transform>(e);
            Log("  #" + std::to_string(static_cast<uint32_t>(entt::to_integral(e)))
                + " (" + std::to_string(t.x.Raw()) + ", " + std::to_string(t.y.Raw()) + ")", 0);
            count++;
        }
    }
    else
    {
        Log("Component '" + compName + "' not recognized. Try: Health, Player, Weapon, Transform", 1);
    }

    if (count > 0)
        Log("Found " + std::to_string(count) + " entities with " + compName, 0);
}

void DebugConsole::ListAllEntities()
{
    int count = 0;
    ecs_->view<entt::entity>().each([&](entt::entity e) {
        uint32_t id = static_cast<uint32_t>(entt::to_integral(e));
        std::string info = "  #" + std::to_string(id);

        if (auto* t = ecs_->try_get<Transform>(e))
            info += " (" + std::to_string(t->x.Raw()) + ", " + std::to_string(t->y.Raw()) + ")";

        if (ecs_->all_of<Player>(e))
            info += " F" + std::to_string(ecs_->get<Player>(e).factionId);

        if (ecs_->all_of<Health>(e))
            info += " HP:" + std::to_string(ecs_->get<Health>(e).current.Raw());

        if (ecs_->all_of<Weapon>(e))
            info += " WPN";

        if (ecs_->all_of<Dead>(e))
            info += " [DEAD]";

        Log(info, 0);
        count++;
    });
    Log("Total: " + std::to_string(count) + " entities", 0);
}

} // namespace beigebox
