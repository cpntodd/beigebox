// engine/src/render/texture.cpp
// ─────────────────────────────────────────────────────────────
// OpenGL Texture — STB_image PNG loading.
// ─────────────────────────────────────────────────────────────

#include "beigebox/render/texture.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <cstdio>

namespace beigebox {

Texture::~Texture() { Release(); }

Texture::Texture(Texture&& other) noexcept
    : id_(other.id_), width_(other.width_), height_(other.height_)
{
    other.id_ = 0;
}

Texture& Texture::operator=(Texture&& other) noexcept
{
    if (this != &other) { Release(); id_ = other.id_; width_ = other.width_; height_ = other.height_; other.id_ = 0; }
    return *this;
}

bool Texture::Load(const std::string& path)
{
    Release();

    int channels;
    unsigned char* data = stbi_load(path.c_str(), &width_, &height_, &channels, 4); // force RGBA
    if (!data)
    {
        SDL_Log("Texture: failed to load '%s': %s", path.c_str(), stbi_failure_reason());
        return false;
    }

    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width_, height_, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    stbi_image_free(data);
    SDL_Log("Texture: loaded '%s' (%dx%d)", path.c_str(), width_, height_);
    return true;
}

bool Texture::CreateSolidColor(uint8_t r, uint8_t g, uint8_t b)
{
    Release();
    width_ = 1; height_ = 1;

    unsigned char pixel[] = {r, g, b, 255};

    glGenTextures(1, &id_);
    glBindTexture(GL_TEXTURE_2D, id_);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixel);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    return true;
}

void Texture::Bind(int unit) const
{
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, id_);
}

void Texture::Release()
{
    if (id_) { glDeleteTextures(1, &id_); id_ = 0; }
}

} // namespace beigebox
