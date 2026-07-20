// editor/src/panels/asset_browser.h
// ─────────────────────────────────────────────────────────────
// Asset Browser Panel — file explorer for assets/ directory.
// ─────────────────────────────────────────────────────────────
#pragma once

#include "beigebox/render/sprite_registry.h"

#include <string>
#include <vector>
#include <functional>

namespace beigebox {

class AIChatPanel;

class AssetBrowser
{
public:
    explicit AssetBrowser(SpriteRegistry& registry)
        : registry_(&registry) {}

    void SetAIChat(AIChatPanel* chat) { aiChat_ = chat; }

    // Callbacks
    using SpriteCallback = std::function<void(const std::string& name)>;
    SpriteCallback onSpriteSelected;

    void Draw();

private:
    void RefreshFileList();
    void Log(const std::string& msg);

    SpriteRegistry* registry_;
    AIChatPanel*     aiChat_ = nullptr;

    std::vector<std::string> files_;
    std::string selectedFile_;
    char importPath_[256] = {};
    bool showImport_ = false;
};

} // namespace beigebox
