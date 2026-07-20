// editor/src/panels/event_debugger.h
// ─────────────────────────────────────────────────────────────
// Event Debugger Panel — trace log, breakpoints, step controls.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <entt/entt.hpp>
#include "beigebox/lua/lua_bridge.h"

#include <functional>

namespace beigebox {

class AIChatPanel;

class EventDebugger
{
public:
    EventDebugger(entt::registry& ecs, LuaBridge& lua)
        : ecs_(&ecs), lua_(&lua) {}

    void SetAIChat(AIChatPanel* chat) { aiChat_ = chat; }

    void Draw();

private:
    void DrawTraceLog();
    void DrawBreakpoints();
    void DrawInspector();

    void Log(const std::string& msg);

    entt::registry* ecs_;
    LuaBridge*      lua_;
    AIChatPanel*    aiChat_ = nullptr;

    // Breakpoint UI state
    int  bpEntityId_  = 0;
    int  bpEventIdx_  = 1; // default: OnTick
    bool bpAllEntities_ = false;

    // Inspector: entity to inspect
    int inspectEntityId_ = 0;
};

} // namespace beigebox
