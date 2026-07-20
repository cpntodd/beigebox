// editor/src/panels/entity_list.h
// ─────────────────────────────────────────────────────────────
// Entity List Panel — displays all entities in the ECS registry
// with their components and attached scripts.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <entt/entt.hpp>

#include "beigebox/lua/lua_bridge.h"

namespace beigebox {

class EntityListPanel
{
public:
    explicit EntityListPanel(entt::registry& registry, LuaBridge& lua)
        : registry_(&registry), lua_(&lua) {}

    // Render the entity list table. Call once per frame inside
    // an ImGui window (or use Begin/End internally).
    void Draw();

private:
    entt::registry* registry_;
    LuaBridge*      lua_;
};

} // namespace beigebox
