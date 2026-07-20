// editor/src/panels/viewport.cpp
// ─────────────────────────────────────────────────────────────
// Game Viewport + Level Designer — implementation.
// ─────────────────────────────────────────────────────────────

#include "viewport.h"
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

    // ── Toolbar ──────────────────────────────────────────────
    if (ImGui::Button(isometric_ ? "Iso ▦" : "Top ⬒"))
        ToggleProjection();
    ImGui::SameLine();
    ImGui::Text("Zoom: %.1fx", zoom_);
    ImGui::SameLine();
    const char* terrains[] = {"Frozen", "Thawed", "Salvage", "Geothermal", "Impassable"};
    ImGui::Combo("Brush", &selectedTerrain_, terrains, 5);

    // ── Viewport Image ───────────────────────────────────────
    ImVec2 avail = ImGui::GetContentRegionAvail();
    ImVec2 imageSize(avail.x, avail.y - 5);

    if (static_cast<int>(imageSize.x) != fboW_ || static_cast<int>(imageSize.y) != fboH_)
    {
        fboW_ = static_cast<int>(imageSize.x);
        fboH_ = static_cast<int>(imageSize.y);
        // Recreate FBO at new size (simplified — just resize next frame)
    }

    RenderToFramebuffer();

    ImVec2 cursorPos = ImGui::GetCursorPos();
    ImGui::Image((ImTextureID)(intptr_t)fboTex_,
        imageSize, ImVec2(0, 1), ImVec2(1, 0)); // flip Y

    // ── Mouse handling on the viewport ───────────────────────
    if (ImGui::IsItemHovered())
    {
        ImVec2 mousePos = ImGui::GetMousePos();
        ImVec2 imagePos = ImGui::GetItemRectMin();
        HandleMouseInput(imagePos, imageSize);

        // Scroll wheel → zoom
        float wheel = ImGui::GetIO().MouseWheel;
        zoom_ += wheel * 0.1f;
        if (zoom_ < 0.25f) zoom_ = 0.25f;
        if (zoom_ > 4.0f)  zoom_ = 4.0f;

        // Middle-click drag → pan
        if (ImGui::IsMouseDragging(ImGuiMouseButton_Middle))
        {
            ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Middle);
            camX_ -= delta.x / zoom_;
            camY_ -= delta.y / zoom_;
            ImGui::ResetMouseDragDelta(ImGuiMouseButton_Middle);
        }
    }

    ImGui::End();
}

void ViewportPanel::HandleMouseInput(const ImVec2& imagePos, const ImVec2& imageSize)
{
    if (!mapTiles_) return;

    ImVec2 mousePos = ImGui::GetMousePos();
    float localX = (mousePos.x - imagePos.x) / imageSize.x;
    float localY = (mousePos.y - imagePos.y) / imageSize.y;

    // Convert to world tile coordinates
    int tileX = static_cast<int>(localX * mapW_ + camX_ / 64.0f);
    int tileY = static_cast<int>(localY * mapH_ + camY_ / 32.0f);

    if (tileX < 0 || tileX >= mapW_ || tileY < 0 || tileY >= mapH_) return;

    // Left-click: paint terrain
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        int idx = tileY * mapW_ + tileX;
        (*mapTiles_)[idx].terrain = static_cast<TileTerrain>(selectedTerrain_);
    }

    // Right-click: place entity
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Right))
    {
        auto entity = ecs_->create();
        ecs_->emplace<Transform>(entity, FixedPoint::FromInt(tileX), FixedPoint::FromInt(tileY));
        ecs_->emplace<Health>(entity, FixedPoint::FromInt(100), FixedPoint::FromInt(100));
        ecs_->emplace<Player>(entity, 1);
        ecs_->emplace<Renderable>(entity, 0.3f, 0.7f, 0.3f);
        if (onEntitySelected) onEntitySelected(entity);
    }
}

} // namespace beigebox
