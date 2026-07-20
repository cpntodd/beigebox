// editor/src/panels/asset_browser.cpp
// ─────────────────────────────────────────────────────────────
// Asset Browser Panel — file listing, preview, import.
// ─────────────────────────────────────────────────────────────

#include "asset_browser.h"
#include "ai_chat.h"

#include <imgui.h>

#include <SDL2/SDL.h>
#include <sys/stat.h>
#include <dirent.h>
#include <cstdio>
#include <cstring>
#include <algorithm>

namespace beigebox {

void AssetBrowser::RefreshFileList()
{
    files_.clear();

    // Ensure assets/sprites/ exists
    mkdir("assets", 0755);
    mkdir("assets/sprites", 0755);

    DIR* dir = opendir("assets/sprites");
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (entry->d_type == DT_REG)
        {
            std::string name(entry->d_name);
            // Filter for image files
            if (name.size() > 4 &&
                (name.rfind(".png") != std::string::npos ||
                 name.rfind(".bmp") != std::string::npos ||
                 name.rfind(".jpg") != std::string::npos ||
                 name.rfind(".tga") != std::string::npos))
            {
                files_.push_back(name);
            }
        }
    }
    closedir(dir);
}

void AssetBrowser::Log(const std::string& msg)
{
    if (aiChat_) aiChat_->AppendMessage("system", msg);
    SDL_Log("%s", msg.c_str());
}

void AssetBrowser::Draw()
{
    ImGui::Begin("Asset Browser");

    // ── Toolbar ──────────────────────────────────────────────
    if (ImGui::Button("Refresh"))
        RefreshFileList();
    ImGui::SameLine();
    if (ImGui::Button("Import..."))
    {
        strcpy(importPath_, "assets/sprites/");
        showImport_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload All"))
    {
        registry_->ReloadAll();
        Log("All sprites reloaded.");
    }

    ImGui::Separator();

    // ── File List ────────────────────────────────────────────
    if (files_.empty())
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
            "No sprites found. Place .png files in assets/sprites/");
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
            "or use Import to copy a file.");
    }

    ImGui::BeginChild("##fileList", ImVec2(0, -40), true);

    for (auto& name : files_)
    {
        bool isSelected = (name == selectedFile_);
        bool isLoaded = registry_->IsLoaded(name);

        ImGui::PushID(name.c_str());

        if (isLoaded)
            ImGui::TextColored(ImVec4(0.3f, 0.9f, 0.3f, 1.0f), "●");
        else
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "○");
        ImGui::SameLine();

        if (ImGui::Selectable(name.c_str(), &isSelected))
        {
            selectedFile_ = name;
            if (onSpriteSelected) onSpriteSelected(name);
        }

        // Right-click context menu
        if (ImGui::BeginPopupContextItem())
        {
            if (ImGui::MenuItem("Load Sprite"))
            {
                registry_->Load(name);
                Log("Loaded: " + name);
            }
            if (ImGui::MenuItem("Assign to Selected Entity"))
            {
                if (onSpriteSelected) onSpriteSelected(name);
            }
            ImGui::EndPopup();
        }

        // Preview tooltip on hover
        if (ImGui::IsItemHovered() && isLoaded)
        {
            ImGui::BeginTooltip();
            Texture* tex = registry_->Get(name);
            if (tex && tex->Valid())
            {
                ImGui::Text("%s (%dx%d)", name.c_str(), tex->Width(), tex->Height());
                float scale = std::min(128.0f / tex->Width(), 128.0f / tex->Height());
                ImGui::Image((ImTextureID)(intptr_t)tex->ID(),
                    ImVec2(tex->Width() * scale, tex->Height() * scale));
            }
            ImGui::EndTooltip();
        }

        ImGui::PopID();
    }

    ImGui::EndChild();

    // ── Status bar ───────────────────────────────────────────
    ImGui::Text("%zu files | %s selected",
        files_.size(),
        selectedFile_.empty() ? "none" : selectedFile_.c_str());

    ImGui::End();

    // ── Import Dialog ────────────────────────────────────────
    if (showImport_)
    {
        ImGui::OpenPopup("Import Sprite");
        if (ImGui::BeginPopupModal("Import Sprite", &showImport_,
            ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::Text("Enter the path to a .png file to copy into assets/sprites/");
            ImGui::InputText("Source Path", importPath_, sizeof(importPath_));
            ImGui::Spacing();
            if (ImGui::Button("Import", ImVec2(100, 0)))
            {
                // Extract filename from path
                std::string path(importPath_);
                auto slash = path.find_last_of("/\\");
                std::string filename = (slash != std::string::npos)
                    ? path.substr(slash + 1) : path;

                std::string dest = "assets/sprites/" + filename;

                // Simple copy
                FILE* src = fopen(path.c_str(), "rb");
                if (src)
                {
                    FILE* dst = fopen(dest.c_str(), "wb");
                    if (dst)
                    {
                        char buf[8192]; size_t n;
                        while ((n = fread(buf, 1, sizeof(buf), src)) > 0)
                            fwrite(buf, 1, n, dst);
                        fclose(dst);
                        Log("Imported: " + filename);
                        registry_->Load(filename);
                        RefreshFileList();
                    }
                    else Log("Error: cannot write to " + dest);
                    fclose(src);
                }
                else Log("Error: cannot read " + path);

                showImport_ = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0)))
                showImport_ = false;
            ImGui::EndPopup();
        }
    }
}

} // namespace beigebox
