// editor/src/settings.cpp
// ─────────────────────────────────────────────────────────────
// Persistent settings — JSON read/write to XDG_CONFIG_HOME.
// ─────────────────────────────────────────────────────────────

#include "settings.h"
#include <nlohmann/json.hpp>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>

namespace beigebox {

using json = nlohmann::json;

static bool MkdirP(const std::string& path)
{
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i)
    {
        cur += path[i];
        if (path[i] == '/' || i == path.size() - 1)
        {
            if (!cur.empty() && cur != "/")
                mkdir(cur.c_str(), 0755);
        }
    }
    return true;
}

std::string Settings::ConfigDir()
{
    const char* xdg = getenv("XDG_CONFIG_HOME");
    if (xdg && xdg[0])
        return std::string(xdg) + "/beigebox";
    const char* home = getenv("HOME");
    if (home && home[0])
        return std::string(home) + "/.config/beigebox";
    return "./.beigebox_config"; // fallback
}

std::string Settings::ConfigPath()
{
    return ConfigDir() + "/settings.json";
}

bool Settings::Load()
{
    std::string path = ConfigPath();
    FILE* f = fopen(path.c_str(), "r");
    if (!f) return false; // first run — no settings yet

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0) { fclose(f); return false; }

    std::string s(sz, '\0');
    fread(&s[0], 1, sz, f);
    fclose(f);

    try
    {
        json j = json::parse(s);

        // General
        if (j.contains("defaultProjectPath")) defaultProjectPath = j["defaultProjectPath"].get<std::string>();
        if (j.contains("autoSaveMinutes")) autoSaveMinutes = j["autoSaveMinutes"].get<int>();
        if (j.contains("autoBackupOnSave")) autoBackupOnSave = j["autoBackupOnSave"].get<bool>();
        if (j.contains("showWelcomeOnStart")) showWelcomeOnStart = j["showWelcomeOnStart"].get<bool>();
        if (j.contains("rememberWindowLayout")) rememberWindowLayout = j["rememberWindowLayout"].get<bool>();

        // Editor
        if (j.contains("editorFontScale")) editorFontScale = j["editorFontScale"].get<float>();
        if (j.contains("editorThemeIdx")) editorThemeIdx = j["editorThemeIdx"].get<int>();
        if (j.contains("showLineNumbers")) showLineNumbers = j["showLineNumbers"].get<bool>();
        if (j.contains("tabSize")) tabSize = j["tabSize"].get<int>();
        if (j.contains("autoIndent")) autoIndent = j["autoIndent"].get<bool>();

        // Rendering
        if (j.contains("vsync")) vsync = j["vsync"].get<bool>();
        if (j.contains("fpsLimit")) fpsLimit = j["fpsLimit"].get<int>();
        if (j.contains("viewportBgR")) viewportBgR = j["viewportBgR"].get<float>();
        if (j.contains("viewportBgG")) viewportBgG = j["viewportBgG"].get<float>();
        if (j.contains("viewportBgB")) viewportBgB = j["viewportBgB"].get<float>();
        if (j.contains("showGrid")) showGrid = j["showGrid"].get<bool>();
        if (j.contains("tileSize")) tileSize = j["tileSize"].get<int>();

        // AI
        if (j.contains("aiProviderIdx")) aiProviderIdx = j["aiProviderIdx"].get<int>();
        if (j.contains("aiModel")) aiModel = j["aiModel"].get<std::string>();
        if (j.contains("aiApiKey")) aiApiKey = j["aiApiKey"].get<std::string>();
        if (j.contains("aiEndpoint")) aiEndpoint = j["aiEndpoint"].get<std::string>();
        if (j.contains("aiTemperature")) aiTemperature = j["aiTemperature"].get<float>();
        if (j.contains("aiMaxTokens")) aiMaxTokens = j["aiMaxTokens"].get<int>();
        if (j.contains("aiThinkingMode")) aiThinkingMode = j["aiThinkingMode"].get<bool>();

        // Git
        if (j.contains("gitAutoCommit")) gitAutoCommit = j["gitAutoCommit"].get<bool>();
        if (j.contains("gitAutoPush")) gitAutoPush = j["gitAutoPush"].get<bool>();
        if (j.contains("gitRemote")) gitRemote = j["gitRemote"].get<std::string>();
        if (j.contains("gitBranch")) gitBranch = j["gitBranch"].get<std::string>();
        if (j.contains("gitAuthorName")) gitAuthorName = j["gitAuthorName"].get<std::string>();
        if (j.contains("gitAuthorEmail")) gitAuthorEmail = j["gitAuthorEmail"].get<std::string>();
        if (j.contains("gitCommitTemplate")) gitCommitTemplate = j["gitCommitTemplate"].get<std::string>();

        // Audio
        if (j.contains("masterVolume")) masterVolume = j["masterVolume"].get<float>();
        if (j.contains("sfxVolume")) sfxVolume = j["sfxVolume"].get<float>();
        if (j.contains("musicVolume")) musicVolume = j["musicVolume"].get<float>();
        if (j.contains("muteWhenUnfocused")) muteWhenUnfocused = j["muteWhenUnfocused"].get<bool>();

        // World
        if (j.contains("mapWidth")) mapWidth = j["mapWidth"].get<int>();
        if (j.contains("mapHeight")) mapHeight = j["mapHeight"].get<int>();
        if (j.contains("mapSeed")) mapSeed = j["mapSeed"].get<int>();
        if (j.contains("mapSalvageDensity")) mapSalvageDensity = j["mapSalvageDensity"].get<float>();
        if (j.contains("mapGeothermalFreq")) mapGeothermalFreq = j["mapGeothermalFreq"].get<float>();

        return true;
    }
    catch (const std::exception&) { return false; }
}

bool Settings::Save() const
{
    std::string dir = ConfigDir();
    MkdirP(dir);

    json j;
    // General
    j["defaultProjectPath"] = defaultProjectPath;
    j["autoSaveMinutes"] = autoSaveMinutes;
    j["autoBackupOnSave"] = autoBackupOnSave;
    j["showWelcomeOnStart"] = showWelcomeOnStart;
    j["rememberWindowLayout"] = rememberWindowLayout;
    // Editor
    j["editorFontScale"] = editorFontScale;
    j["editorThemeIdx"] = editorThemeIdx;
    j["showLineNumbers"] = showLineNumbers;
    j["tabSize"] = tabSize;
    j["autoIndent"] = autoIndent;
    // Rendering
    j["vsync"] = vsync;
    j["fpsLimit"] = fpsLimit;
    j["viewportBgR"] = viewportBgR;
    j["viewportBgG"] = viewportBgG;
    j["viewportBgB"] = viewportBgB;
    j["showGrid"] = showGrid;
    j["tileSize"] = tileSize;
    // AI
    j["aiProviderIdx"] = aiProviderIdx;
    j["aiModel"] = aiModel;
    j["aiApiKey"] = aiApiKey;
    j["aiEndpoint"] = aiEndpoint;
    j["aiTemperature"] = aiTemperature;
    j["aiMaxTokens"] = aiMaxTokens;
    j["aiThinkingMode"] = aiThinkingMode;
    // Git
    j["gitAutoCommit"] = gitAutoCommit;
    j["gitAutoPush"] = gitAutoPush;
    j["gitRemote"] = gitRemote;
    j["gitBranch"] = gitBranch;
    j["gitAuthorName"] = gitAuthorName;
    j["gitAuthorEmail"] = gitAuthorEmail;
    j["gitCommitTemplate"] = gitCommitTemplate;
    // Audio
    j["masterVolume"] = masterVolume;
    j["sfxVolume"] = sfxVolume;
    j["musicVolume"] = musicVolume;
    j["muteWhenUnfocused"] = muteWhenUnfocused;
    // World
    j["mapWidth"] = mapWidth;
    j["mapHeight"] = mapHeight;
    j["mapSeed"] = mapSeed;
    j["mapSalvageDensity"] = mapSalvageDensity;
    j["mapGeothermalFreq"] = mapGeothermalFreq;

    std::string path = ConfigPath();
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return false;
    std::string s = j.dump(2);
    fwrite(s.c_str(), 1, s.size(), f);
    fclose(f);
    return true;
}

} // namespace beigebox
