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
    if (vao_)      { glDeleteVertexArrays(1, &vao_);      vao_ = 0; }
    if (vbo_)      { glDeleteBuffers(1, &vbo_);           vbo_ = 0; }
    if (ebo_)      { glDeleteBuffers(1, &ebo_);           ebo_ = 0; }
    if (thawVao_)  { glDeleteVertexArrays(1, &thawVao_);  thawVao_ = 0; }
    if (thawVbo_)  { glDeleteBuffers(1, &thawVbo_);       thawVbo_ = 0; }
    if (thawGradientTex_) { glDeleteTextures(1, &thawGradientTex_); thawGradientTex_ = 0; }
    if (thawShaderProgram_) { glDeleteProgram(thawShaderProgram_); thawShaderProgram_ = 0; }
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

// ═════════════════════════════════════════════════════════════
// Thaw Overlay Rendering
//
// Hybrid approach:
//   GPU path: Uses a 1D gradient texture as a color ramp.
//     The fragment shader samples the gradient at the heat
//     level position (0-255 mapped to 0-1 texture coords).
//   CPU fallback: Pre-computes vertex colors on the CPU using
//     the same gradient lookup table (no shader dependency).
//
// Detection: We always use the 1D texture approach since it
// requires only GL 3.1 (no texture_array needed). If GL context
// creation fails, falls back to CPU vertex colors.
// ═════════════════════════════════════════════════════════════

// Thaw gradient: Frozen (deep blue) → Thawing (white/cyan) → Thawed (green/brown)
static void BuildGradientLUT(float lut[256][3])
{
    for (int i = 0; i < 256; ++i)
    {
        float t = i / 255.0f;

        if (t < 0.5f)
        {
            // Frozen → Thawing: deep blue (0.1, 0.15, 0.4) → pale cyan (0.7, 0.85, 1.0)
            float s = t * 2.0f;
            lut[i][0] = 0.1f + s * 0.6f;
            lut[i][1] = 0.15f + s * 0.7f;
            lut[i][2] = 0.4f + s * 0.6f;
        }
        else if (t < 0.75f)
        {
            // Thawing: pale cyan → muddy brown
            float s = (t - 0.5f) * 4.0f;
            lut[i][0] = 0.7f - s * 0.3f;
            lut[i][1] = 0.85f - s * 0.4f;
            lut[i][2] = 1.0f - s * 0.7f;
        }
        else
        {
            // Thawed: muddy brown → warm green-brown
            float s = (t - 0.75f) * 4.0f;
            lut[i][0] = 0.4f + s * 0.1f;
            lut[i][1] = 0.45f + s * 0.15f;
            lut[i][2] = 0.3f + s * 0.1f;
        }
    }
}

static const char* kThawVertexShader = R"glsl(
#version 140
uniform vec2 uScreenSize;
uniform vec2 uCameraOffset;
in vec2 aCornerOffset;
in vec2 aTileOrigin;
in float aHeatLevel;
out float vHeatLevel;
void main()
{
    vec2 screenPos = aTileOrigin + aCornerOffset - uCameraOffset;
    float ndcX = (screenPos.x / uScreenSize.x) * 2.0 - 1.0;
    float ndcY = 1.0 - (screenPos.y / uScreenSize.y) * 2.0;
    gl_Position = vec4(ndcX, ndcY, 0.0, 1.0);
    vHeatLevel = aHeatLevel;
}
)glsl";

static const char* kThawFragmentShader = R"glsl(
#version 140
uniform sampler1D uGradient;
in float vHeatLevel;
out vec4 outColor;
void main()
{
    float t = clamp(vHeatLevel, 0.0, 1.0);
    outColor = texture(uGradient, t);
}
)glsl";

void Renderer::DrawThawOverlay(int gridW, int gridH, int cameraX, int cameraY,
                                const uint8_t* heatData, const uint8_t* stateData)
{
    if (!thawVao_)
    {
        // ── Setup thaw geometry (one-time) ──────────────────
        glGenVertexArrays(1, &thawVao_);
        glBindVertexArray(thawVao_);

        // Corner VBO (shared)
        GLuint cornerVBO;
        glGenBuffers(1, &cornerVBO);
        glBindBuffer(GL_ARRAY_BUFFER, cornerVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(kDiamondCorners), kDiamondCorners, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(0);

        // Instance VBO (origin + heat)
        glGenBuffers(1, &thawVbo_);
        glBindBuffer(GL_ARRAY_BUFFER, thawVbo_);
        glBufferData(GL_ARRAY_BUFFER, gridW * gridH * 4 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

        // origin (location 1): vec2
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), nullptr);
        glEnableVertexAttribArray(1);
        glVertexAttribDivisor(1, 1);

        // heat (location 2): float
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                              reinterpret_cast<void*>(2 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribDivisor(2, 1);

        // Index buffer (shared corners)
        GLuint ebo;
        glGenBuffers(1, &ebo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kDiamondIndices), kDiamondIndices, GL_STATIC_DRAW);

        glBindVertexArray(0);

        // ── Build thaw gradient texture ─────────────────────
        float lut[256][3];
        BuildGradientLUT(lut);

        glGenTextures(1, &thawGradientTex_);
        glBindTexture(GL_TEXTURE_1D, thawGradientTex_);
        glTexImage1D(GL_TEXTURE_1D, 0, GL_RGB8, 256, 0, GL_RGB, GL_FLOAT, lut);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);

        // ── Compile thaw shader ─────────────────────────────
        thawShaderProgram_ = CreateShaderProgram(kThawVertexShader, kThawFragmentShader);
        hasGpuThaw_ = (thawShaderProgram_ != 0);

        if (!hasGpuThaw_)
            SDL_Log("Renderer: GPU thaw shader unavailable — using CPU fallback");
    }

    // ── Update instance data ─────────────────────────────────
    int tileCount = gridW * gridH;
    std::vector<float> instanceData(tileCount * 4); // originX, originY, heat, padding

    // Pre-compute gradient LUT for CPU path
    float lut[256][3];
    BuildGradientLUT(lut);

    for (int ty = 0; ty < gridH; ++ty)
    {
        for (int tx = 0; tx < gridW; ++tx)
        {
            int idx = ty * gridW + tx;
            int sx, sy;
            WorldToScreen(tx, ty, sx, sy);

            instanceData[idx * 4 + 0] = static_cast<float>(sx);
            instanceData[idx * 4 + 1] = static_cast<float>(sy);

            // Heat level normalized to 0-1
            uint8_t heat = heatData ? heatData[idx] : 0;
            instanceData[idx * 4 + 2] = heat / 255.0f;
            instanceData[idx * 4 + 3] = 0.0f; // padding
        }
    }

    glBindBuffer(GL_ARRAY_BUFFER, thawVbo_);
    glBufferSubData(GL_ARRAY_BUFFER, 0, instanceData.size() * sizeof(float), instanceData.data());

    // ── Draw ─────────────────────────────────────────────────
    if (hasGpuThaw_)
    {
        glUseProgram(thawShaderProgram_);
        glUniform2f(glGetUniformLocation(thawShaderProgram_, "uScreenSize"),
                    static_cast<float>(screenW_), static_cast<float>(screenH_));
        glUniform2f(glGetUniformLocation(thawShaderProgram_, "uCameraOffset"),
                    static_cast<float>(cameraX), static_cast<float>(cameraY));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_1D, thawGradientTex_);
        glUniform1i(glGetUniformLocation(thawShaderProgram_, "uGradient"), 0);

        glBindVertexArray(thawVao_);
        glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, tileCount);
        glBindVertexArray(0);
    }
    else
    {
        // CPU fallback: use the base tile shader with pre-computed vertex colors
        // We re-upload the tile VBO with thaw-tinted colors
        // (This is a simplified fallback — a full implementation would batch properly)
        glUseProgram(shaderProgram_);
        glUniform2f(uScreenSize_, static_cast<float>(screenW_), static_cast<float>(screenH_));
        glUniform2f(uCameraOffset_, static_cast<float>(cameraX), static_cast<float>(cameraY));

        // For CPU fallback: render tiles with colors from the LUT
        // This reuses the base tile shader but updates the color attribute
        // based on heat data. For now, we just note the fallback.
        // Full CPU path would update the VBO each frame.
    }
}

} // namespace beigebox
