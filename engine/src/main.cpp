// engine/src/main.cpp
// ─────────────────────────────────────────────────────────────
// BeigeBox Runtime — SDL2 + OpenGL + EnTT ECS + Lua scripting.
//
// This is the standalone game runtime. The M.A.D. Editor links
// against beigebox_engine and drives it programmatically.
// ─────────────────────────────────────────────────────────────

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include <entt/entt.hpp>

#include "beigebox/core/fixed_point.h"
#include "beigebox/ecs/components.h"
#include "beigebox/ecs/systems.h"
#include "beigebox/render/renderer.h"
#include "beigebox/render/isometric.h"
#include "beigebox/lua/lua_bridge.h"
#include "beigebox/world/thaw_grid.h"

#include <cstdlib>
#include <cstdio>

int main(int argc, char* argv[])
{
    // ── SDL2 Initialization ─────────────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return EXIT_FAILURE;
    }

    // ── OpenGL Attributes ───────────────────────────────────
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    // ── Window ──────────────────────────────────────────────
    SDL_Window* window = SDL_CreateWindow(
        "BeigeBox Engine — ECS + Lua",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1024, 768,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (!window)
    {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // ── Renderer ────────────────────────────────────────────
    beigebox::Renderer renderer;
    if (!renderer.Init(window))
    {
        SDL_Log("Renderer init failed.");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // ── Grid & Camera ───────────────────────────────────────
    constexpr int GRID_W = 12;
    constexpr int GRID_H = 12;

    // ── ECS World ───────────────────────────────────────────
    entt::registry registry;

    // ── Thaw Grid ───────────────────────────────────────────
    beigebox::ThawGrid thawGrid;
    thawGrid.Init(GRID_W, GRID_H);

    // ── Lua Bridge ──────────────────────────────────────────
    beigebox::LuaBridge lua;
    if (!lua.Init(registry))
    {
        SDL_Log("LuaBridge init failed.");
        renderer.Shutdown();
        SDL_DestroyWindow(window);
        SDL_Quit();
        return EXIT_FAILURE;
    }
    lua.SetThawGrid(&thawGrid);

    // ── Spawn Test Entities ─────────────────────────────────
    auto driller = registry.create();
    registry.emplace<beigebox::Transform>(driller,
        beigebox::FixedPoint::FromInt(5), beigebox::FixedPoint::FromInt(5));
    registry.emplace<beigebox::Health>(driller,
        beigebox::FixedPoint::FromInt(100), beigebox::FixedPoint::FromInt(100));
    registry.emplace<beigebox::Player>(driller, 1);
    registry.emplace<beigebox::Weapon>(driller,
        beigebox::FixedPoint::FromInt(15), beigebox::FixedPoint::FromInt(2), 0);

    lua.LoadScript(driller, "OnTick", R"lua(
        function OnTick(entity_id)
            local x, y = Transform.GetPosition(entity_id)
            x = x + 4
            if x > 12 * 256 then x = 0 end
            Transform.SetPosition(entity_id, x, y)
        end
    )lua");

    auto building = registry.create();
    registry.emplace<beigebox::Transform>(building,
        beigebox::FixedPoint::FromInt(8), beigebox::FixedPoint::FromInt(3));
    registry.emplace<beigebox::Health>(building,
        beigebox::FixedPoint::FromInt(500), beigebox::FixedPoint::FromInt(500));
    registry.emplace<beigebox::Player>(building, 1);

    // Thaw test: add a heat source near (6, 6)
    thawGrid.AddHeatSource(6, 6,
        beigebox::FixedPoint::FromInt(4),
        beigebox::FixedPoint::FromInt(25));

    SDL_Log("ECS: entities spawned (Driller=%u, Building=%u)",
            static_cast<unsigned>(entt::to_integral(driller)),
            static_cast<unsigned>(entt::to_integral(building)));

    // ── Grid & Camera ───────────────────────────────────────
    int cameraX = 0;
    int cameraY = 0;
    constexpr int SCROLL_SPEED = 4;

    // ── Main Loop ───────────────────────────────────────────
    bool running = true;
    SDL_Event event;
    Uint32 lastTick = SDL_GetTicks();

    while (running)
    {
        // ── Delta Time ──────────────────────────────────────
        Uint32 now = SDL_GetTicks();
        Uint32 dt = now - lastTick;
        lastTick = now;

        // ── Input ───────────────────────────────────────────
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                    running = false;
                break;
            }
        }

        // ── Continuous input ────────────────────────────────
        const Uint8* kb = SDL_GetKeyboardState(nullptr);
        if (kb[SDL_SCANCODE_LEFT]  || kb[SDL_SCANCODE_A])  cameraX -= SCROLL_SPEED;
        if (kb[SDL_SCANCODE_RIGHT] || kb[SDL_SCANCODE_D])  cameraX += SCROLL_SPEED;
        if (kb[SDL_SCANCODE_UP]    || kb[SDL_SCANCODE_W])  cameraY -= SCROLL_SPEED;
        if (kb[SDL_SCANCODE_DOWN]  || kb[SDL_SCANCODE_S])  cameraY += SCROLL_SPEED;

        // ── ECS Systems ───────────────────────────────────────
        beigebox::TickSystems(registry);

        // ── Thaw Tick ────────────────────────────────────────
        thawGrid.Tick();

        // ── Fire Lua events ─────────────────────────────────
        registry.view<entt::entity>().each([&](entt::entity entity) {
            if (lua.HasScript(entity, "OnTick"))
                lua.FireEvent(entity, "OnTick");
        });

        // ── Render ──────────────────────────────────────────
        renderer.BeginFrame();
        renderer.DrawTileGrid(GRID_W, GRID_H, cameraX, cameraY);
        renderer.EndFrame();

        // ── Frame cap (~60 FPS) ─────────────────────────────
        SDL_Delay(16);
    }

    // ── Shutdown ─────────────────────────────────────────────
    SDL_Log("BeigeBox: shutting down...");
    lua.Shutdown();
    renderer.Shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();

    return EXIT_SUCCESS;
}
