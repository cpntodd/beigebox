// editor/src/panels/event_debugger.cpp
// ─────────────────────────────────────────────────────────────
// Event Debugger Panel — trace viewer + breakpoints + inspector.
// ─────────────────────────────────────────────────────────────

#include "event_debugger.h"
#include "ai_chat.h"
#include "beigebox/ecs/components.h"

#include <imgui.h>
#include <SDL2/SDL.h>

namespace beigebox {

void EventDebugger::Log(const std::string& msg)
{
    if (aiChat_) aiChat_->AppendMessage("system", "[Debug] " + msg);
    SDL_Log("%s", msg.c_str());
}

void EventDebugger::Draw()
{
    ImGui::Begin("Event Debugger");

    if (ImGui::BeginTabBar("##debugTabs"))
    {
        if (ImGui::BeginTabItem("Trace Log"))
        {
            DrawTraceLog();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Breakpoints"))
        {
            DrawBreakpoints();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Inspector"))
        {
            DrawInspector();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ═════════════════════════════════════════════════════════════
// Trace Log Tab
// ═════════════════════════════════════════════════════════════

void EventDebugger::DrawTraceLog()
{
    bool tracing = lua_->IsTraceEnabled();
    if (ImGui::Checkbox("Enable Tracing", &tracing))
        lua_->SetTraceEnabled(tracing);

    ImGui::SameLine();
    if (ImGui::Button("Clear"))
        lua_->ClearTraces();

    ImGui::SameLine();
    ImGui::Text("(%zu events)", lua_->GetTraces().size());

    ImGui::Separator();

    if (ImGui::BeginTable("##tracelog", 4,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY,
        ImVec2(0, -1)))
    {
        ImGui::TableSetupColumn("Frame", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Entity", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthFixed, 120);
        ImGui::TableSetupColumn("Info", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        auto& traces = lua_->GetTraces();
        // Show newest at top
        for (auto it = traces.rbegin(); it != traces.rend(); ++it)
        {
            ImGui::TableNextRow();

            // Frame
            ImGui::TableSetColumnIndex(0);
            if (it->breakpoint)
                ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "%u ⏸", it->frame);
            else
                ImGui::Text("%u", it->frame);

            // Entity
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", it->entityId);

            // Event
            ImGui::TableSetColumnIndex(2);
            ImGui::Text("%s", it->eventName.c_str());

            // Info — show entity state if it exists
            ImGui::TableSetColumnIndex(3);
            auto entity = entt::entity(it->entityId);
            if (ecs_->valid(entity))
            {
                if (auto* t = ecs_->try_get<Transform>(entity))
                    ImGui::Text("pos=(%d,%d)", t->x.ToInt(), t->y.ToInt());
                else
                    ImGui::Text("—");
            }
            else
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "destroyed");
        }

        ImGui::EndTable();
    }
}

// ═════════════════════════════════════════════════════════════
// Breakpoints Tab
// ═════════════════════════════════════════════════════════════

void EventDebugger::DrawBreakpoints()
{
    // ── Step controls ────────────────────────────────────────
    bool paused = lua_->IsPaused();
    if (paused)
    {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "⏸ PAUSED");
        ImGui::Text("Entity %u, Event: %s",
            lua_->LastBreakEntity(), lua_->LastBreakEvent().c_str());
    }
    else
    {
        ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "▶ RUNNING");
    }

    if (ImGui::Button("Continue (F5)", ImVec2(120, 0)))
    {
        lua_->ContinueExecution();
        Log("Execution continued.");
    }
    ImGui::SameLine();
    if (ImGui::Button("Step Once (F10)", ImVec2(120, 0)))
    {
        lua_->StepOnce();
        Log("Stepping one event...");
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear All Breakpoints"))
    {
        lua_->ClearAllBreakpoints();
        Log("All breakpoints cleared.");
    }

    ImGui::Separator();

    // ── Add breakpoint ───────────────────────────────────────
    ImGui::Text("Add Breakpoint:");
    ImGui::Checkbox("All Entities", &bpAllEntities_);
    if (!bpAllEntities_)
        ImGui::InputInt("Entity ID", &bpEntityId_);

    const char* events[] = {"OnInit", "OnTick", "OnRightClick", "OnTakeDamage", "OnDeath"};
    ImGui::Combo("Event", &bpEventIdx_, events, 5);

    ImGui::SameLine();
    if (ImGui::Button("Add"))
    {
        auto e = bpAllEntities_
            ? entt::entity(0xFFFFFFFF)
            : entt::entity(static_cast<uint32_t>(bpEntityId_));
        lua_->AddBreakpoint(e, events[bpEventIdx_]);
        Log("Breakpoint added: " + std::string(events[bpEventIdx_]));
    }

    ImGui::Separator();

    // ── Active breakpoints list ──────────────────────────────
    ImGui::Text("Active Breakpoints:");
    // (We can't iterate the breakpoints_ set directly since it's private)
    // Show the message that breakpoints are active
    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
        "Use the Add button above to set breakpoints.\n"
        "When a breakpoint hits, execution pauses and the\n"
        "trace log shows ⏸ at that event.");
}

// ═════════════════════════════════════════════════════════════
// Inspector Tab
// ═════════════════════════════════════════════════════════════

void EventDebugger::DrawInspector()
{
    ImGui::Text("Inspect entity state at break time:");
    ImGui::InputInt("Entity ID", &inspectEntityId_);

    ImGui::Separator();

    auto entity = entt::entity(static_cast<uint32_t>(inspectEntityId_));
    if (!ecs_->valid(entity))
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Entity not found.");
        return;
    }

    uint32_t id = static_cast<uint32_t>(entt::to_integral(entity));
    ImGui::Text("Entity %u", id);

    if (auto* t = ecs_->try_get<Transform>(entity))
        ImGui::Text("  Position: (%d, %d) tiles", t->x.ToInt(), t->y.ToInt());
    if (auto* h = ecs_->try_get<Health>(entity))
    {
        ImGui::Text("  Health: %d / %d HP", h->current.ToInt(), h->max.ToInt());
        float frac = h->current.Raw() > 0
            ? static_cast<float>(h->current.Raw()) / static_cast<float>(h->max.Raw()) : 0.0f;
        ImGui::ProgressBar(frac, ImVec2(-1, 0), "");
    }
    if (auto* p = ecs_->try_get<Player>(entity))
        ImGui::Text("  Faction: %d", p->factionId);
    if (auto* w = ecs_->try_get<Weapon>(entity))
        ImGui::Text("  Weapon: %d dmg, %d range, type=%d",
            w->damage.ToInt(), w->range.ToInt(), w->damageType);
    if (auto* m = ecs_->try_get<Movement>(entity))
        ImGui::Text("  Moving to: (%d, %d) at speed %d",
            m->targetX.ToInt(), m->targetY.ToInt(), m->speed.ToInt());

    // Script status
    ImGui::Separator();
    ImGui::Text("Scripts:");
    for (auto evt : {"OnInit", "OnTick", "OnRightClick", "OnTakeDamage", "OnDeath"})
    {
        if (lua_->HasScript(entity, evt))
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "  ✓ %s", evt);
        else
            ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "  — %s", evt);
    }
}

} // namespace beigebox
