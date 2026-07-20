// editor/src/panels/properties_grid.cpp
// ─────────────────────────────────────────────────────────────
// Properties Grid — VB6-style component editor.
// ─────────────────────────────────────────────────────────────

#include "properties_grid.h"
#include "beigebox/ecs/components.h"

#include <imgui.h>
#include <string>

namespace beigebox {

void PropertiesGrid::Draw()
{
    ImGui::Begin("Properties");

    if (selected_ == entt::null || !registry_->valid(selected_))
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
            "Select an entity in the Entity List to edit its properties.");
        ImGui::End();
        return;
    }

    uint32_t id = static_cast<uint32_t>(entt::to_integral(selected_));
    ImGui::Text("Entity %u", id);
    ImGui::Separator();

    DrawTransform();
    DrawHealth();
    DrawPlayer();
    DrawMovement();
    DrawWeapon();

    ImGui::End();
}

// ── Transform ────────────────────────────────────────────────

void PropertiesGrid::DrawTransform()
{
    if (!registry_->all_of<Transform>(selected_)) return;

    auto& t = registry_->get<Transform>(selected_);
    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
    {
        int x = t.x.ToInt();
        int y = t.y.ToInt();
        ImGui::InputInt("X (tiles)", &x);
        ImGui::InputInt("Y (tiles)", &y);
        t.x = FixedPoint::FromInt(std::max(0, x));
        t.y = FixedPoint::FromInt(std::max(0, y));
    }
}

// ── Health ───────────────────────────────────────────────────

void PropertiesGrid::DrawHealth()
{
    if (!registry_->all_of<Health>(selected_)) return;

    auto& h = registry_->get<Health>(selected_);
    if (ImGui::CollapsingHeader("Health", ImGuiTreeNodeFlags_DefaultOpen))
    {
        int cur = h.current.ToInt();
        int max = h.max.ToInt();
        ImGui::InputInt("Current", &cur);
        ImGui::InputInt("Max", &max);
        h.current = FixedPoint::FromInt(std::max(0, cur));
        h.max     = FixedPoint::FromInt(std::max(1, max));
        if (h.current > h.max) h.current = h.max;

        float frac = static_cast<float>(h.current.Raw()) / static_cast<float>(h.max.Raw());
        ImGui::ProgressBar(frac, ImVec2(-1, 0), "");
        ImGui::Text("%d / %d HP", h.current.ToInt(), h.max.ToInt());
    }
}

// ── Player ───────────────────────────────────────────────────

void PropertiesGrid::DrawPlayer()
{
    if (!registry_->all_of<Player>(selected_)) return;

    auto& p = registry_->get<Player>(selected_);
    if (ImGui::CollapsingHeader("Player", ImGuiTreeNodeFlags_DefaultOpen))
    {
        ImGui::InputInt("Faction ID", &p.factionId);
    }
}

// ── Movement ─────────────────────────────────────────────────

void PropertiesGrid::DrawMovement()
{
    if (!registry_->all_of<Movement>(selected_)) return;

    auto& m = registry_->get<Movement>(selected_);
    if (ImGui::CollapsingHeader("Movement", ImGuiTreeNodeFlags_DefaultOpen))
    {
        int tx = m.targetX.ToInt();
        int ty = m.targetY.ToInt();
        int spd = m.speed.ToInt();
        ImGui::InputInt("Target X", &tx);
        ImGui::InputInt("Target Y", &ty);
        ImGui::InputInt("Speed (tiles/s)", &spd);
        m.targetX = FixedPoint::FromInt(tx);
        m.targetY = FixedPoint::FromInt(ty);
        m.speed   = FixedPoint::FromInt(std::max(0, spd));
    }
}

// ── Weapon ───────────────────────────────────────────────────

void PropertiesGrid::DrawWeapon()
{
    if (!registry_->all_of<Weapon>(selected_)) return;

    auto& w = registry_->get<Weapon>(selected_);
    if (ImGui::CollapsingHeader("Weapon", ImGuiTreeNodeFlags_DefaultOpen))
    {
        int dmg = w.damage.ToInt();
        int rng = w.range.ToInt();
        ImGui::InputInt("Damage", &dmg);
        ImGui::InputInt("Range", &rng);
        w.damage = FixedPoint::FromInt(std::max(0, dmg));
        w.range  = FixedPoint::FromInt(std::max(0, rng));

        const char* types[] = {"Kinetic", "Thermal", "Pure"};
        ImGui::Combo("Damage Type", &w.damageType, types, 3);
    }
}

} // namespace beigebox
