// editor/src/panels/event_editor.h
// ─────────────────────────────────────────────────────────────
// Event Editor Panel — raw Lua script editor per entity/event.
//
// Displays the list of event handlers for a selected entity,
// with add/remove/edit buttons and a Lua code editor.
//
// The visual block canvas is deferred to v0.2 per GDD.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <entt/entt.hpp>
#include "beigebox/lua/lua_bridge.h"

#include <string>
#include <vector>

namespace beigebox {

class EventEditor
{
public:
    EventEditor(entt::registry& registry, LuaBridge& lua)
        : registry_(&registry), lua_(&lua) {}

    void SelectEntity(entt::entity entity) { selected_ = entity; }
    entt::entity Selected() const { return selected_; }

    void Draw();

private:
    void DrawEventScript(const std::string& eventName);

    entt::registry* registry_;
    LuaBridge*      lua_;
    entt::entity    selected_ = entt::null;

    // Editing state
    char   luaEditorBuf_[4096] = {};
    std::string editingEvent_;
    bool   editorOpen_ = false;
};

} // namespace beigebox
