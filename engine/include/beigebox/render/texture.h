// engine/include/beigebox/render/texture.h
// ─────────────────────────────────────────────────────────────
// OpenGL Texture wrapper — loads PNG via STB_image.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <string>

namespace beigebox {

class Texture
{
public:
    Texture() = default;
    ~Texture();

    Texture(const Texture&) = delete;
    Texture& operator=(const Texture&) = delete;
    Texture(Texture&& other) noexcept;
    Texture& operator=(Texture&& other) noexcept;

    // Load from PNG file. Returns true on success.
    bool Load(const std::string& path);

    // Create a 1x1 solid-color texture (for fallback rendering).
    bool CreateSolidColor(uint8_t r, uint8_t g, uint8_t b);

    // Bind to the given texture unit.
    void Bind(int unit = 0) const;

    int Width()  const { return width_; }
    int Height() const { return height_; }
    GLuint ID()  const { return id_; }
    bool  Valid() const { return id_ != 0; }

private:
    void Release();

    GLuint id_ = 0;
    int width_  = 0;
    int height_ = 0;
};

} // namespace beigebox
