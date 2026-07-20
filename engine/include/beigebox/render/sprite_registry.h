// engine/include/beigebox/render/sprite_registry.h
// ─────────────────────────────────────────────────────────────
// Sprite Registry — maps names to OpenGL textures.
//
// Sprites are loaded from assets/sprites/ on demand and cached.
// Supports hot-reload via ReloadAll(). Thread-safe for read,
// not for concurrent load+reload.
// ─────────────────────────────────────────────────────────────
#pragma once

#include "beigebox/render/texture.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace beigebox {

class SpriteRegistry
{
public:
    // ── Lifecycle ────────────────────────────────────────────
    SpriteRegistry() = default;

    // Load a sprite by filename (relative to assets/sprites/).
    // Returns nullptr if not found or load failed.
    Texture* Load(const std::string& name);

    // Get a previously loaded sprite. Returns nullptr if not loaded.
    Texture* Get(const std::string& name);

    // Check if a sprite is loaded.
    bool IsLoaded(const std::string& name) const;

    // Reload all cached sprites from disk (hot-reload).
    void ReloadAll();

    // List all loaded sprite names.
    std::vector<std::string> ListNames() const;

    // Get or create a solid-color fallback texture (1×1 pixel).
    Texture* GetFallback();

private:
    std::unordered_map<std::string, Texture> textures_;
    Texture fallback_;
};

} // namespace beigebox
