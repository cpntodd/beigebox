// editor/src/panels/build_manager.h
// ─────────────────────────────────────────────────────────────
// Build & Export Manager — standalone panel.
//
// One-click build pipeline with validation, progress logging,
// .tar.gz packaging, and configurable output targets.
// Accessible via File → Export Game and as dockable panel.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <string>
#include <vector>
#include <functional>

namespace beigebox {

class AIChatPanel;

class BuildManager
{
public:
    BuildManager();
    ~BuildManager() = default;

    void SetAIChat(AIChatPanel* c) { aiChat_ = c; }
    void SetRootPath(const std::string& p) { rootPath_ = p; }
    void Draw();

    // Open the panel programmatically (e.g. from File → Export Game)
    void Open() { show_ = true; }

private:
    void DoBuild();
    void DoPackage();
    bool ValidateBuild();
    std::string GenerateLaunchScript() const;
    void Log(const std::string& msg);
    void ClearLog();

    // ── File copy helpers ────────────────────────────────────
    bool CopyFile(const std::string& src, const std::string& dst);
    bool CopyDir(const std::string& srcDir, const std::string& dstDir);

    AIChatPanel* aiChat_ = nullptr;
    std::string  rootPath_ = ".";

    // UI state
    bool  show_ = true;
    char  outputPath_[256] = "./dist/MAD_Export";
    char  versionBuf_[32]   = "0.1.0";
    bool  buildLinux_   = true;
    bool  buildWindows_ = false;
    bool  packageTarGz_ = true;
    bool  buildRunning_ = false;
    float buildProgress_ = 0.0f;
    std::string buildStatus_;

    // Build log
    std::vector<std::string> buildLog_;
    bool  autoScroll_ = true;

    // Steps
    int   totalSteps_ = 8;
    int   currentStep_ = 0;
};

} // namespace beigebox
