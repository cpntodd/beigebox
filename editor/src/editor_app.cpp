// editor/src/editor_app.cpp
// ─────────────────────────────────────────────────────────────
// M.A.D. Editor — application entry point.
//
// Creates an SDL2 window with an OpenGL 3.1 context, initializes
// Dear ImGui with docking, the ECS + Lua engine context, MCP
// tool registry, and renders the Entity List + AI Chat panels.
// ─────────────────────────────────────────────────────────────

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_opengl3.h>

#include <entt/entt.hpp>

#include "beigebox/ecs/components.h"
#include "beigebox/ecs/systems.h"
#include "beigebox/lua/lua_bridge.h"
#include "beigebox/core/fixed_point.h"
#include "beigebox/world/thaw_grid.h"
#include "beigebox/render/sprite_registry.h"
#include "beigebox/render/renderer.h"

#include "mcp/tool_registry.h"
#include "mcp/tools.h"
#include "mcp/json_rpc.h"
#include "mcp/llm_client.h"
#include "panels/entity_list.h"
#include "panels/ai_chat.h"
#include "panels/properties_grid.h"
#include "panels/event_editor.h"
#include "panels/asset_browser.h"
#include "panels/event_debugger.h"
#include "panels/viewport.h"
#include "panels/project_explorer.h"
#include "panels/sound_manager.h"
#include "panels/unit_templates.h"
#include "panels/script_editor.h"
#include "panels/entity_editor.h"
#include "panels/debug_console.h"
#include "panels/level_editor.h"
#include "panels/menu_builder.h"
#include "panels/faction_editor.h"
#include "panels/build_manager.h"
#include "undo/undo_manager.h"
#include "menu_bar.h"
#include "settings.h"

#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

int main(int argc, char* argv[])
{
    // ── SDL2 Window + OpenGL Context ────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0)
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow(
        "M.A.D. Editor — Project M.A.D.",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_MAXIMIZED
    );

    if (!window)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);

    // ── Dear ImGui Initialization ───────────────────────────
    // Store imgui.ini in XDG config dir, not CWD
    {
        std::string configDir = beigebox::Settings::ConfigDir();
        mkdir(configDir.c_str(), 0755);
        std::string iniPath = configDir + "/imgui.ini";

        FILE* f = fopen(iniPath.c_str(), "rb");
        if (f)
        {
            fseek(f, 0, SEEK_END);
            long sz = ftell(f);
            fclose(f);
            if (sz < 10)  // too small to be valid — corrupt
            {
                remove(iniPath.c_str());
                SDL_Log("imgui.ini was corrupt — removed");
            }
        }
    }

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    {
        std::string iniPath = beigebox::Settings::ConfigDir() + "/imgui.ini";
        static std::string s_iniPath;
        s_iniPath = iniPath;
        io.IniFilename = s_iniPath.c_str();
    }

    // ── Auto DPI detection ───────────────────────────────────
    {
        float ddpi, hdpi, vdpi;
        if (SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi) == 0)
        {
            float dpiScale = hdpi / 96.0f; // 96 DPI = standard
            if (dpiScale < 1.0f) dpiScale = 1.0f;
            if (dpiScale > 3.0f) dpiScale = 3.0f;
            io.FontGlobalScale = dpiScale;
            SDL_Log("DPI: %.0f (scale: %.2f)", hdpi, dpiScale);
        }
    }
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigWindowsMoveFromTitleBarOnly = true;

    // Retro-styled editor
    ImGui::StyleColorsDark();
    ImGui::GetStyle().FrameRounding = 2.0f;
    ImGui::GetStyle().WindowRounding = 4.0f;

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init("#version 130");

    // ── Engine Context ──────────────────────────────────────
    entt::registry ecs;
    beigebox::ThawGrid thawGrid;
    thawGrid.Init(32, 32);

    beigebox::LuaBridge lua;
    lua.Init(ecs);

    beigebox::SpriteRegistry spriteRegistry;
    lua.SetThawGrid(&thawGrid);

    // ── MCP Tool Registry + JSON-RPC Server ─────────────────
    beigebox::ToolRegistry tools(ecs, lua);
    beigebox::RegisterAllTools(tools);

    // ── Editor Panels ───────────────────────────────────────
    beigebox::EntityListPanel entityList(ecs, lua);
    beigebox::AIChatPanel     aiChat(tools);
    beigebox::AssetBrowser    assetBrowser(spriteRegistry);
    beigebox::PropertiesGrid  propGrid(ecs);
    beigebox::EventEditor     eventEditor(ecs, lua);
    beigebox::EventDebugger   eventDebugger(ecs, lua);
    beigebox::UndoManager     undoManager;

    // ── Viewport Renderer ────────────────────────────────────
    beigebox::Renderer viewportRenderer;
    viewportRenderer.Init(window);

    // ── Viewport Panel ───────────────────────────────────────
    beigebox::ViewportPanel viewport(ecs, viewportRenderer, thawGrid, lua);
    viewport.SetUndoManager(&undoManager);

    beigebox::ProjectExplorer projectExplorer;
    beigebox::SoundManager    soundManager;
    beigebox::UnitTemplateManager unitTemplates;
    beigebox::ScriptEditor    scriptEditor(lua);
    beigebox::EntityEditor    entityEditor(lua, viewportRenderer, thawGrid);
    beigebox::DebugConsole   debugConsole(ecs, lua);
    beigebox::LevelEditor    levelEditor;
    beigebox::MenuBuilder    menuBuilder;
    beigebox::FactionEditor  factionEditor;
    beigebox::BuildManager   buildManager;
    unitTemplates.Load("unit_templates.json");

    // JSON-RPC server — logs through the AI Chat panel
    beigebox::JsonRpcServer jsonRpc(tools);
    jsonRpc.SetLogger([&](const std::string& msg) {
        aiChat.AppendMessage("system", "[JSON-RPC] " + msg);
    });

    // AI LLM Client
    beigebox::LlmClient llmClient;
    llmClient.BuildSystemPrompt(tools);

    // ── Main Menu Bar ────────────────────────────────────────
    bool running = true;

    beigebox::MainMenuBar menuBar(ecs, lua, tools, llmClient);
    menuBar.SetAIChat(&aiChat);
    assetBrowser.SetAIChat(&aiChat);
    eventDebugger.SetAIChat(&aiChat);
    aiChat.SetLlmClient(&llmClient);
    eventDebugger.SetAIChat(&aiChat);
    menuBar.SetThawGrid(&thawGrid);
    menuBar.SetSpriteRegistry(&spriteRegistry);
    menuBar.SetUndoManager(&undoManager);
    menuBar.onQuit = [&]() { running = false; };
    menuBar.onExportGame = [&]() { buildManager.Open(); };
    menuBar.onNewProject = [&]() {
        // Clear ECS and reset to default
        ecs.clear();
        thawGrid.Init(32, 32);
        auto e = ecs.create();
        ecs.emplace<beigebox::Transform>(e, beigebox::FixedPoint::FromInt(5), beigebox::FixedPoint::FromInt(5));
        ecs.emplace<beigebox::Health>(e, beigebox::FixedPoint::FromInt(100), beigebox::FixedPoint::FromInt(100));
        ecs.emplace<beigebox::Player>(e, 1);
        lua.LoadScript(e, "OnTick", R"lua(
            function OnTick(entity_id)
                local x, y = Transform.GetPosition(entity_id)
                x = x + 4
                if x > 12 * 256 then x = 0 end
                Transform.SetPosition(entity_id, x, y)
            end
        )lua");
        aiChat.AppendMessage("system", "New project created. Demo entity spawned.");
    };
    menuBar.onResetLayout = [&]() {
        aiChat.AppendMessage("system", "Layout reset requested. Delete imgui.ini and restart to reset window positions.");
    };

    // ── Selection sync: Entity List → Properties + Event Editor ──
    entityList.onSelect = [&](entt::entity e) {
        propGrid.SelectEntity(e);
        eventEditor.SelectEntity(e);
        menuBar.SetSelectedEntity(e);
    };

    // ── Seed: spawn a demo entity for the user to play with ─
    auto demoEntity = ecs.create();
    ecs.emplace<beigebox::Transform>(demoEntity,
        beigebox::FixedPoint::FromInt(5),
        beigebox::FixedPoint::FromInt(5));
    ecs.emplace<beigebox::Health>(demoEntity,
        beigebox::FixedPoint::FromInt(100),
        beigebox::FixedPoint::FromInt(100));
    ecs.emplace<beigebox::Player>(demoEntity, 1);
    lua.LoadScript(demoEntity, "OnTick", R"lua(
        function OnTick(entity_id)
            local x, y = Transform.GetPosition(entity_id)
            x = x + 4  -- drift slowly right
            if x > 12 * 256 then x = 0 end
            Transform.SetPosition(entity_id, x, y)
        end
    )lua");

    aiChat.AppendMessage("system",
        "Welcome to M.A.D. Editor v0.1.0\n"
        "Type /help to see available tools.\n"
        "Try: /create_entity faction=2 name=\"Cryo Walker\"\n"
        "     /query_entities\n"
        "     /write_script entity=1 event=OnInit code=\"function OnInit(id) Transform.SetPosition(id, 10*256, 3*256) end\"\n");

    // ── Main Editor Loop ────────────────────────────────────
    while (running)
    {
        // ── Poll Events ────────────────────────────────────
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                running = false;
            if (event.type == SDL_WINDOWEVENT
                && event.window.event == SDL_WINDOWEVENT_CLOSE
                && event.window.windowID == SDL_GetWindowID(window))
                running = false;

            // ── Undo/Redo shortcuts ──────────────────────────
            if (event.type == SDL_KEYDOWN)
            {
                bool ctrl = (SDL_GetModState() & KMOD_CTRL) != 0;
                if (ctrl && event.key.keysym.sym == SDLK_z)
                    undoManager.Undo();
                if (ctrl && event.key.keysym.sym == SDLK_y)
                    undoManager.Redo();
            }
        }

        // ── ECS Tick (game logic) ───────────────────────────
        beigebox::TickSystems(ecs);
        thawGrid.Tick();
        ecs.view<entt::entity>().each([&](entt::entity entity) {
            if (lua.HasScript(entity, "OnTick"))
                lua.FireEvent(entity, "OnTick");
        });

        // ── ImGui Frame Begin ───────────────────────────────
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // ── Dockspace ───────────────────────────────────────
        ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // ── Menu Bar ────────────────────────────────────────
        menuBar.Draw();

        // ── Editor Panels ───────────────────────────────────
        projectExplorer.SetProjectPath(menuBar.GetProjectPath());
        entityList.Draw();
        propGrid.Draw();
        viewport.Draw();
        unitTemplates.Draw();
        projectExplorer.Draw();
        soundManager.Draw();
        scriptEditor.Draw();
        entityEditor.Draw();
        debugConsole.Draw();
        levelEditor.Draw();
        menuBuilder.Draw();
        factionEditor.Draw();
        buildManager.Draw();
        assetBrowser.Draw();
        eventEditor.Draw();
        eventDebugger.Draw();
        aiChat.Draw();

        // ── ImGui Frame End + Render ────────────────────────
        ImGui::Render();
        SDL_GL_MakeCurrent(window, gl_context);
        glViewport(0, 0,
            static_cast<int>(io.DisplaySize.x),
            static_cast<int>(io.DisplaySize.y));
        glClearColor(0.10f, 0.10f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // ── Shutdown ─────────────────────────────────────────────
    lua.Shutdown();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return EXIT_SUCCESS;
}
