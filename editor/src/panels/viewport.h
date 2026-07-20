// editor/src/panels/viewport.h
// ─────────────────────────────────────────────────────────────
// Game Viewport + Map Editor Panel
//
// Features:
//   - Iso/top-down toggle, camera pan/zoom
//   - Terrain brush + object palette + spawn point placement
//   - Layer stack: terrain, overlay, resources, entities, regions
//   - FBO-rendered minimap in corner
//   - Wang tile autotile transition data
//   - JSON map format (.ogm.json)
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
#include <string>

namespace beigebox {

class UndoManager;
class EntityEditor;

class ViewportPanel
{
public:
    ViewportPanel(entt::registry& ecs, Renderer& renderer,
                  ThawGrid& thaw, LuaBridge& lua);

    void SetUndoManager(UndoManager* um) { undoManager_ = um; }
    void SetEntityEditor(EntityEditor* ee) { entityEditor_ = ee; }
    void SetMapTiles(std::vector<MapTile>* tiles, int w, int h) {
        mapTiles_ = tiles; mapW_ = w; mapH_ = h;
    }

    using EntityCallback = std::function<void(entt::entity)>;
    EntityCallback onEntitySelected;

    void Draw();
    bool IsIsometric() const { return isometric_; }
    void ToggleProjection() { isometric_ = !isometric_; }

private:
    void InitFramebuffer();
    void RenderToFramebuffer();
    void RenderMinimap();
    void HandleMouseInput(const ImVec2& imagePos, const ImVec2& imageSize);
    void DrawToolbar();
    void DrawObjectPalette();
    void DrawLayerPanel();
    void SaveMap();
    void LoadMap();

    entt::registry*  ecs_;
    Renderer*        renderer_;
    ThawGrid*        thaw_;
    LuaBridge*       lua_;
    UndoManager*     undoManager_ = nullptr;
    EntityEditor*    entityEditor_ = nullptr;

    std::vector<MapTile>* mapTiles_ = nullptr;
    int mapW_ = 32, mapH_ = 32;
    int mapSeed_ = 42;

    // FBOs
    GLuint fbo_ = 0, fboTex_ = 0;
    GLuint minimapFbo_ = 0, minimapTex_ = 0;
    int fboW_ = 1024, fboH_ = 768;

    // Camera
    float camX_ = 0, camY_ = 0;
    float zoom_ = 1.0f;
    bool isometric_ = true;

    // Tools
    int  selectedTerrain_ = 0;
    int  brushSize_ = 1;
    bool paintMode_ = false;

    // Layers
    bool showTerrain_   = true;
    bool showOverlay_   = true;
    bool showEntities_  = true;
    bool showRegions_   = false;

    // Object palette
    struct PaletteEntry { std::string name; std::string category; };
    std::vector<PaletteEntry> paletteEntries_;
    int  selectedPaletteEntry_ = -1;
    char paletteFilter_[64] = {};

    // Spawn mode
    bool spawnMode_ = false;
    int  spawnFaction_ = 1;

    // Map path
    char mapPathBuf_[512] = {};
};

} // namespace beigebox
