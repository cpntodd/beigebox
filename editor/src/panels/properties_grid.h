// editor/src/panels/properties_grid.h
// ─────────────────────────────────────────────────────────────
// Properties Grid Panel — multi-purpose property editor.
//
// Displays ECS entity components when an entity is selected,
// or WidgetDef fields when a Menu Builder widget is selected.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <entt/entt.hpp>

namespace beigebox {

struct WidgetDef;

class PropertiesGrid
{
public:
    explicit PropertiesGrid(entt::registry& registry)
        : registry_(&registry) {}

    // Select an ECS entity to edit. Pass entt::null to deselect.
    void SelectEntity(entt::entity entity) { selected_ = entity; selectedWidget_ = nullptr; }

    // Select a Menu Builder widget to edit. Pass nullptr to deselect.
    void SelectWidget(WidgetDef* w) { selectedWidget_ = w; selected_ = entt::null; }

    // Get the currently selected entity.
    entt::entity Selected() const { return selected_; }

    // Render the properties grid. Call once per frame.
    void Draw();

private:
    void DrawTransform();
    void DrawHealth();
    void DrawPlayer();
    void DrawMovement();
    void DrawWeapon();
    void DrawWidgetProperties();

    entt::registry* registry_;
    entt::entity    selected_ = entt::null;
    WidgetDef*       selectedWidget_ = nullptr;
};

} // namespace beigebox
