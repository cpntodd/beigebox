// engine/src/render/sprite_registry.cpp
// ─────────────────────────────────────────────────────────────
// Sprite Registry implementation.
// ─────────────────────────────────────────────────────────────

#include "beigebox/render/sprite_registry.h"

#include <SDL2/SDL.h>

namespace beigebox {

Texture* SpriteRegistry::Load(const std::string& name)
{
    auto it = textures_.find(name);
    if (it != textures_.end())
        return &it->second;

    std::string path = "assets/sprites/" + name;
    Texture tex;
    if (tex.Load(path))
    {
        auto [insertedIt, ok] = textures_.emplace(name, std::move(tex));
        if (ok)
        {
            SDL_Log("SpriteRegistry: loaded '%s'", name.c_str());
            return &insertedIt->second;
        }
    }
    SDL_Log("SpriteRegistry: failed to load '%s'", name.c_str());
    return nullptr;
}

Texture* SpriteRegistry::Get(const std::string& name)
{
    auto it = textures_.find(name);
    return (it != textures_.end()) ? &it->second : nullptr;
}

bool SpriteRegistry::IsLoaded(const std::string& name) const
{
    return textures_.count(name) > 0;
}

void SpriteRegistry::ReloadAll()
{
    std::vector<std::string> names;
    for (auto& [name, tex] : textures_)
        names.push_back(name);

    textures_.clear();

    for (auto& name : names)
        Load(name);

    SDL_Log("SpriteRegistry: reloaded %zu sprites", names.size());
}

std::vector<std::string> SpriteRegistry::ListNames() const
{
    std::vector<std::string> names;
    for (auto& [name, tex] : textures_)
        names.push_back(name);
    return names;
}

Texture* SpriteRegistry::GetFallback()
{
    if (!fallback_.Valid())
        fallback_.CreateSolidColor(255, 0, 255); // magenta = missing texture
    return &fallback_;
}

} // namespace beigebox
