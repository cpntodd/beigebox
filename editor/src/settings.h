// editor/src/settings.h
// ─────────────────────────────────────────────────────────────
// Persistent Application Settings
//
// Stores user preferences in $XDG_CONFIG_HOME/beigebox/settings.json
// (Debian convention: ~/.config/beigebox/settings.json).
// Read at startup, written on Apply in the Preferences dialog.
// ─────────────────────────────────────────────────────────────
#pragma once

#include <string>
#include <cstdint>

namespace beigebox {

struct Settings {
    // ── General ──────────────────────────────────────────────
    std::string defaultProjectPath = "./projects";
    int   autoSaveMinutes   = 5;
    bool  autoBackupOnSave  = true;
    bool  showWelcomeOnStart = true;
    bool  rememberWindowLayout = true;

    // ── Editor ───────────────────────────────────────────────
    float editorFontScale  = 1.0f;
    int   editorThemeIdx   = 0;   // 0=Dark, 1=Light, 2=Classic
    bool  showLineNumbers  = true;
    int   tabSize          = 4;
    bool  autoIndent       = true;

    // ── Rendering ────────────────────────────────────────────
    bool  vsync            = true;
    int   fpsLimit         = 0;   // 0 = unlimited
    float viewportBgR      = 0.08f;
    float viewportBgG      = 0.08f;
    float viewportBgB      = 0.12f;
    bool  showGrid         = true;
    int   tileSize         = 64;  // pixels per tile (iso)

    // ── AI ───────────────────────────────────────────────────
    int   aiProviderIdx    = 3;   // 0=Ollama, 1=OpenAI, 2=Anthropic, 3=DeepSeek
    std::string aiModel    = "deepseek-chat";
    std::string aiApiKey;
    std::string aiEndpoint = "https://api.deepseek.com/v1";
    float  aiTemperature   = 0.7f;
    int    aiMaxTokens     = 2048;
    bool   aiThinkingMode  = false;

    // ── Git ──────────────────────────────────────────────────
    bool  gitAutoCommit    = false;
    bool  gitAutoPush      = false;
    std::string gitRemote  = "origin";
    std::string gitBranch  = "master";
    std::string gitAuthorName;
    std::string gitAuthorEmail;
    std::string gitCommitTemplate = "Auto-save: project snapshot";

    // ── Audio ────────────────────────────────────────────────
    float masterVolume    = 1.0f;
    float sfxVolume       = 0.8f;
    float musicVolume     = 0.6f;
    bool  muteWhenUnfocused = true;

    // ── World Defaults ───────────────────────────────────────
    int   mapWidth         = 32;
    int   mapHeight        = 32;
    int   mapSeed          = 42;
    float mapSalvageDensity = 0.15f;
    float mapGeothermalFreq = 0.05f;

    // ── Serialization ────────────────────────────────────────
    static std::string ConfigDir();
    static std::string ConfigPath();
    bool Load();
    bool Save() const;
};

} // namespace beigebox
