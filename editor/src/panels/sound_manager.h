// editor/src/panels/sound_manager.h
// ─────────────────────────────────────────────────────────────
// Sound Manager / Editor — SDL2_mixer-based audio tool.
//
// Features:
//   - Sound banks from directory structure
//   - SFX tab: event binding grid, import, preview, per-sound settings
//   - Music tab: playlist, play/stop, per-map music assignment
//   - Global event defaults + per-unit-type overrides
//   - OGG, MP3, FLAC, WAV, Opus support via SDL2_mixer
// ─────────────────────────────────────────────────────────────
#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

namespace beigebox {

class AIChatPanel;

// ── Sound metadata ──────────────────────────────────────────
struct SoundMeta
{
    std::string name;       // display name (filename without ext)
    std::string path;       // relative path within sounds/, e.g. "combat/explosion.ogg"
    std::string bank;       // parent bank name, e.g. "combat"
    float volume   = 1.0f;
    bool  loop     = false;
    float pan      = 0.0f;  // -1.0 (left) to 1.0 (right)
    bool  loaded   = false;
    Mix_Chunk* chunk = nullptr;
};

// ── Event binding ───────────────────────────────────────────
struct EventBinding
{
    std::string eventName;
    std::string soundPath;
    std::string unitTypeOverride;  // empty = global
};

// ── Music track ─────────────────────────────────────────────
struct MusicTrack
{
    std::string name;
    std::string path;
    std::string mapName;    // empty = default playlist
    bool        loaded = false;
    Mix_Music*  music  = nullptr;
};

class SoundManager
{
public:
    using LogCallback = std::function<void(const std::string&)>;

    SoundManager();
    ~SoundManager();

    SoundManager(const SoundManager&) = delete;
    SoundManager& operator=(const SoundManager&) = delete;

    void SetAIChat(AIChatPanel* c) { aiChat_ = c; }
    void SetRootPath(const std::string& p) { rootPath_ = p; }
    void Draw();

    // ── Runtime API ──────────────────────────────────────────
    void PlaySfx(const std::string& soundPath, float volume = 1.0f);
    void PlayEvent(const std::string& eventName,
                   const std::string& unitType = "");
    void PlayMusic(const std::string& trackName);
    void StopMusic();
    void SetMasterVolume(float v);
    void SetSfxVolume(float v);
    void SetMusicVolume(float v);

    // ── Unit types (populated externally from UnitTemplateManager) ──
    void SetUnitTypes(const std::vector<std::string>& types) { unitTypes_ = types; }

private:
    void DrawSfxTab();
    void DrawMusicTab();
    void DrawEventBindingGrid();
    void ScanSoundBanks();
    void LoadSoundMeta();
    void SaveSoundMeta();
    void LoadMusicList();
    void ImportSound();
    void PreviewSound(const SoundMeta& sound);
    int  FindFreeChannel();
    void Log(const std::string& msg);
    std::string SfxDir() const;
    std::string MusicDir() const;
    std::string MetaPath() const;

    std::vector<SoundMeta>     sounds_;
    std::vector<EventBinding>  bindings_;
    std::vector<MusicTrack>    musicTracks_;
    std::vector<std::string>   soundBanks_;

    int    masterVolume_  = MIX_MAX_VOLUME;
    int    sfxVolume_     = MIX_MAX_VOLUME;
    int    musicVolume_   = MIX_MAX_VOLUME;
    int    previewChannel_ = -1;

    static const char* kEvents[];
    static const int   kEventCount;

    char   importPath_[512]  = {};
    char   bankFilter_[64]   = {};
    bool   showImport_       = false;
    int    selectedBank_     = 0;
    int    selectedTab_      = 0;
    char   newBankName_[64]  = {};

    std::vector<std::string> unitTypes_;
    AIChatPanel* aiChat_     = nullptr;
    std::string  rootPath_   = ".";
};

} // namespace beigebox
