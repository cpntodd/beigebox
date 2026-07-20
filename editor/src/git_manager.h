// editor/src/git_manager.h
// ─────────────────────────────────────────────────────────────
// Git Manager — wraps git CLI for backup and project management.
//
// Uses popen("git ...") for all operations. Requires git to be
// installed (standard on Debian development machines).
// ─────────────────────────────────────────────────────────────
#pragma once

#include <string>
#include <functional>

namespace beigebox {

class GitManager
{
public:
    using LogCallback = std::function<void(const std::string&)>;

    void SetLogger(LogCallback cb) { logger_ = std::move(cb); }

    // Check if git is available and the path is a git repo
    bool IsAvailable() const;
    bool IsRepo(const std::string& projectPath) const;

    // Initialize a new git repo in the project directory
    bool Init(const std::string& projectPath);

    // Stage all changes and commit
    bool Commit(const std::string& projectPath, const std::string& message);

    // Push to remote
    bool Push(const std::string& projectPath,
              const std::string& remote = "origin",
              const std::string& branch = "master");

    // Get current status summary
    std::string Status(const std::string& projectPath) const;

    // Get recent log
    std::string Log(const std::string& projectPath, int count = 5) const;

    // Configure author
    bool SetAuthor(const std::string& projectPath,
                   const std::string& name,
                   const std::string& email);

    // Add a remote
    bool AddRemote(const std::string& projectPath,
                   const std::string& name,
                   const std::string& url);

private:
    std::string RunGit(const std::string& projectPath,
                       const std::string& args) const;

    LogCallback logger_;
};

} // namespace beigebox
