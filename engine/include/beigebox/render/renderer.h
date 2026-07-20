// engine/include/beigebox/render/renderer.h
// ─────────────────────────────────────────────────────────────
// Minimal OpenGL 3.1 renderer for the isometric tile grid.
//
// Creates and manages the GL context, compiles shaders, and
// draws the tile grid as colored diamond quads.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <cstdint>

namespace beigebox {

struct TileColor {
    float r, g, b;
};

class Renderer {
public:
    Renderer() = default;
    ~Renderer();

    // Non-copyable (owns GL resources)
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Initialize GL context and shaders. Must be called after SDL window creation.
    bool Init(SDL_Window* window);

    // Free all GL resources.
    void Shutdown();

    // Clear the frame and prepare for drawing.
    void BeginFrame();

    // Swap buffers.
    void EndFrame();

    // Draw an isometric tile grid.
    // gridW, gridH: grid dimensions in tiles
    // cameraX, cameraY: top-left screen offset in pixels (for scrolling)
    void DrawTileGrid(int gridW, int gridH, int cameraX, int cameraY);

    // Get screen dimensions from the current viewport.
    int ScreenWidth() const { return screenW_; }
    int ScreenHeight() const { return screenH_; }

private:
    SDL_GLContext glContext_ = nullptr;
    int screenW_ = 0;
    int screenH_ = 0;

    // OpenGL objects
    GLuint shaderProgram_ = 0;
    GLuint vao_ = 0;
    GLuint vbo_ = 0;
    GLuint ebo_ = 0;

    // Uniform locations
    GLint uScreenSize_ = -1;
    GLint uCameraOffset_ = -1;

    // Helpers
    GLuint CompileShader(GLenum type, const char* source);
    GLuint CreateShaderProgram(const char* vertexSrc, const char* fragmentSrc);
    void SetupTileGeometry(int gridW, int gridH);
};

} // namespace beigebox
