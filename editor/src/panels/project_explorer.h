// editor/src/panels/project_explorer.h
#pragma once
#include <string>
#include <functional>

namespace beigebox {
class AIChatPanel;

class ProjectExplorer {
public:
    void SetAIChat(AIChatPanel* c) { aiChat_ = c; }
    using FileCallback = std::function<void(const std::string& path)>;
    FileCallback onFileOpen;
    void Draw();
private:
    void ScanDir(const std::string& dir);
    void DrawNode(const std::string& path, const std::string& name, bool isDir);
    AIChatPanel* aiChat_ = nullptr;
    std::string rootPath_ = ".";
};
} // namespace beigebox
