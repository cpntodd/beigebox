// engine/src/main.cpp
// ─────────────────────────────────────────────────────────────
// BeigeBox Runtime — Playable RTS Prototype
//
// Fixed 30 Hz logic timestep, variable rendering, procedural
// map with thaw overlay, mouse-driven unit control, combat.
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
#include "beigebox/world/map_generator.h"

#include <cstdlib>
#include <cstdio>
#include <vector>

static constexpr int MAP_W      = 24;
static constexpr int MAP_H      = 24;
static constexpr int LOGIC_HZ   = 30;
static constexpr int LOGIC_MS   = 1000 / LOGIC_HZ;
static constexpr int MAX_ENTITIES = 64;

int main(int argc, char* argv[])
{
    // ── SDL2 Init ───────────────────────────────────────────
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError()); return EXIT_FAILURE;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_Window* window = SDL_CreateWindow("BeigeBox — Playable Prototype",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1024, 768,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);
    if (!window) { SDL_Log("Window failed: %s", SDL_GetError()); SDL_Quit(); return EXIT_FAILURE; }

    // ── GL Context ─────────────────────────────────────────
    SDL_GLContext glCtx = SDL_GL_CreateContext(window);
    if (!glCtx) {
        SDL_Log("SDL_GL_CreateContext failed: %s", SDL_GetError());
        SDL_DestroyWindow(window); SDL_Quit(); return EXIT_FAILURE;
    }
    SDL_GL_MakeCurrent(window, glCtx);

    // ── Renderer ────────────────────────────────────────────
    beigebox::Renderer renderer;
    if (!renderer.Init(window)) { SDL_GL_DeleteContext(glCtx); SDL_DestroyWindow(window); SDL_Quit(); return EXIT_FAILURE; }

    // ── ECS + Lua + Thaw ────────────────────────────────────
    entt::registry ecs;
    beigebox::ThawGrid thaw;
    thaw.Init(MAP_W, MAP_H);
    beigebox::LuaBridge lua;
    lua.Init(ecs);
    lua.SetThawGrid(&thaw);

    // ── Generate Map ────────────────────────────────────────
    beigebox::MapGenerator gen;
    beigebox::MapGenerator::Params mapParams;
    mapParams.width = MAP_W; mapParams.height = MAP_H; mapParams.seed = 42;
    mapParams.salvageDensity = 0.15f; mapParams.geothermalFreq = 0.05f;
    std::vector<beigebox::MapTile> mapTiles(MAP_W * MAP_H);
    gen.Generate(mapTiles.data(), mapParams);

    // Extract heat/state arrays for thaw overlay
    std::vector<uint8_t> heatData(MAP_W * MAP_H, 0);
    std::vector<uint8_t> stateData(MAP_W * MAP_H, 0);

    // Spawn geothermal HeatSource entities from map
    for (int y = 0; y < MAP_H; ++y)
        for (int x = 0; x < MAP_W; ++x)
            if (mapTiles[y * MAP_W + x].terrain == beigebox::TileTerrain::Geothermal) {
                auto e = ecs.create();
                ecs.emplace<beigebox::Transform>(e, beigebox::FixedPoint::FromInt(x), beigebox::FixedPoint::FromInt(y));
                ecs.emplace<beigebox::HeatSource>(e, beigebox::FixedPoint::FromInt(2), beigebox::FixedPoint::FromInt(20), 0u);
            }

    // ── Spawn Player Units (Faction 1) ──────────────────────
    auto playerBase = ecs.create();
    ecs.emplace<beigebox::Transform>(playerBase, beigebox::FixedPoint::FromInt(2), beigebox::FixedPoint::FromInt(2));
    ecs.emplace<beigebox::Health>(playerBase, beigebox::FixedPoint::FromInt(500), beigebox::FixedPoint::FromInt(500));
    ecs.emplace<beigebox::Player>(playerBase, 1);
    ecs.emplace<beigebox::Renderable>(playerBase, 0.2f, 0.5f, 0.9f);

    auto playerUnit = ecs.create();
    ecs.emplace<beigebox::Transform>(playerUnit, beigebox::FixedPoint::FromInt(4), beigebox::FixedPoint::FromInt(4));
    ecs.emplace<beigebox::Health>(playerUnit, beigebox::FixedPoint::FromInt(150), beigebox::FixedPoint::FromInt(150));
    ecs.emplace<beigebox::Player>(playerUnit, 1);
    ecs.emplace<beigebox::Weapon>(playerUnit, beigebox::FixedPoint::FromInt(25), beigebox::FixedPoint::FromInt(2), 0);
    ecs.emplace<beigebox::Renderable>(playerUnit, 0.3f, 0.7f, 0.3f);

    // ── Spawn Enemy Units (Faction 2) ───────────────────────
    for (int i = 0; i < 3; ++i) {
        int ex = 15 + (i % 3) * 2;
        int ey = 5 + i;
        auto enemy = ecs.create();
        ecs.emplace<beigebox::Transform>(enemy, beigebox::FixedPoint::FromInt(ex), beigebox::FixedPoint::FromInt(ey));
        ecs.emplace<beigebox::Health>(enemy, beigebox::FixedPoint::FromInt(80), beigebox::FixedPoint::FromInt(80));
        ecs.emplace<beigebox::Player>(enemy, 2);
        ecs.emplace<beigebox::Weapon>(enemy, beigebox::FixedPoint::FromInt(15), beigebox::FixedPoint::FromInt(2), 0);
        ecs.emplace<beigebox::Renderable>(enemy, 0.9f, 0.2f, 0.2f);

        // Enemy AI: patrol toward nearest player unit
        lua.LoadScript(enemy, "OnTick", R"lua(
            function OnTick(entity_id)
                local enemies = Combat.GetEnemiesInRadius(entity_id, 20 * 256)
                local nearest, nearestDist = 0, 99999999
                for tid, hp in pairs(enemies) do
                    local dist = Transform.GetDistance(entity_id, tid)
                    if dist < nearestDist then nearest = tid; nearestDist = dist end
                end
                if nearest > 0 then
                    Combat.DealDamage(nearest, 5 * 256, 0)
                    Orders.AttackTarget(entity_id, nearest)
                end
            end
        )lua");
    }

    // ── Camera + Selection State ────────────────────────────
    int camX = 0, camY = 0;
    beigebox::FixedPoint mouseWorldX = beigebox::FixedPoint::FromInt(0);
    beigebox::FixedPoint mouseWorldY = beigebox::FixedPoint::FromInt(0);
    entt::entity selectedEntity = entt::null;
    int scrollSpeed = 4;
    bool showHud = true;

    // ── Game Loop ───────────────────────────────────────────
    bool running = true;
    SDL_Event event;
    Uint32 lastLogic = SDL_GetTicks();
    Uint32 logicAccum = 0;
    Uint32 frameCount = 0;
    Uint32 lastFpsUpdate = SDL_GetTicks();
    float fps = 0.0f;

    while (running)
    {
        Uint32 now = SDL_GetTicks();
        Uint32 frameDelta = now - lastLogic;
        lastLogic = now;
        logicAccum += frameDelta;
        if (logicAccum > 200) logicAccum = 200; // cap spiral of death

        // ── Input ───────────────────────────────────────────
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F1) showHud = !showHud;

            if (event.type == SDL_MOUSEMOTION) {
                int mx = event.motion.x + camX;
                int my = event.motion.y + camY;
                int tx, ty;
                beigebox::ScreenToWorld(mx, my, tx, ty);
                mouseWorldX = beigebox::FixedPoint::FromInt(tx);
                mouseWorldY = beigebox::FixedPoint::FromInt(ty);
            }

            if (event.type == SDL_MOUSEBUTTONDOWN)
            {
                int mx = event.button.x + camX;
                int my = event.button.y + camY;
                int tx, ty;
                beigebox::ScreenToWorld(mx, my, tx, ty);

                if (event.button.button == SDL_BUTTON_LEFT)
                {
                    // Find entity at click position
                    selectedEntity = entt::null;
                    beigebox::FixedPoint clickX = beigebox::FixedPoint::FromInt(tx);
                    beigebox::FixedPoint clickY = beigebox::FixedPoint::FromInt(ty);
                    auto view = ecs.view<beigebox::Transform, beigebox::Player>();
                    for (auto e : view) {
                        auto& pt = ecs.get<beigebox::Transform>(e);
                        beigebox::FixedPoint dx = beigebox::Abs(pt.x - clickX);
                        beigebox::FixedPoint dy = beigebox::Abs(pt.y - clickY);
                        if (dx <= beigebox::FixedPoint::FromInt(1) && dy <= beigebox::FixedPoint::FromInt(1)) {
                            auto& pp = ecs.get<beigebox::Player>(e);
                            if (pp.factionId == 1) { selectedEntity = e; break; }
                        }
                    }
                }
                else if (event.button.button == SDL_BUTTON_RIGHT && selectedEntity != entt::null)
                {
                    // Find target at click position
                    beigebox::FixedPoint clickX = beigebox::FixedPoint::FromInt(tx);
                    beigebox::FixedPoint clickY = beigebox::FixedPoint::FromInt(ty);
                    entt::entity target = entt::null;

                    auto view = ecs.view<beigebox::Transform, beigebox::Player>();
                    for (auto e : view) {
                        auto& pt = ecs.get<beigebox::Transform>(e);
                        beigebox::FixedPoint dx = beigebox::Abs(pt.x - clickX);
                        beigebox::FixedPoint dy = beigebox::Abs(pt.y - clickY);
                        if (dx <= beigebox::FixedPoint::FromInt(1) && dy <= beigebox::FixedPoint::FromInt(1)) {
                            if (ecs.get<beigebox::Player>(e).factionId != 1) { target = e; break; }
                        }
                    }

                    if (target != entt::null) {
                        // Attack target
                        auto& tt = ecs.get<beigebox::Transform>(target);
                        ecs.emplace_or_replace<beigebox::Movement>(selectedEntity,
                            tt.x, tt.y, beigebox::FixedPoint::FromInt(3));
                        lua.FireEvent(selectedEntity, "OnRightClick");
                    } else {
                        // Move to position
                        ecs.emplace_or_replace<beigebox::Movement>(selectedEntity,
                            clickX, clickY, beigebox::FixedPoint::FromInt(2));
                    }
                }
            }

            // Mouse wheel zoom placeholder
            if (event.type == SDL_MOUSEWHEEL)
                scrollSpeed = std::max(1, std::min(16, scrollSpeed + event.wheel.y));
        }

        // ── Fixed Timestep Logic ────────────────────────────
        while (logicAccum >= LOGIC_MS)
        {
            logicAccum -= LOGIC_MS;

            // Thaw tick
            thaw.Tick();

            // Copy thaw data for overlay rendering
            for (int y = 0; y < MAP_H; ++y)
                for (int x = 0; x < MAP_W; ++x) {
                    int idx = y * MAP_W + x;
                    heatData[idx] = thaw.GetHeat(x, y);
                    uint8_t st = 0;
                    if (!thaw.IsFrozen(x, y)) st = thaw.IsBuildable(x, y) ? 2 : 1;
                    stateData[idx] = st;
                }

            // ECS systems
            beigebox::TickSystems(ecs);

            // Fire Lua events
            ecs.view<entt::entity>().each([&](entt::entity entity) {
                if (lua.HasScript(entity, "OnTick")) lua.FireEvent(entity, "OnTick");
            });
        }

        // ── Continuous Input ────────────────────────────────
        const Uint8* kb = SDL_GetKeyboardState(nullptr);
        if (kb[SDL_SCANCODE_LEFT]  || kb[SDL_SCANCODE_A]) camX -= scrollSpeed;
        if (kb[SDL_SCANCODE_RIGHT] || kb[SDL_SCANCODE_D]) camX += scrollSpeed;
        if (kb[SDL_SCANCODE_UP]    || kb[SDL_SCANCODE_W]) camY -= scrollSpeed;
        if (kb[SDL_SCANCODE_DOWN]  || kb[SDL_SCANCODE_S]) camY += scrollSpeed;

        // ── FPS Counter ─────────────────────────────────────
        frameCount++;
        if (now - lastFpsUpdate >= 1000) {
            fps = frameCount * 1000.0f / (now - lastFpsUpdate);
            frameCount = 0; lastFpsUpdate = now;
        }

        // ── Win Check ───────────────────────────────────────
        int enemyCount = 0, playerCount = 0;
        ecs.view<beigebox::Player, beigebox::Health>().each([&](auto e, auto& p, auto& h) {
            if (h.current.Raw() <= 0) return;
            if (p.factionId == 1) playerCount++;
            else if (p.factionId == 2) enemyCount++;
        });
        if (enemyCount == 0) { SDL_Log("VICTORY! All enemies eliminated."); }
        if (playerCount == 0) { SDL_Log("DEFEAT! All your units destroyed."); }

        // ── Render ──────────────────────────────────────────
        renderer.BeginFrame();
        renderer.DrawTileGrid(MAP_W, MAP_H, camX, camY);
        renderer.DrawThawOverlay(MAP_W, MAP_H, camX, camY, heatData.data(), stateData.data());
        renderer.EndFrame();

        // ── Console HUD (throttled) ──────────────────────────
        static Uint32 lastHudLog = 0;
        if (showHud && now - lastHudLog >= 1000)
        {
            lastHudLog = now;
            SDL_Log("FPS:%.0f | Cam:(%d,%d) | Player:%d Enemy:%d | Sel:%s",
                fps, camX, camY, playerCount, enemyCount,
                selectedEntity != entt::null ? "unit" : "none");
        }

        SDL_Delay(1); // don't burn CPU
    }

    // ── Shutdown ─────────────────────────────────────────────
    lua.Shutdown();
    renderer.Shutdown();
    SDL_GL_DeleteContext(glCtx);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return EXIT_SUCCESS;
}
