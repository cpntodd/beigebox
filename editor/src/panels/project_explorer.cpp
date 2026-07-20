// editor/src/panels/project_explorer.cpp
// ─────────────────────────────────────────────────────────────
// Project Explorer — full implementation.
//
// Lazy-loaded tree, navigation breadcrumb bar, path browser,
// filter, right-click context menu, inline rename.
// ─────────────────────────────────────────────────────────────

#include "project_explorer.h"
#include "ai_chat.h"

#include <imgui.h>
#include <SDL2/SDL.h>

#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <set>

namespace beigebox {

// ── Ignored directory names ─────────────────────────────────
// These are skipped during scans to keep the tree clean.

static const std::set<std::string> kIgnored = {
    ".git", "build", ".vscode", "__pycache__", "node_modules", ".beigebox_config"
};

bool ProjectExplorer::IsIgnored(const std::string& name)
{
    if (!name.empty() && name[0] == '.') return true;
    return kIgnored.count(name) > 0;
}

std::string ProjectExplorer::JoinPath(const std::string& a, const std::string& b)
{
    if (a.empty()) return b;
    if (b.empty()) return a;
    if (a.back() == '/') return a + b;
    return a + "/" + b;
}

void ProjectExplorer::Log(const std::string& msg)
{
    if (aiChat_) aiChat_->AppendMessage("system", "[Project] " + msg);
    SDL_Log("[Project] %s", msg.c_str());
}

// ── Path Management ─────────────────────────────────────────

void ProjectExplorer::SetRootPath(const std::string& path)
{
    std::string p = path;
    while (!p.empty() && p.back() == '/') p.pop_back();
    if (p.empty()) p = ".";

    if (p == rootPath_) return;
    rootPath_ = p;
    NavigateTo(rootPath_);
    RefreshTree();
}

void ProjectExplorer::BrowseRootPath()
{
#ifdef __linux__
    FILE* pipe = popen("zenity --file-selection --directory 2>/dev/null", "r");
    if (pipe)
    {
        char line[512];
        if (fgets(line, sizeof(line), pipe))
        {
            size_t len = strlen(line);
            if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
            if (line[0])
            {
                SetRootPath(line);
                snprintf(pathBuf_, sizeof(pathBuf_), "%s", line);
            }
        }
        pclose(pipe);
    }
    else
    {
        Log("zenity not available - type path manually.");
    }
#else
    Log("File browser not available on this platform.");
#endif
}

// ── Navigation ──────────────────────────────────────────────

void ProjectExplorer::NavigateTo(const std::string& path)
{
    if (navIndex_ >= 0 && navIndex_ + 1 < static_cast<int>(navHistory_.size()))
        navHistory_.resize(navIndex_ + 1);

    navHistory_.push_back(path);
    navIndex_ = static_cast<int>(navHistory_.size()) - 1;
}

void ProjectExplorer::NavBack()
{
    if (navIndex_ > 0)
    {
        navIndex_--;
        rootPath_ = navHistory_[navIndex_];
        RefreshTree();
    }
}

void ProjectExplorer::NavForward()
{
    if (navIndex_ + 1 < static_cast<int>(navHistory_.size()))
    {
        navIndex_++;
        rootPath_ = navHistory_[navIndex_];
        RefreshTree();
    }
}

// ── Tree Operations ─────────────────────────────────────────

void ProjectExplorer::ScanChildren(TreeNode& node)
{
    node.children.clear();
    node.childrenLoaded = true;

    DIR* d = opendir(node.fullPath.c_str());
    if (!d) return;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr)
    {
        std::string name(entry->d_name);
        if (name == "." || name == "..") continue;
        if (IsIgnored(name)) continue;

        TreeNode child;
        child.name     = name;
        child.fullPath = JoinPath(node.fullPath, name);

        if (entry->d_type == DT_DIR)
        {
            child.isDir = true;
        }
        else if (entry->d_type == DT_UNKNOWN)
        {
            struct stat st;
            if (stat(child.fullPath.c_str(), &st) == 0)
                child.isDir = S_ISDIR(st.st_mode);
        }

        node.children.push_back(std::move(child));
    }
    closedir(d);

    std::sort(node.children.begin(), node.children.end(),
        [](const TreeNode& a, const TreeNode& b) {
            if (a.isDir != b.isDir) return a.isDir > b.isDir;
            return a.name < b.name;
        });
}

void ProjectExplorer::RefreshTree()
{
    // Save currently expanded paths before rebuild
    std::vector<std::string> expandedPaths;
    CollectExpandedPaths(rootNode_, expandedPaths);

    rootNode_.name     = rootPath_;
    rootNode_.fullPath = rootPath_;
    rootNode_.isDir    = true;
    rootNode_.expanded = true;
    rootNode_.children.clear();
    rootNode_.childrenLoaded = false;
    ScanChildren(rootNode_);

    // Restore expanded state
    RestoreExpandedPaths(rootNode_, expandedPaths);
}

void ProjectExplorer::CollectExpandedPaths(const TreeNode& node,
    std::vector<std::string>& paths)
{
    if (node.expanded && node.isDir && !node.fullPath.empty())
        paths.push_back(node.fullPath);
    for (const auto& child : node.children)
        CollectExpandedPaths(child, paths);
}

void ProjectExplorer::RestoreExpandedPaths(TreeNode& node,
    const std::vector<std::string>& paths)
{
    for (const auto& p : paths)
    {
        if (node.fullPath == p)
        {
            node.expanded = true;
            if (!node.childrenLoaded)
                ScanChildren(node);
            break;
        }
    }
    for (auto& child : node.children)
        RestoreExpandedPaths(child, paths);
}

// ── Context Menu ────────────────────────────────────────────

void ProjectExplorer::DrawContextMenu(const TreeNode& node)
{
    if (ImGui::BeginPopupContextItem(node.name.c_str()))
    {
        if (!node.isDir)
        {
            if (ImGui::MenuItem("Open"))
            {
                if (onFileOpen) onFileOpen(node.fullPath);
            }
            ImGui::Separator();
        }

        if (ImGui::MenuItem("Rename", "F2"))
        {
            renaming_     = true;
            renameTarget_ = node.fullPath;
            strncpy(renameBuf_, node.name.c_str(), sizeof(renameBuf_) - 1);
            renameBuf_[sizeof(renameBuf_) - 1] = '\0';
        }

        if (ImGui::MenuItem("Delete", "Del"))
        {
            std::string cmd = "rm -rf \"" + node.fullPath + "\"";
            if (system(cmd.c_str()) == 0)
            {
                Log("Deleted: " + node.fullPath);
                RefreshTree();
            }
            else
            {
                Log("Failed to delete: " + node.fullPath);
            }
        }

        if (node.isDir)
        {
            ImGui::Separator();
            if (ImGui::MenuItem("New Folder"))
            {
                std::string newPath = JoinPath(node.fullPath, "new_folder");
                mkdir(newPath.c_str(), 0755);
                Log("Created: " + newPath);
                RefreshTree();
            }
            if (ImGui::MenuItem("New Lua Script"))
            {
                std::string newPath = JoinPath(node.fullPath, "untitled.lua");
                FILE* f = fopen(newPath.c_str(), "w");
                if (f)
                {
                    fprintf(f, "-- New M.A.D. Lua Script\n");
                    fprintf(f, "function OnTick(entity_id)\n");
                    fprintf(f, "    -- Your code here\n");
                    fprintf(f, "end\n");
                    fclose(f);
                    Log("Created: " + newPath);
                    RefreshTree();
                }
                else
                {
                    Log("Failed to create: " + newPath);
                }
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Reveal in File Manager"))
        {
            std::string target = node.isDir ? node.fullPath
                : node.fullPath.substr(0, node.fullPath.rfind('/'));
            std::string cmd = "xdg-open \"" + target + "\" &";
            system(cmd.c_str());
        }

        if (ImGui::MenuItem("Copy Path", "Ctrl+Shift+C"))
        {
            SDL_SetClipboardText(node.fullPath.c_str());
        }

        ImGui::Separator();
        if (ImGui::MenuItem("Refresh", "F5"))
        {
            RefreshTree();
        }

        ImGui::EndPopup();
    }
}

// ── Tree Node Drawing ───────────────────────────────────────

void ProjectExplorer::DrawTreeNode(TreeNode& node, int depth)
{
    ImGui::PushID(node.fullPath.c_str());

    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_SpanAvailWidth;
    if (!node.isDir)
        flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    if (node.children.empty() && node.isDir && node.childrenLoaded)
        flags |= ImGuiTreeNodeFlags_Leaf;

    // ── Indent ───────────────────────────────────────────────
    float indentVal = depth * 16.0f;
    if (depth > 0)
        ImGui::Indent(indentVal);

    // ── Icon ─────────────────────────────────────────────────
    std::string label;
    if (node.isDir)
        label = node.expanded ? "\xf0\x9f\x93\x82 " : "\xf0\x9f\x93\x81 ";
    else
    {
        std::string ext;
        auto dot = node.name.rfind('.');
        if (dot != std::string::npos) ext = node.name.substr(dot);

        if (ext == ".lua")            label = "\xf0\x9f\x8c\x99 ";
        else if (ext == ".madproj")   label = "\xf0\x9f\x8e\xae ";
        else if (ext == ".ogm")       label = "\xf0\x9f\x97\xba\xef\xb8\x8f ";
        else if (ext == ".png" || ext == ".jpg" || ext == ".bmp") label = "\xf0\x9f\x96\xbc\xef\xb8\x8f ";
        else if (ext == ".wav" || ext == ".ogg" || ext == ".mp3") label = "\xf0\x9f\x94\x8a ";
        else if (ext == ".json")      label = "\xf0\x9f\x93\x8b ";
        else if (ext == ".glsl")      label = "\xe2\x9c\xa8 ";
        else if (ext == ".md" || ext == ".txt") label = "\xf0\x9f\x93\x9d ";
        else                          label = "\xf0\x9f\x93\x84 ";
    }
    label += node.name;

    // ── Filter check ─────────────────────────────────────────
    bool passesFilter = true;
    if (filterBuf_[0])
    {
        std::string nameLower = node.name;
        std::string filterLower = filterBuf_;
        std::transform(nameLower.begin(), nameLower.end(), nameLower.begin(), ::tolower);
        std::transform(filterLower.begin(), filterLower.end(), filterLower.begin(), ::tolower);
        passesFilter = nameLower.find(filterLower) != std::string::npos;
    }

    if (!passesFilter && !node.isDir)
    {
        ImGui::PopID();
        if (depth > 0) ImGui::Unindent(indentVal);
        return;
    }

    // ── Inline rename ────────────────────────────────────────
    if (renaming_ && renameTarget_ == node.fullPath)
    {
        ImGui::SetNextItemWidth(200);
        if (ImGui::InputText("##rename", renameBuf_, sizeof(renameBuf_),
                ImGuiInputTextFlags_EnterReturnsTrue))
        {
            std::string parent = node.fullPath.substr(0, node.fullPath.rfind('/'));
            std::string newPath = JoinPath(parent, renameBuf_);
            if (rename(node.fullPath.c_str(), newPath.c_str()) == 0)
            {
                Log(std::string("Renamed: ") + node.name + " -> " + renameBuf_);
                renaming_ = false;
                RefreshTree();
                ImGui::PopID();
                if (depth > 0) ImGui::Unindent(indentVal);
                return;
            }
            else
            {
                Log("Rename failed.");
            }
        }
        if (ImGui::IsItemDeactivated() && !ImGui::IsItemActive())
            renaming_ = false;
    }
    else
    {
        if (node.isDir)
        {
            ImGui::SetNextItemOpen(node.expanded);
            bool open = ImGui::TreeNodeEx(label.c_str(), flags);
            if (open != node.expanded)
            {
                node.expanded = open;
                // Lazy-load children on first expand
                if (node.expanded && !node.childrenLoaded)
                    ScanChildren(node);
            }
        }
        else
        {
            ImGui::TreeNodeEx(label.c_str(), flags);
            if (ImGui::IsItemClicked() && onFileOpen)
                onFileOpen(node.fullPath);
        }

        // ── Context menu ────────────────────────────────────
        DrawContextMenu(node);

        // ── Drag source for .madproj files ─────────────────
        if (!node.isDir && node.name.size() > 8 &&
            node.name.substr(node.name.size() - 8) == ".madproj")
        {
            if (ImGui::BeginDragDropSource())
            {
                ImGui::SetDragDropPayload("PROJECT_FILE", node.fullPath.c_str(),
                    node.fullPath.size() + 1);
                ImGui::Text("Project: %s", node.name.c_str());
                ImGui::EndDragDropSource();
            }
        }
    }

    // ── Recurse children ─────────────────────────────────────
    if (node.isDir && node.expanded)
    {
        for (auto& child : node.children)
        {
            if (filterBuf_[0] && !child.isDir)
            {
                std::string nl = child.name;
                std::string fl = filterBuf_;
                std::transform(nl.begin(), nl.end(), nl.begin(), ::tolower);
                std::transform(fl.begin(), fl.end(), fl.begin(), ::tolower);
                if (nl.find(fl) == std::string::npos) continue;
            }
            DrawTreeNode(child, depth + 1);
        }
        ImGui::TreePop();
    }

    if (depth > 0) ImGui::Unindent(indentVal);
    ImGui::PopID();
}

// ── Main Draw ───────────────────────────────────────────────

void ProjectExplorer::Draw()
{
    ImGui::Begin("Project Explorer");

    // ── Auto-sync to project path on first draw ──────────────
    if (rootPath_ == "." && !projectPath_.empty()
        && projectPath_ != "untitled.madproj")
    {
        std::string dir = projectPath_;
        auto slash = dir.rfind('/');
        if (slash != std::string::npos)
            dir = dir.substr(0, slash);
        if (!dir.empty() && dir != ".")
            SetRootPath(dir);
    }

    // ── Navigation bar ───────────────────────────────────────
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 2));
    float navBtnW = 28;

    if (ImGui::Button("\xe2\x97\x82", ImVec2(navBtnW, 0))) NavBack();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Back");
    ImGui::SameLine();

    if (ImGui::Button("\xe2\x96\xb8", ImVec2(navBtnW, 0))) NavForward();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Forward");
    ImGui::SameLine();

    if (ImGui::Button("\xf0\x9f\x8f\xa0", ImVec2(navBtnW, 0)))
    {
        if (!projectPath_.empty())
        {
            std::string dir = projectPath_;
            auto slash = dir.rfind('/');
            if (slash != std::string::npos)
                dir = dir.substr(0, slash);
            if (!dir.empty()) SetRootPath(dir);
        }
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Go to project root");
    ImGui::SameLine();

    // ── Editable path ────────────────────────────────────────
    snprintf(pathBuf_, sizeof(pathBuf_), "%s", rootPath_.c_str());
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 70);
    if (ImGui::InputText("##projPath", pathBuf_, sizeof(pathBuf_),
            ImGuiInputTextFlags_EnterReturnsTrue))
    {
        SetRootPath(pathBuf_);
    }
    ImGui::SameLine();

    if (ImGui::Button("...##browseProj", ImVec2(30, 0)))
        BrowseRootPath();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Browse for folder...");

    ImGui::PopStyleVar();
    ImGui::Separator();

    // ── Filter ───────────────────────────────────────────────
    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 40);
    ImGui::InputTextWithHint("##fileFilter", "Filter...", filterBuf_, sizeof(filterBuf_));
    ImGui::SameLine();
    if (ImGui::Button("\xf0\x9f\x94\x84", ImVec2(30, 0))) RefreshTree();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Refresh tree");

    ImGui::Separator();

    // ── Tree ─────────────────────────────────────────────────
    ImGui::BeginChild("##fileTree", ImVec2(0, 0), false,
        ImGuiWindowFlags_HorizontalScrollbar);

    if (!rootNode_.childrenLoaded)
        RefreshTree();

    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 12.0f);
    for (auto& child : rootNode_.children)
        DrawTreeNode(child, 0);
    ImGui::PopStyleVar();

    // ── Right-click on empty space ───────────────────────────
    if (ImGui::BeginPopupContextWindow("##explorerContext",
            ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverExistingPopup))
    {
        if (ImGui::MenuItem("New Folder"))
        {
            std::string newPath = JoinPath(rootPath_, "new_folder");
            mkdir(newPath.c_str(), 0755);
            Log("Created: " + newPath);
            RefreshTree();
        }
        if (ImGui::MenuItem("New Lua Script"))
        {
            std::string newPath = JoinPath(rootPath_, "untitled.lua");
            FILE* f = fopen(newPath.c_str(), "w");
            if (f) {
                fprintf(f, "-- New M.A.D. Lua Script\nfunction OnTick(entity_id)\n    -- Your code here\nend\n");
                fclose(f);
                Log("Created: " + newPath);
                RefreshTree();
            }
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Refresh", "F5")) RefreshTree();
        ImGui::EndPopup();
    }

    ImGui::EndChild();
    ImGui::End();
}

} // namespace beigebox

