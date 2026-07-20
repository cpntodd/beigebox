// editor/src/panels/project_explorer.cpp
#include "project_explorer.h"
#include "ai_chat.h"
#include <imgui.h>
#include <SDL2/SDL.h>
#include <dirent.h>
#include <algorithm>

namespace beigebox {

void ProjectExplorer::Draw() {
    ImGui::Begin("Project Explorer");
    ImGui::Text("Project Files");
    ImGui::Separator();

    struct DirEntry { std::string name; bool isDir; };
    std::vector<DirEntry> entries;
    DIR* d = opendir(rootPath_.c_str());
    if (d) {
        struct dirent* e;
        while ((e = readdir(d))) {
            std::string n(e->d_name);
            if (n == "." || n == ".." || n[0] == '.') continue;
            bool isDir = (e->d_type == DT_DIR);
            entries.push_back({n, isDir});
        }
        closedir(d);
    }

    std::sort(entries.begin(), entries.end(), [](auto& a, auto& b) {
        if (a.isDir != b.isDir) return a.isDir > b.isDir;
        return a.name < b.name;
    });

    for (auto& entry : entries) {
        ImGui::PushID(entry.name.c_str());
        if (entry.isDir) ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.3f, 1.0f), "📁");
        else ImGui::TextColored(ImVec4(0.6f, 0.8f, 0.6f, 1.0f), "📄");
        ImGui::SameLine();
        if (ImGui::Selectable(entry.name.c_str())) {
            std::string path = rootPath_ + "/" + entry.name;
            if (!entry.isDir && onFileOpen) onFileOpen(path);
        }
        ImGui::PopID();
    }
    ImGui::End();
}

} // namespace beigebox
