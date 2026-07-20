// editor/src/panels/viewport.h
// ─────────────────────────────────────────────────────────────
// Game Viewport + Level Designer Panel
//
// Renders the isometric tile grid into an FBO, displays it as
// an ImGui image. Supports iso/top-down toggle, camera pan/zoom,
// tile painting, and entity placement.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <entt/entt.hpp>
#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

#include "beigebox/render/renderer.h"
#include "beigebox/world/thaw_grid.h"
#include "beigebox/world/map_generator.h"
#include "beigebox/lua/lua_bridge.h"

#include <imgui.h>

#include <vector>
#include <functional>

namespace beigebox {

class UndoManager;

class ViewportPanel
{
public:
    ViewportPanel(entt::registry& ecs, Renderer& renderer,
                  ThawGrid& thaw, LuaBridge& lua);

    void SetUndoManager(UndoManager* um) { undoManager_ = um; }
    void SetMapTiles(std::vector<MapTile>* tiles, int w, int h) {
        mapTiles_ = tiles; mapW_ = w; mapH_ = h;
    }

    // Callbacks
    using EntityCallback = std::function<void(entt::entity)>;
    EntityCallback onEntitySelected;

    void Draw();

    // Toggle rendering mode
    bool IsIsometric() const { return isometric_; }
    void ToggleProjection() { isometric_ = !isometric_; }

private:
    void InitFramebuffer();
    void RenderToFramebuffer();
    void HandleMouseInput(const ImVec2& imagePos, const ImVec2& imageSize);

    entt::registry*  ecs_;
    Renderer*        renderer_;
    ThawGrid*        thaw_;
    LuaBridge*       lua_;
    UndoManager*     undoManager_ = nullptr;

    std::vector<MapTile>* mapTiles_ = nullptr;
    int mapW_ = 24, mapH_ = 24;

    // FBO for render-to-texture
    GLuint fbo_ = 0;
    GLuint fboTex_ = 0;
    int fboW_ = 1024, fboH_ = 768;

    // Camera
    float camX_ = 0, camY_ = 0;
    float zoom_ = 1.0f;
    bool isometric_ = true;

    // Tool state
    int  selectedTerrain_ = 0;  // TileTerrain enum index
    bool paintMode_ = false;
};

} // namespace beigebox
