// editor/src/panels/viewport.cpp
// ─────────────────────────────────────────────────────────────
// Game Viewport + Level Designer — implementation.
// ─────────────────────────────────────────────────────────────

#include "viewport.h"
#include "entity_editor.h"
#include "../undo/undo_manager.h"

#include <imgui.h>
#include <SDL2/SDL.h>

namespace beigebox {

ViewportPanel::ViewportPanel(entt::registry& ecs, Renderer& renderer,
                             ThawGrid& thaw, LuaBridge& lua)
    : ecs_(&ecs), renderer_(&renderer), thaw_(&thaw), lua_(&lua)
{
    InitFramebuffer();
}

void ViewportPanel::InitFramebuffer()
{
    // Create FBO
    glGenFramebuffers(1, &fbo_);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);

    // Create color attachment texture
    glGenTextures(1, &fboTex_);
    glBindTexture(GL_TEXTURE_2D, fboTex_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fboW_, fboH_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fboTex_, 0);

    // Create depth renderbuffer
    GLuint rbo;
    glGenRenderbuffers(1, &rbo);
    glBindRenderbuffer(GL_RENDERBUFFER, rbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, fboW_, fboH_);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, rbo);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ViewportPanel::RenderToFramebuffer()
{
    // Save current viewport
    GLint prevVp[4];
    glGetIntegerv(GL_VIEWPORT, prevVp);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo_);
    glViewport(0, 0, fboW_, fboH_);
    glClearColor(0.08f, 0.08f, 0.12f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (mapTiles_)
        renderer_->DrawTileGrid(mapW_, mapH_, static_cast<int>(camX_), static_cast<int>(camY_));

    // Draw entity overlays as colored diamonds
    auto view = ecs_->view<Transform, Player>();
    // (Simple diamond rendering would go here — using tile grid for now)

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(prevVp[0], prevVp[1], prevVp[2], prevVp[3]);
}

void ViewportPanel::Draw()
{
    ImGui::Begin("Game Viewport");

    DrawToolbar();
    ImGui::Separator();

    // ── Two-column: viewport + side panel ────────────────────
    float sideWidth = 180;
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 viewportSize(avail.x - sideWidth - 10, avail.y);

    // ── Viewport Image ───────────────────────────────────────
    ImGui::BeginChild("##viewportArea", viewportSize, false);

    if (static_cast<int>(viewportSize.x) != fboW_ || static_cast<int>(viewportSize.y) != fboH_)
    {
        fboW_ = static_cast<int>(viewportSize.x);
        fboH_ = static_cast<int>(viewportSize.y);
    }

    RenderToFramebuffer();

    ImVec2 cursorPos = ImGui::GetCursorPos();
    ImGui::Image((ImTextureID)(intptr_t)fboTex_,
        viewportSize, ImVec2(0, 1), ImVec2(1, 0));

    if (ImGui::IsItemHovered())
    {
        ImVec2 imagePos = ImGui::GetItemRectMin();
        HandleMouseInput(imagePos, viewportSize);

        float wheel = ImGui::GetIO().MouseWheel;
        zoom_ += wheel * 0.1f;
        if (zoom_ < 0.25f) zoom_ = 0.25f;
        if (zoom_ > 4.0f)  zoom_ = 4.0f;

        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        {
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
            camX_ -= delta.x / zoom_;
            camY_ -= delta.y / zoom_;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
        }
    }

    // ── Minimap overlay ──────────────────────────────────────
    ImVec2 miniPos(ImGui::GetCursorPosX() + viewportSize.x - 140,
                   ImGui::GetCursorPosY() - viewportSize.y + 5);
    ImGui::SetCursorPos(ImVec2(viewportSize.x - 140, 5));
    RenderMinimap();

    ImGui::EndChild();
    ImGui::SameLine();

    // ── Side panel ───────────────────────────────────────────
    ImGui::BeginChild("##viewportSide", ImVec2(sideWidth, 0), false);
    DrawLayerPanel();
    ImGui::Separator();
    DrawObjectPalette();
    ImGui::EndChild();

    ImGui::End();
}

// ── Toolbar ──────────────────────────────────────────────────

void ViewportPanel::DrawToolbar()
{
    if (ImGui::Button(isometric_ ? "Iso" : "Top"))
        ToggleProjection();
    ImGui::SameLine();
    ImGui::Text("Zoom:%.1f", zoom_);
    ImGui::SameLine();

    const char* terrains[] = {"Frozen", "Thawed", "Salvage", "Geothermal", "Impassable"};
    ImGui::SetNextItemWidth(100);
    ImGui::Combo("Brush", &selectedTerrain_, terrains, 5);
    ImGui::SameLine();

    ImGui::SetNextItemWidth(50);
    ImGui::InputInt("Size", &brushSize_);
    if (brushSize_ < 1) brushSize_ = 1;
    if (brushSize_ > 8) brushSize_ = 8;
    ImGui::SameLine();

    if (ImGui::Button("Save Map"))
        SaveMap();
    ImGui::SameLine();
    if (ImGui::Button("Load Map"))
        LoadMap();
    ImGui::SameLine();
    ImGui::TextDisabled("%dx%d", mapW_, mapH_);
}

// ── Layer Panel ──────────────────────────────────────────────

void ViewportPanel::DrawLayerPanel()
{
    ImGui::Text("Layers");
    ImGui::Checkbox("Terrain", &showTerrain_);
    ImGui::Checkbox("Overlay", &showOverlay_);
    ImGui::Checkbox("Entities", &showEntities_);
    ImGui::Checkbox("Regions", &showRegions_);
}

// ── Object Palette ───────────────────────────────────────────

void ViewportPanel::DrawObjectPalette()
{
    ImGui::Text("Place Object");
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
    ImGui::InputTextWithHint("##palFilter", "Filter...", paletteFilter_, sizeof(paletteFilter_));

    // Lazy-populate palette from EntityEditor
    if (paletteEntries_.empty() && entityEditor_)
    {
        for (auto& name : entityEditor_->UnitTypeNames())
            paletteEntries_.push_back({name, "entity"});
        // Built-in map objects
        paletteEntries_.push_back({"GeothermalVent", "object"});
        paletteEntries_.push_back({"SalvagePile", "object"});
        paletteEntries_.push_back({"Rubble", "object"});
        paletteEntries_.push_back({"Tree", "object"});
        paletteEntries_.push_back({"Rock", "object"});
        paletteEntries_.push_back({"Spawn Point", "spawn"});
    }

    ImGui::BeginChild("##paletteList", ImVec2(0, 0), true);
    for (int i = 0; i < static_cast<int>(paletteEntries_.size()); ++i)
    {
        auto& pe = paletteEntries_[i];
        if (paletteFilter_[0])
        {
            std::string nl = pe.name, fl = paletteFilter_;
            std::transform(nl.begin(), nl.end(), nl.begin(), ::tolower);
            std::transform(fl.begin(), fl.end(), fl.begin(), ::tolower);
            if (nl.find(fl) == std::string::npos) continue;
        }

        ImGui::PushID(i);
        ImGui::TextColored(pe.category == "entity" ? ImVec4(0.3f, 0.8f, 0.3f, 1.0f) :
                            pe.category == "object" ? ImVec4(0.8f, 0.7f, 0.3f, 1.0f) :
                            ImVec4(0.5f, 0.5f, 0.9f, 1.0f),
            "%s", pe.name.c_str());
        if (ImGui::IsItemClicked())
            selectedPaletteEntry_ = i;
        ImGui::PopID();
    }
    ImGui::EndChild();
}

// ── Minimap ─────────────────────────────────────────────────

void ViewportPanel::RenderMinimap()
{
    ImVec2 miniSize(130, 100);
    ImGui::BeginChild("##minimap", ImVec2(miniSize.x + 10, miniSize.y + 10), true,
        ImGuiWindowFlags_NoScrollbar);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p = ImGui::GetCursorScreenPos();

    // Background
    dl->AddRectFilled(p, ImVec2(p.x + miniSize.x, p.y + miniSize.y),
        IM_COL32(10, 10, 20, 255));

    // Draw tile grid as colored pixels
    if (mapTiles_)
    {
        float tx = miniSize.x / mapW_;
        float ty = miniSize.y / mapH_;
        for (int y = 0; y < mapH_; ++y)
        {
            for (int x = 0; x < mapW_; ++x)
            {
                auto& tile = (*mapTiles_)[y * mapW_ + x];
                ImU32 col;
                switch (tile.terrain)
                {
                    case TileTerrain::FrozenGround: col = IM_COL32(40, 60, 120, 255); break;
                    case TileTerrain::ThawedGround: col = IM_COL32(60, 100, 60, 255); break;
                    case TileTerrain::SalvageField: col = IM_COL32(120, 100, 40, 255); break;
                    case TileTerrain::Geothermal:   col = IM_COL32(180, 60, 30, 255); break;
                    case TileTerrain::Impassable:   col = IM_COL32(80, 80, 80, 255); break;
                    default: col = IM_COL32(0, 0, 0, 255);
                }
                dl->AddRectFilled(
                    ImVec2(p.x + x * tx, p.y + y * ty),
                    ImVec2(p.x + (x + 1) * tx, p.y + (y + 1) * ty),
                    col);
            }
        }
    }

    // Viewport rectangle
    if (mapTiles_)
    {
        float vx = (camX_ / 64.0f) / mapW_ * miniSize.x;
        float vy = (camY_ / 32.0f) / mapH_ * miniSize.y;
        float vw = (fboW_ / zoom_) / 64.0f / mapW_ * miniSize.x;
        float vh = (fboH_ / zoom_) / 32.0f / mapH_ * miniSize.y;
        dl->AddRect(ImVec2(p.x + vx, p.y + vy),
            ImVec2(p.x + vx + vw, p.y + vy + vh),
            IM_COL32(255, 255, 255, 200));
    }

    ImGui::EndChild();
}

// ── Map I/O ─────────────────────────────────────────────────

void ViewportPanel::SaveMap()
{
    if (!mapTiles_) return;
    std::string path = mapPathBuf_[0] ? mapPathBuf_ : "current_map.ogm.json";
    MapGenerator::SaveToJson(path, mapTiles_->data(), mapW_, mapH_, mapSeed_);
    snprintf(mapPathBuf_, sizeof(mapPathBuf_), "%s", path.c_str());
}

void ViewportPanel::LoadMap()
{
    std::vector<MapTile> tiles;
    int w, h, seed;
    std::string path = mapPathBuf_[0] ? mapPathBuf_ : "current_map.ogm.json";
    if (MapGenerator::LoadFromJson(path, tiles, w, h, seed))
    {
        if (mapTiles_)
        {
            *mapTiles_ = tiles;
            mapW_ = w;
            mapH_ = h;
            mapSeed_ = seed;
        }
    }
}

// ── Mouse Input (upgraded) ──────────────────────────────────

void ViewportPanel::HandleMouseInput(const ImVec2& imagePos, const ImVec2& imageSize)
{
    if (!mapTiles_) return;

    ImVec2 mousePos = ImGui::GetMousePos();
    float localX = (mousePos.x - imagePos.x) / imageSize.x;
    float localY = (mousePos.y - imagePos.y) / imageSize.y;

    int tileX = static_cast<int>(localX * mapW_ + camX_ / 64.0f);
    int tileY = static_cast<int>(localY * mapH_ + camY_ / 32.0f);

    if (tileX < 0 || tileX >= mapW_ || tileY < 0 || tileY >= mapH_) return;

    // Left-click: paint terrain with brush size
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || 
        (ImGui::IsMouseDown(ImGuiMouseButton_Left) && brushSize_ > 1))
    {
        for (int dy = -brushSize_/2; dy <= brushSize_/2; ++dy)
        {
            for (int dx = -brushSize_/2; dx <= brushSize_/2; ++dx)
            {
                int tx = tileX + dx;
                int ty = tileY + dy;
                if (tx >= 0 && tx < mapW_ && ty >= 0 && ty < mapH_)
                {
                    int idx = ty * mapW_ + tileX;
                    (*mapTiles_)[idx].terrain = static_cast<TileTerrain>(selectedTerrain_);
                }
            }
        }
    }

    // Right-click: place selected palette object
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right) && selectedPaletteEntry_ >= 0
        && selectedPaletteEntry_ < static_cast<int>(paletteEntries_.size()))
    {
        auto& pe = paletteEntries_[selectedPaletteEntry_];
        if (pe.category == "spawn")
        {
            // Place spawn point
            // (In future: store in map JSON spawns array)
        }
        else
        {
            auto entity = ecs_->create();
            ecs_->emplace<Transform>(entity,
                FixedPoint::FromInt(tileX), FixedPoint::FromInt(tileY));
            ecs_->emplace<Health>(entity,
                FixedPoint::FromInt(100), FixedPoint::FromInt(100));
            ecs_->emplace<Player>(entity, spawnFaction_);
            ecs_->emplace<Renderable>(entity, 0.3f, 0.7f, 0.3f);
            if (onEntitySelected) onEntitySelected(entity);
        }
    }
}

} // namespace beigebox
