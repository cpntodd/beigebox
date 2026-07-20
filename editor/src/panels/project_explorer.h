// editor/src/panels/project_explorer.h
// ─────────────────────────────────────────────────────────────
// Project Explorer — lazy-loaded tree view with navigation bar,
// path browser, filter, and right-click context menu.
//
// Auto-syncs to the active project path. Falls back to manual
// path entry with zenity browse button.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <string>
#include <functional>
#include <vector>

namespace beigebox {

class AIChatPanel;

class ProjectExplorer
{
public:
    using FileCallback = std::function<void(const std::string& path)>;

    void SetAIChat(AIChatPanel* c) { aiChat_ = c; }
    FileCallback onFileOpen;

    // ── Path management ──────────────────────────────────────
    void SetRootPath(const std::string& path);
    void SetProjectPath(const std::string& path) { projectPath_ = path; }
    const std::string& RootPath() const { return rootPath_; }

    void Draw();

private:
    // ── Tree data ────────────────────────────────────────────
    struct TreeNode
    {
        std::string name;
        std::string fullPath;
        bool        isDir = false;
        bool        expanded = false;
        bool        childrenLoaded = false;
        std::vector<TreeNode> children;
    };

    TreeNode rootNode_;

    // ── Navigation ───────────────────────────────────────────
    std::vector<std::string> navHistory_;
    int navIndex_ = -1;
    void NavigateTo(const std::string& path);
    void NavBack();
    void NavForward();

    // ── Tree operations ──────────────────────────────────────
    void ScanChildren(TreeNode& node);
    void DrawTreeNode(TreeNode& node, int depth);
    void DrawContextMenu(const TreeNode& node);
    void RefreshTree();

    // ── File system helpers ──────────────────────────────────
    static bool IsIgnored(const std::string& name);
    static std::string JoinPath(const std::string& a, const std::string& b);
    void BrowseRootPath();
    void Log(const std::string& msg);

    // ── Inline rename state ──────────────────────────────────
    bool        renaming_        = false;
    char        renameBuf_[256]  = {};
    std::string renameTarget_;

    // ── Filter ───────────────────────────────────────────────
    char        filterBuf_[128]  = {};

    // ── Path bar ─────────────────────────────────────────────
    char        pathBuf_[512]    = {};

    // ── Dependencies ─────────────────────────────────────────
    AIChatPanel* aiChat_     = nullptr;
    std::string  rootPath_   = ".";
    std::string  projectPath_;
};

} // namespace beigebox
