// editor/src/panels/properties_grid.cpp
// ─────────────────────────────────────────────────────────────
// Properties Grid — multi-purpose property editor.
// ─────────────────────────────────────────────────────────────

#include "properties_grid.h"
#include "beigebox/ecs/components.h"
#include "menu_builder.h"  // for WidgetDef, WidgetType, Anchor

#include <imgui.h>
#include <string>
#include <cstring>

namespace beigebox {

void PropertiesGrid::Draw()
{
    ImGui::Begin("Properties");

    // ── Widget properties mode (takes priority) ──────────
    if (selectedWidget_)
    {
        DrawWidgetProperties();
        ImGui::End();
        return;
    }

    // ── Entity properties mode ───────────────────────────
    if (selected_ == entt::null || !registry_->valid(selected_))
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
            "Select an entity or widget to edit its properties.");
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

// ═════════════════════════════════════════════════════════════
// Widget Properties mode (when a Menu Builder widget is selected)
// ═════════════════════════════════════════════════════════════

void PropertiesGrid::DrawWidgetProperties()
{
    WidgetDef& w = *selectedWidget_;
    ImGui::Text("%s %s", MenuBuilder::WidgetIcon(w.type), w.name.c_str());
    ImGui::Separator();

    // Name
    char nameBuf[128];
    strncpy(nameBuf, w.name.c_str(), sizeof(nameBuf) - 1);
    nameBuf[sizeof(nameBuf) - 1] = 0;
    if (ImGui::InputText("Name", nameBuf, sizeof(nameBuf)))
        w.name = nameBuf;

    // Type (read-only)
    ImGui::LabelText("Type", "%s %s", MenuBuilder::WidgetIcon(w.type),
        MenuBuilder::WidgetTypeName(w.type));

    // Position
    ImGui::InputInt("Pos X", &w.offsetX);
    ImGui::InputInt("Pos Y", &w.offsetY);

    // Size
    if (ImGui::InputInt("Width", &w.width))  { if (w.width < 20) w.width = 20; }
    if (ImGui::InputInt("Height", &w.height)) { if (w.height < 14) w.height = 14; }

    // Anchor
    const char* anchorNames[] = {"Top-Left","Top-Center","Top-Right",
        "Center-Left","Center","Center-Right",
        "Bottom-Left","Bottom-Center","Bottom-Right"};
    int aidx = (int)w.anchor;
    if (ImGui::Combo("Anchor", &aidx, anchorNames, 9))
        w.anchor = (Anchor)aidx;

    // Text
    char txtBuf[256];
    strncpy(txtBuf, w.text.c_str(), sizeof(txtBuf) - 1);
    txtBuf[sizeof(txtBuf) - 1] = 0;
    if (ImGui::InputText("Text", txtBuf, sizeof(txtBuf)))
        w.text = txtBuf;

    // Font size
    if (ImGui::InputInt("Font Size", &w.fontSize))
        { if (w.fontSize < 8) w.fontSize = 8; }

    // Color
    float col[4] = {w.colorR, w.colorG, w.colorB, w.colorA};
    if (ImGui::ColorEdit4("Color", col))
        { w.colorR = col[0]; w.colorG = col[1]; w.colorB = col[2]; w.colorA = col[3]; }

    // Flags
    ImGui::Checkbox("Visible", &w.visible);
    ImGui::SameLine();
    ImGui::Checkbox("Locked", &w.locked);

    // onClick
    char clickBuf[128];
    strncpy(clickBuf, w.onClick.c_str(), sizeof(clickBuf) - 1);
    clickBuf[sizeof(clickBuf) - 1] = 0;
    if (ImGui::InputText("OnClick", clickBuf, sizeof(clickBuf)))
        w.onClick = clickBuf;

    // Binding
    char bindBuf[128];
    strncpy(bindBuf, w.binding.c_str(), sizeof(bindBuf) - 1);
    bindBuf[sizeof(bindBuf) - 1] = 0;
    if (ImGui::InputText("Binding", bindBuf, sizeof(bindBuf)))
        w.binding = bindBuf;
}

} // namespace beigebox
