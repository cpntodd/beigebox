// engine/src/render/renderer.cpp
// ─────────────────────────────────────────────────────────────
// OpenGL 3.1 isometric tile renderer implementation.
//
// Uses a single VAO with instanced tile quads. The vertex shader
// applies the isometric projection; the fragment shader assigns
// a checkerboard color based on tile coordinates.
// ─────────────────────────────────────────────────────────────

#include "beigebox/render/renderer.h"
#include "beigebox/render/isometric.h"
#include <cstdio>
#include <cstddef>
#include <vector>

namespace beigebox {

// ── Vertex Shader (GLSL 1.40) ───────────────────────────────
// Transforms a diamond tile quad from isometric world-space to
// normalized device coordinates.

static const char* kVertexShader = R"glsl(
#version 140

uniform vec2 uScreenSize;    // viewport width, height
uniform vec2 uCameraOffset;  // scroll offset in pixels

in vec2 aCornerOffset;       // corner offset from tile center (in pixels)
in vec2 aTileOrigin;         // screen-space origin of this tile (in pixels)
in vec3 aColor;              // tile color

out vec3 vColor;

void main()
{
    // Compute final screen position: tile origin + corner offset - camera
    vec2 screenPos = aTileOrigin + aCornerOffset - uCameraOffset;

    // Convert pixel coords to NDC ([-1, 1] range, Y flipped for OpenGL)
    float ndcX = (screenPos.x / uScreenSize.x) * 2.0 - 1.0;
    float ndcY = 1.0 - (screenPos.y / uScreenSize.y) * 2.0;

    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
    vColor = aColor;
}
)glsl";

// ── Fragment Shader (GLSL 1.40) ─────────────────────────────
static const char* kFragmentShader = R"glsl(
#version 140

in vec3 vColor;
out vec4 outColor;

void main()
{
    outColor = vec4(vColor, 1.0);
}
)glsl";

// ── Corner offsets for a diamond tile ───────────────────────
// Diamond shape: top, right, bottom, left points
// (centered at origin)
static const float kDiamondCorners[] = {
     0.0f,  -TILE_HH,  // top
     TILE_HW,  0.0f,   // right
     0.0f,   TILE_HH,  // bottom
    -TILE_HW,  0.0f    // left
};

static const unsigned int kDiamondIndices[] = {
    0, 1, 2,  // top-right triangle
    0, 2, 3   // bottom-left triangle
};

// ── Tile vertex structure (per-tile instance data) ──────────
struct TileVertex {
    float originX, originY;  // tile screen-space origin
    float r, g, b;           // tile color
};

// ── Lifecycle ────────────────────────────────────────────────

Renderer::~Renderer() { Shutdown(); }

bool Renderer::Init(SDL_Window* window)
{
    SDL_GL_GetDrawableSize(window, &screenW_, &screenH_);

    glContext_ = SDL_GL_CreateContext(window);
    if (!glContext_)
    {
        SDL_Log("Renderer: SDL_GL_CreateContext failed: %s", SDL_GetError());
        return false;
    }

    SDL_GL_MakeCurrent(window, glContext_);

    // ── Compile shaders ─────────────────────────────────────
    shaderProgram_ = CreateShaderProgram(kVertexShader, kFragmentShader);
    if (!shaderProgram_)
        return false;

    // ── Retrieve uniform locations ──────────────────────────
    uScreenSize_   = glGetUniformLocation(shaderProgram_, "uScreenSize");
    uCameraOffset_ = glGetUniformLocation(shaderProgram_, "uCameraOffset");

    // ── Global GL state ─────────────────────────────────────
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0.08f, 0.08f, 0.12f, 1.0f);  // dark background

    SDL_Log("Renderer: initialized (%dx%d)", screenW_, screenH_);
    return true;
}

void Renderer::Shutdown()
{
    if (vao_)  { glDeleteVertexArrays(1, &vao_);  vao_ = 0; }
    if (vbo_)  { glDeleteBuffers(1, &vbo_);       vbo_ = 0; }
    if (ebo_)  { glDeleteBuffers(1, &ebo_);       ebo_ = 0; }
    if (shaderProgram_) { glDeleteProgram(shaderProgram_); shaderProgram_ = 0; }
    if (glContext_)
    {
        SDL_GL_DeleteContext(glContext_);
        glContext_ = nullptr;
    }
}

void Renderer::BeginFrame()
{
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::EndFrame()
{
    SDL_GL_SwapWindow(SDL_GL_GetCurrentWindow());
}

// ── Shader Compilation Helpers ──────────────────────────────

GLuint Renderer::CompileShader(GLenum type, const char* source)
{
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (!compiled)
    {
        GLint len = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len);
        glGetShaderInfoLog(shader, len, &len, log.data());
        SDL_Log("Renderer: shader compile error: %s", log.data());
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

GLuint Renderer::CreateShaderProgram(const char* vertexSrc, const char* fragmentSrc)
{
    GLuint vs = CompileShader(GL_VERTEX_SHADER, vertexSrc);
    GLuint fs = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);
    if (!vs || !fs)
    {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return 0;
    }

    GLuint program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    GLint linked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &linked);
    if (!linked)
    {
        GLint len = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &len);
        std::vector<char> log(len);
        glGetProgramInfoLog(program, len, &len, log.data());
        SDL_Log("Renderer: shader link error: %s", log.data());
        glDeleteProgram(program);
        program = 0;
    }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return program;
}

// ── Tile Grid Geometry ──────────────────────────────────────

void Renderer::SetupTileGeometry(int gridW, int gridH)
{
    // ── Create VAO ──────────────────────────────────────────
    glGenVertexArrays(1, &vao_);
    glBindVertexArray(vao_);

    // ── Static corner data (VBO, stream 0) ──────────────────
    // All tiles share the same diamond corner offsets.
    GLuint cornerVBO;
    glGenBuffers(1, &cornerVBO);
    glBindBuffer(GL_ARRAY_BUFFER, cornerVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kDiamondCorners), kDiamondCorners, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glEnableVertexAttribArray(0);

    // ── Static index buffer (EBO) ───────────────────────────
    glGenBuffers(1, &ebo_);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kDiamondIndices), kDiamondIndices, GL_STATIC_DRAW);

    // ── Per-tile instance data (VBO, stream 1) ──────────────
    // originX, originY, r, g, b  for each tile.
    int tileCount = gridW * gridH;
    std::vector<TileVertex> tiles(tileCount);

    for (int ty = 0; ty < gridH; ++ty)
    {
        for (int tx = 0; tx < gridW; ++tx)
        {
            int idx = ty * gridW + tx;
            int sx, sy;
            WorldToScreen(tx, ty, sx, sy);
            tiles[idx].originX = static_cast<float>(sx);
            tiles[idx].originY = static_cast<float>(sy);

            // Checkerboard colors — green and brown
            if ((tx + ty) % 2 == 0)
            {
                tiles[idx].r = 0.25f; tiles[idx].g = 0.55f; tiles[idx].b = 0.25f;
            }
            else
            {
                tiles[idx].r = 0.35f; tiles[idx].g = 0.45f; tiles[idx].b = 0.20f;
            }
        }
    }

    glGenBuffers(1, &vbo_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, tiles.size() * sizeof(TileVertex), tiles.data(), GL_DYNAMIC_DRAW);

    // origin (location 1): vec2
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(TileVertex),
                          reinterpret_cast<void*>(offsetof(TileVertex, originX)));
    glEnableVertexAttribArray(1);

    // color (location 2): vec3
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(TileVertex),
                          reinterpret_cast<void*>(offsetof(TileVertex, r)));
    glEnableVertexAttribArray(2);

    // Mark attribute 1 as instanced (one per tile), attribute 0 is shared corners
    glVertexAttribDivisor(1, 1); // advance per instance
    glVertexAttribDivisor(2, 1);

    glBindVertexArray(0);
}

// ── Public Draw Call ────────────────────────────────────────

void Renderer::DrawTileGrid(int gridW, int gridH, int cameraX, int cameraY)
{
    if (!vao_)
        SetupTileGeometry(gridW, gridH);

    glUseProgram(shaderProgram_);
    glUniform2f(uScreenSize_, static_cast<float>(screenW_), static_cast<float>(screenH_));
    glUniform2f(uCameraOffset_, static_cast<float>(cameraX), static_cast<float>(cameraY));

    glBindVertexArray(vao_);
    glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, gridW * gridH);
    glBindVertexArray(0);
}

} // namespace beigebox
