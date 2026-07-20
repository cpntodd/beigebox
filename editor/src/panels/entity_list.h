// editor/src/panels/entity_list.h
// ─────────────────────────────────────────────────────────────
// Entity List Panel — displays all entities in the ECS registry
// with their components and attached scripts.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <entt/entt.hpp>
#include "beigebox/lua/lua_bridge.h"
#include <functional>

namespace beigebox {

class EntityListPanel
{
public:
    explicit EntityListPanel(entt::registry& registry, LuaBridge& lua)
        : registry_(&registry), lua_(&lua) {}

    void Draw();

    // Get the currently selected entity
    entt::entity Selected() const { return selected_; }

    // Callback when user clicks an entity row
    using SelectCallback = std::function<void(entt::entity)>;
    SelectCallback onSelect;

private:
    entt::registry* registry_;
    LuaBridge*      lua_;
    entt::entity    selected_ = entt::null;
};

} // namespace beigebox
