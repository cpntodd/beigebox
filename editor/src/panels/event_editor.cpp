// editor/src/panels/event_editor.cpp
// ─────────────────────────────────────────────────────────────
// Event Editor Panel — raw Lua script management per entity.
// ─────────────────────────────────────────────────────────────

#include "event_editor.h"

#include <imgui.h>
#include <cstring>

namespace beigebox {

static const char* kEventNames[] = {"OnInit", "OnTick", "OnRightClick", "OnTakeDamage", "OnDeath"};
static const int   kEventCount = 5;

void EventEditor::Draw()
{
    ImGui::Begin("Event Editor");

    if (selected_ == entt::null || !registry_->valid(selected_))
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
            "Select an entity in the Entity List.");
        ImGui::End();
        return;
    }

    uint32_t id = static_cast<uint32_t>(entt::to_integral(selected_));
    ImGui::Text("Entity %u — Event Scripts", id);
    ImGui::Separator();

    for (int i = 0; i < kEventCount; ++i)
    {
        bool hasScript = lua_->HasScript(selected_, kEventNames[i]);
        ImGui::PushID(kEventNames[i]);

        if (hasScript)
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "●");
        else
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "○");
        ImGui::SameLine();
        ImGui::Text("%s", kEventNames[i]);
        ImGui::SameLine();

        if (ImGui::SmallButton("Edit"))
        {
            editingEvent_ = kEventNames[i];
            std::string header = "-- " + editingEvent_ + " handler for entity " + std::to_string(id) + "\n";
            std::string funcDef = "function " + editingEvent_ + "(entity_id)\n    \nend\n";
            std::string init = header + funcDef;
            strncpy(luaEditorBuf_, init.c_str(), sizeof(luaEditorBuf_) - 1);
            luaEditorBuf_[sizeof(luaEditorBuf_) - 1] = '\0';
            editorOpen_ = true;
            ImGui::OpenPopup("Lua Editor");
        }

        ImGui::SameLine();
        if (hasScript && ImGui::SmallButton("Clear"))
        {
            lua_->LoadScript(selected_, kEventNames[i],
                "function " + std::string(kEventNames[i]) + "(entity_id) end");
        }

        ImGui::PopID();
    }

    // ── Lua Editor Popup ──────────────────────────────────
    if (ImGui::BeginPopupModal("Lua Editor", &editorOpen_, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Editing: %s", editingEvent_.c_str());

        ImGui::InputTextMultiline("##luaCode", luaEditorBuf_, sizeof(luaEditorBuf_),
            ImVec2(580, 260), ImGuiInputTextFlags_AllowTabInput);

        if (ImGui::Button("Save & Close", ImVec2(120, 0)))
        {
            lua_->LoadScript(selected_, editingEvent_, luaEditorBuf_);
            editorOpen_ = false;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2(80, 0)))
        {
            editorOpen_ = false;
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }

    ImGui::End();
}

} // namespace beigebox
