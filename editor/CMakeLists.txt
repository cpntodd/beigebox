# editor/CMakeLists.txt
# ─────────────────────────────────────────────────────────────
# M.A.D. Editor — ImGui-based game editor with MCP AI integration.
# Links against the beigebox_engine library.
# ─────────────────────────────────────────────────────────────

# ── ImGui Core + Platform Backends ───────────────────────────
# ImGui docking branch doesn't ship a CMake target, so we compile
# the core .cpp files directly alongside the platform backends.
set(IMGUI_SOURCES
    # Core
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    # Platform backends
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
)

add_executable(mad_editor
    src/editor_app.cpp
    src/menu_bar.cpp
    src/mcp/tool_registry.cpp
    src/mcp/tools.cpp
    src/mcp/json_rpc.cpp
    src/mcp/llm_client.cpp
    src/panels/entity_list.cpp
    src/panels/ai_chat.cpp
    src/panels/properties_grid.cpp
    src/panels/event_editor.cpp
    src/panels/asset_browser.cpp
    src/panels/event_debugger.cpp
    src/undo/undo_manager.cpp
    ${IMGUI_SOURCES}
)

# ── Include paths ────────────────────────────────────────────
target_include_directories(mad_editor
    PRIVATE
        src
        ${imgui_SOURCE_DIR}
        ${imgui_SOURCE_DIR}/backends
)

# ── Dependencies ─────────────────────────────────────────────
target_link_libraries(mad_editor
    PRIVATE
        beigebox_engine
        SDL2::SDL2
        OpenGL::GL
        nlohmann_json::nlohmann_json
)
