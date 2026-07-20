// editor/src/git_manager.cpp
// ─────────────────────────────────────────────────────────────
// Git Manager — wraps git CLI via popen.
// ─────────────────────────────────────────────────────────────

#include "git_manager.h"
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <array>

namespace beigebox {

std::string GitManager::RunGit(const std::string& projectPath,
                                const std::string& args) const
{
    std::string cmd = "cd \"" + projectPath + "\" && git " + args + " 2>&1";
    std::array<char, 256> buffer;
    std::string result;

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return "";

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
        result += buffer.data();

    int rc = pclose(pipe);
    if (rc != 0 && logger_)
        logger_("git " + args + " → exit " + std::to_string(rc));

    return result;
}

bool GitManager::IsAvailable() const
{
    FILE* pipe = popen("git --version 2>/dev/null", "r");
    if (!pipe) return false;
    char buf[128];
    bool ok = fgets(buf, sizeof(buf), pipe) != nullptr;
    pclose(pipe);
    return ok;
}

bool GitManager::IsRepo(const std::string& projectPath) const
{
    std::string out = RunGit(projectPath, "rev-parse --git-dir");
    return out.find(".git") != std::string::npos;
}

bool GitManager::Init(const std::string& projectPath)
{
    if (IsRepo(projectPath))
    {
        if (logger_) logger_("Git repo already exists.");
        return true;
    }
    std::string out = RunGit(projectPath, "init");
    if (logger_) logger_(out);
    return out.find("Initialized") != std::string::npos;
}

bool GitManager::Commit(const std::string& projectPath, const std::string& message)
{
    if (!IsRepo(projectPath))
    {
        if (logger_) logger_("Not a git repo. Use Git → Init first.");
        return false;
    }
    // Stage all
    RunGit(projectPath, "add -A");
    // Commit
    std::string escaped = message;
    // Escape double quotes in message
    for (size_t i = 0; i < escaped.size(); ++i)
        if (escaped[i] == '"') { escaped.insert(i, "\\"); ++i; }
    std::string out = RunGit(projectPath, "commit -m \"" + escaped + "\"");
    if (logger_) logger_(out);
    return out.find("changed") != std::string::npos ||
           out.find("insertion") != std::string::npos ||
           out.find("nothing to commit") != std::string::npos;
}

bool GitManager::Push(const std::string& projectPath,
                       const std::string& remote,
                       const std::string& branch)
{
    if (!IsRepo(projectPath)) return false;
    std::string out = RunGit(projectPath, "push " + remote + " " + branch);
    if (logger_) logger_(out);
    return out.find("error") == std::string::npos &&
           out.find("fatal") == std::string::npos;
}

std::string GitManager::Status(const std::string& projectPath) const
{
    return RunGit(projectPath, "status --short");
}

std::string GitManager::Log(const std::string& projectPath, int count) const
{
    return RunGit(projectPath,
        "log --oneline -" + std::to_string(count));
}

bool GitManager::SetAuthor(const std::string& projectPath,
                            const std::string& name,
                            const std::string& email)
{
    if (!IsRepo(projectPath)) return false;
    RunGit(projectPath, "config user.name \"" + name + "\"");
    RunGit(projectPath, "config user.email \"" + email + "\"");
    return true;
}

bool GitManager::AddRemote(const std::string& projectPath,
                            const std::string& name,
                            const std::string& url)
{
    if (!IsRepo(projectPath)) return false;
    std::string out = RunGit(projectPath, "remote add " + name + " " + url);
    if (logger_) logger_(out);
    return out.find("already exists") != std::string::npos ||
           out.empty();
}

} // namespace beigebox
