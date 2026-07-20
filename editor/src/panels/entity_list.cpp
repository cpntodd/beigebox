// editor/src/panels/entity_list.cpp
// ─────────────────────────────────────────────────────────────
// Entity List Panel — ImGui table of all ECS entities.
// ─────────────────────────────────────────────────────────────

#include "entity_list.h"
#include "beigebox/ecs/components.h"

#include <imgui.h>

namespace beigebox {

void EntityListPanel::Draw()
{
    ImGui::Begin("Entity List");

    if (ImGui::BeginTable("##entitytable", 6,
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
        ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable))
    {
        ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthFixed, 50.0f);
        ImGui::TableSetupColumn("Position", ImGuiTableColumnFlags_WidthFixed, 100.0f);
        ImGui::TableSetupColumn("Health", ImGuiTableColumnFlags_WidthFixed, 80.0f);
        ImGui::TableSetupColumn("Faction", ImGuiTableColumnFlags_WidthFixed, 60.0f);
        ImGui::TableSetupColumn("Scripts", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Components", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        auto view = registry_->view<entt::entity>();
        for (auto entity : view)
        {
            uint32_t id = static_cast<uint32_t>(entt::to_integral(entity));
            ImGui::TableNextRow();

            // ID
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%u", id);

            // Position
            ImGui::TableSetColumnIndex(1);
            if (auto* t = registry_->try_get<Transform>(entity))
                ImGui::Text("(%d, %d)", t->x.ToInt(), t->y.ToInt());
            else
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "—");

            // Health
            ImGui::TableSetColumnIndex(2);
            if (auto* h = registry_->try_get<Health>(entity))
                ImGui::Text("%d / %d", h->current.ToInt(), h->max.ToInt());
            else
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "—");

            // Faction
            ImGui::TableSetColumnIndex(3);
            if (auto* p = registry_->try_get<Player>(entity))
            {
                // Color-code by faction
                switch (p->factionId)
                {
                case 1: ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "%d", p->factionId); break;
                case 2: ImGui::TextColored(ImVec4(0.2f, 0.4f, 0.8f, 1.0f), "%d", p->factionId); break;
                default: ImGui::Text("%d", p->factionId); break;
                }
            }
            else
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "—");

            // Scripts
            ImGui::TableSetColumnIndex(4);
            bool hasAny = false;
            for (auto eventName : {"OnInit", "OnTick", "OnTakeDamage", "OnDeath"})
            {
                if (lua_->HasScript(entity, eventName))
                {
                    if (hasAny) ImGui::SameLine();
                    ImGui::TextColored(ImVec4(0.3f, 0.8f, 0.3f, 1.0f), "%s", eventName);
                    hasAny = true;
                }
            }
            if (!hasAny)
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "none");

            // Components
            ImGui::TableSetColumnIndex(5);
            std::string comps;
            if (registry_->all_of<Transform>(entity)) comps += "Transform ";
            if (registry_->all_of<Health>(entity))    comps += "Health ";
            if (registry_->all_of<Player>(entity))    comps += "Player ";
            if (registry_->all_of<Velocity>(entity))  comps += "Velocity ";
            if (comps.empty()) comps = "—";
            ImGui::Text("%s", comps.c_str());
        }

        ImGui::EndTable();
    }

    ImGui::End();
}

} // namespace beigebox
