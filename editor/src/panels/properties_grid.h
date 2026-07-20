// editor/src/panels/properties_grid.h
// ─────────────────────────────────────────────────────────────
// Properties Grid Panel — VB6-style component property editor.
//
// Displays the components of a selected entity as editable
// fields. Changes are written directly to the ECS registry.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <entt/entt.hpp>

namespace beigebox {

class PropertiesGrid
{
public:
    explicit PropertiesGrid(entt::registry& registry)
        : registry_(&registry) {}

    // Select an entity to edit. Pass entt::null to deselect.
    void SelectEntity(entt::entity entity) { selected_ = entity; }

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

    entt::registry* registry_;
    entt::entity    selected_ = entt::null;
};

} // namespace beigebox
