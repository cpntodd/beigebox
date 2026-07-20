// editor/src/panels/sound_manager.cpp
// ─────────────────────────────────────────────────────────────
// Sound Manager / Editor — full SDL2_mixer implementation.
// ─────────────────────────────────────────────────────────────

#include "sound_manager.h"
#include "ai_chat.h"

#include <imgui.h>
#include <nlohmann/json.hpp>

#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>

#include <dirent.h>
#include <sys/stat.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <fstream>

namespace beigebox {

using json = nlohmann::json;

// ── Event names (matching Lua event system) ─────────────────
const char* SoundManager::kEvents[] = {
    "OnInit", "OnTick", "OnRightClick", "OnTakeDamage", "OnDeath",
    "OnAttack", "OnBuildComplete", "OnUnitSelected", "OnUnitMove",
    "OnSuperweaponReady", "OnSuperweaponFire", "OnMissionStart",
    "OnVictory", "OnDefeat", "OnLowHealth", "OnLevelUp"
};
const int SoundManager::kEventCount = sizeof(kEvents) / sizeof(kEvents[0]);

// ── Constructor / Destructor ────────────────────────────────

SoundManager::SoundManager()
{
    int flags = MIX_INIT_OGG | MIX_INIT_MP3 | MIX_INIT_FLAC;
    int initted = Mix_Init(flags);
    if ((initted & flags) != flags)
        SDL_Log("SoundManager: Mix_Init missing some codecs: %s", Mix_GetError());

    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    {
        SDL_Log("SoundManager: Mix_OpenAudio failed: %s", Mix_GetError());
        return;
    }

    Mix_AllocateChannels(32);
    Mix_VolumeMusic(musicVolume_);
    Mix_Volume(-1, sfxVolume_);

    SDL_Log("SoundManager: SDL2_mixer initialized (OGG, MP3, FLAC)");

    mkdir(SfxDir().c_str(), 0755);
    mkdir(MusicDir().c_str(), 0755);
}

SoundManager::~SoundManager()
{
    for (auto& s : sounds_)
        if (s.chunk) Mix_FreeChunk(s.chunk);
    for (auto& m : musicTracks_)
        if (m.music) Mix_FreeMusic(m.music);

    Mix_CloseAudio();
    Mix_Quit();
}

// ── Path helpers ────────────────────────────────────────────

std::string SoundManager::SfxDir() const
{
    return rootPath_ + "/assets/sounds";
}

std::string SoundManager::MusicDir() const
{
    return rootPath_ + "/assets/music";
}

std::string SoundManager::MetaPath() const
{
    return rootPath_ + "/sound_events.json";
}

void SoundManager::Log(const std::string& msg)
{
    if (aiChat_) aiChat_->AppendMessage("system", "[Sound] " + msg);
    SDL_Log("[Sound] %s", msg.c_str());
}

// ── Sound Bank Scanning ─────────────────────────────────────

void SoundManager::ScanSoundBanks()
{
    soundBanks_.clear();
    sounds_.clear();

    std::string sfxDir = SfxDir();
    mkdir(sfxDir.c_str(), 0755);

    DIR* d = opendir(sfxDir.c_str());
    if (!d) return;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr)
    {
        std::string name(entry->d_name);
        if (name == "." || name == ".." || name[0] == '.') continue;

        if (entry->d_type == DT_DIR)
        {
            soundBanks_.push_back(name);

            std::string bankDir = sfxDir + "/" + name;
            DIR* bd = opendir(bankDir.c_str());
            if (bd)
            {
                struct dirent* sf;
                while ((sf = readdir(bd)) != nullptr)
                {
                    std::string sfname(sf->d_name);
                    if (sfname == "." || sfname == ".." || sfname[0] == '.') continue;
                    if (sf->d_type != DT_REG) continue;

                    std::string ext;
                    auto dot = sfname.rfind('.');
                    if (dot != std::string::npos) ext = sfname.substr(dot);
                    if (ext != ".wav" && ext != ".ogg" && ext != ".mp3" && ext != ".flac" && ext != ".opus")
                        continue;

                    SoundMeta meta;
                    meta.name = sfname.substr(0, dot);
                    meta.path = name + "/" + sfname;
                    meta.bank = name;
                    sounds_.push_back(meta);
                }
                closedir(bd);
            }
        }
    }
    closedir(d);

    std::sort(soundBanks_.begin(), soundBanks_.end());

    Log("Scanned " + std::to_string(soundBanks_.size()) + " banks, "
        + std::to_string(sounds_.size()) + " sounds");
}

// ── JSON Metadata ───────────────────────────────────────────

void SoundManager::LoadSoundMeta()
{
    std::string path = MetaPath();
    std::ifstream f(path);
    if (!f.good()) return;

    try
    {
        json j = json::parse(f);

        if (j.contains("sounds"))
        {
            for (auto& [spath, sjson] : j["sounds"].items())
            {
                for (auto& s : sounds_)
                {
                    if (s.path == spath)
                    {
                        if (sjson.contains("volume")) s.volume = sjson["volume"].get<float>();
                        if (sjson.contains("loop"))   s.loop   = sjson["loop"].get<bool>();
                        if (sjson.contains("pan"))    s.pan    = sjson["pan"].get<float>();
                        break;
                    }
                }
            }
        }

        if (j.contains("bindings"))
        {
            bindings_.clear();
            for (auto& bj : j["bindings"])
            {
                EventBinding eb;
                eb.eventName        = bj.value("event", "");
                eb.soundPath        = bj.value("sound", "");
                eb.unitTypeOverride = bj.value("unitType", "");
                bindings_.push_back(eb);
            }
        }
    }
    catch (const std::exception& e)
    {
        Log(std::string("Error loading sound_events.json: ") + e.what());
    }
}

void SoundManager::SaveSoundMeta()
{
    json j;

    json soundSettings = json::object();
    for (auto& s : sounds_)
    {
        json sj;
        sj["volume"] = s.volume;
        sj["loop"]   = s.loop;
        sj["pan"]    = s.pan;
        soundSettings[s.path] = sj;
    }
    j["sounds"] = soundSettings;

    json bindingsArr = json::array();
    for (auto& b : bindings_)
    {
        json bj;
        bj["event"]    = b.eventName;
        bj["sound"]    = b.soundPath;
        bj["unitType"] = b.unitTypeOverride;
        bindingsArr.push_back(bj);
    }
    j["bindings"] = bindingsArr;

    std::string path = MetaPath();
    std::ofstream f(path);
    if (f.good())
    {
        f << j.dump(2);
        Log("Saved " + path);
    }
    else
    {
        Log("Error writing " + path);
    }
}

// ── Music List ──────────────────────────────────────────────

void SoundManager::LoadMusicList()
{
    musicTracks_.clear();

    std::string musicDir = MusicDir();
    mkdir(musicDir.c_str(), 0755);

    DIR* d = opendir(musicDir.c_str());
    if (!d) return;

    struct dirent* entry;
    while ((entry = readdir(d)) != nullptr)
    {
        std::string name(entry->d_name);
        if (name == "." || name == ".." || name[0] == '.') continue;
        if (entry->d_type != DT_REG) continue;

        std::string ext;
        auto dot = name.rfind('.');
        if (dot != std::string::npos) ext = name.substr(dot);
        if (ext != ".ogg" && ext != ".mp3" && ext != ".flac" && ext != ".wav") continue;

        MusicTrack mt;
        mt.name   = name.substr(0, dot);
        mt.path   = musicDir + "/" + name;
        musicTracks_.push_back(mt);
    }
    closedir(d);
}

// ── Import ──────────────────────────────────────────────────

void SoundManager::ImportSound()
{
    if (!importPath_[0]) return;
    std::string src(importPath_);

    std::string targetDir;
    if (selectedBank_ >= 0 && selectedBank_ < static_cast<int>(soundBanks_.size()))
        targetDir = SfxDir() + "/" + soundBanks_[selectedBank_];
    else
        targetDir = SfxDir();

    auto slash = src.rfind('/');
    std::string fname = (slash != std::string::npos) ? src.substr(slash + 1) : src;
    std::string dst = targetDir + "/" + fname;

    std::ifstream in(src, std::ios::binary);
    std::ofstream out(dst, std::ios::binary);
    if (in.good() && out.good())
    {
        out << in.rdbuf();
        Log("Imported: " + dst);
        ScanSoundBanks();
        LoadSoundMeta();
    }
    else
    {
        Log("Import failed: " + src);
    }
}

// ── Playback ────────────────────────────────────────────────

void SoundManager::PreviewSound(const SoundMeta& sound)
{
    std::string fullPath = SfxDir() + "/" + sound.path;

    if (!sound.loaded || !sound.chunk)
    {
        Mix_Chunk* chunk = Mix_LoadWAV(fullPath.c_str());
        if (!chunk)
        {
            Log(std::string("Failed to load: ") + fullPath + " — " + Mix_GetError());
            return;
        }
        const_cast<SoundMeta&>(sound).chunk  = chunk;
        const_cast<SoundMeta&>(sound).loaded = true;
    }

    int ch = FindFreeChannel();
    if (ch < 0) ch = 0;
    Mix_PlayChannel(ch, sound.chunk, 0);
    Mix_Volume(ch, static_cast<int>(sound.volume * sfxVolume_));
}

void SoundManager::PlaySfx(const std::string& soundPath, float volume)
{
    std::string fullPath = SfxDir() + "/" + soundPath;
    for (auto& s : sounds_)
    {
        if (s.path == soundPath)
        {
            PreviewSound(s);
            return;
        }
    }
    // Fallback: load directly
    Mix_Chunk* chunk = Mix_LoadWAV(fullPath.c_str());
    if (!chunk) return;
    int ch = FindFreeChannel();
    if (ch < 0) ch = 0;
    Mix_PlayChannel(ch, chunk, 0);
    Mix_Volume(ch, static_cast<int>(volume * sfxVolume_));
}

void SoundManager::PlayEvent(const std::string& eventName,
                              const std::string& unitType)
{
    // Check unit-type override first, then global
    for (auto& b : bindings_)
    {
        if (b.eventName == eventName && !unitType.empty()
            && b.unitTypeOverride == unitType)
        {
            PlaySfx(b.soundPath);
            return;
        }
    }
    for (auto& b : bindings_)
    {
        if (b.eventName == eventName && b.unitTypeOverride.empty())
        {
            PlaySfx(b.soundPath);
            return;
        }
    }
}

void SoundManager::PlayMusic(const std::string& trackName)
{
    StopMusic();
    for (auto& mt : musicTracks_)
    {
        if (mt.name == trackName)
        {
            if (!mt.music)
            {
                mt.music = Mix_LoadMUS(mt.path.c_str());
                if (!mt.music)
                {
                    Log(std::string("Failed to load music: ") + mt.path);
                    return;
                }
                mt.loaded = true;
            }
            Mix_PlayMusic(mt.music, -1);
            Mix_VolumeMusic(musicVolume_);
            Log("Playing: " + mt.name);
            return;
        }
    }
    Log("Music track not found: " + trackName);
}

void SoundManager::StopMusic()
{
    Mix_HaltMusic();
}

void SoundManager::SetMasterVolume(float v)
{
    masterVolume_ = static_cast<int>(v * MIX_MAX_VOLUME);
    SetSfxVolume(v);
    SetMusicVolume(v);
}

void SoundManager::SetSfxVolume(float v)
{
    sfxVolume_ = static_cast<int>(v * MIX_MAX_VOLUME);
    Mix_Volume(-1, sfxVolume_);
}

void SoundManager::SetMusicVolume(float v)
{
    musicVolume_ = static_cast<int>(v * MIX_MAX_VOLUME);
    Mix_VolumeMusic(musicVolume_);
}

int SoundManager::FindFreeChannel()
{
    for (int i = 0; i < 32; ++i)
        if (!Mix_Playing(i)) return i;
    return -1;
}

// ═════════════════════════════════════════════════════════════
// UI Drawing
// ═════════════════════════════════════════════════════════════

void SoundManager::Draw()
{
    ImGui::Begin("Sound Manager");

    if (ImGui::BeginTabBar("##soundTabs"))
    {
        if (ImGui::BeginTabItem("SFX##sfxTab"))
        {
            selectedTab_ = 0;
            DrawSfxTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Music##musicTab"))
        {
            selectedTab_ = 1;
            DrawMusicTab();
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Event Bindings##evtTab"))
        {
            selectedTab_ = 2;
            DrawEventBindingGrid();
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    ImGui::End();
}

// ── SFX Tab ─────────────────────────────────────────────────

void SoundManager::DrawSfxTab()
{
    if (ImGui::Button("Rescan Banks"))
    {
        ScanSoundBanks();
        LoadSoundMeta();
    }
    ImGui::SameLine();
    if (ImGui::Button("Import..."))
    {
        showImport_ = true;
        strcpy(importPath_, SfxDir().c_str());
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    ImGui::InputTextWithHint("##bankFilter", "Filter sounds...", bankFilter_, sizeof(bankFilter_));

    ImGui::Separator();

    // Bank selector
    ImGui::Text("Bank:");
    ImGui::SameLine();
    if (ImGui::BeginCombo("##bankCombo",
            selectedBank_ < static_cast<int>(soundBanks_.size())
                ? soundBanks_[selectedBank_].c_str() : "All"))
    {
        if (ImGui::Selectable("All", selectedBank_ == -1))
            selectedBank_ = -1;
        for (int i = 0; i < static_cast<int>(soundBanks_.size()); ++i)
        {
            if (ImGui::Selectable(soundBanks_[i].c_str(), selectedBank_ == i))
                selectedBank_ = i;
        }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120);
    ImGui::InputTextWithHint("##newBank", "New bank...", newBankName_, sizeof(newBankName_));
    ImGui::SameLine();
    if (ImGui::Button("Create Bank") && newBankName_[0])
    {
        std::string bankDir = SfxDir() + "/" + newBankName_;
        mkdir(bankDir.c_str(), 0755);
        Log("Created bank: " + std::string(newBankName_));
        newBankName_[0] = '\0';
        ScanSoundBanks();
    }

    ImGui::Separator();

    // Sound list
    ImGui::BeginChild("##sfxList", ImVec2(0, 0), false);

    static int editIdx = -1;
    int removeIdx = -1;

    for (int i = 0; i < static_cast<int>(sounds_.size()); ++i)
    {
        auto& s = sounds_[i];

        if (selectedBank_ >= 0 && selectedBank_ < static_cast<int>(soundBanks_.size())
            && s.bank != soundBanks_[selectedBank_])
            continue;

        if (bankFilter_[0])
        {
            std::string nl = s.name, fl = bankFilter_;
            std::transform(nl.begin(), nl.end(), nl.begin(), ::tolower);
            std::transform(fl.begin(), fl.end(), fl.begin(), ::tolower);
            if (nl.find(fl) == std::string::npos) continue;
        }

        ImGui::PushID(i);

        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s]", s.bank.c_str());
        ImGui::SameLine();

        if (i == editIdx)
        {
            ImGui::SliderFloat("Vol", &s.volume, 0.0f, 1.0f, "%.2f");
            ImGui::SameLine();
            ImGui::Checkbox("Loop", &s.loop);
            ImGui::SameLine();
            ImGui::SliderFloat("Pan", &s.pan, -1.0f, 1.0f, "%.1f");
            ImGui::SameLine();
            if (ImGui::Button("Done"))
            {
                editIdx = -1;
                SaveSoundMeta();
            }
        }
        else
        {
            ImGui::Text("%s", s.name.c_str());
        }

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 180);
        if (ImGui::SmallButton("▶"))
            PreviewSound(s);
        ImGui::SameLine();
        if (ImGui::SmallButton("Edit"))
            editIdx = (editIdx == i) ? -1 : i;
        ImGui::SameLine();
        if (ImGui::SmallButton("✕"))
            removeIdx = i;

        ImGui::PopID();
    }

    if (removeIdx >= 0 && removeIdx < static_cast<int>(sounds_.size()))
    {
        std::string fullPath = SfxDir() + "/" + sounds_[removeIdx].path;
        if (remove(fullPath.c_str()) == 0)
        {
            Log("Deleted: " + sounds_[removeIdx].name);
            sounds_.erase(sounds_.begin() + removeIdx);
        }
    }

    ImGui::EndChild();

    // Import popup
    if (showImport_)
    {
        ImGui::OpenPopup("Import Sound");
        if (ImGui::BeginPopupModal("Import Sound", &showImport_))
        {
            ImGui::InputText("Source Path", importPath_, sizeof(importPath_));
            ImGui::SameLine();
            if (ImGui::Button("...##browseImport", ImVec2(30, 0)))
            {
#ifdef __linux__
                FILE* pipe = popen("zenity --file-selection 2>/dev/null", "r");
                if (pipe)
                {
                    char line[512];
                    if (fgets(line, sizeof(line), pipe))
                    {
                        size_t len = strlen(line);
                        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
                        strcpy(importPath_, line);
                    }
                    pclose(pipe);
                }
#endif
            }
            ImGui::Spacing();
            if (ImGui::Button("Import", ImVec2(100, 0)))
            {
                ImportSound();
                showImport_ = false;
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel", ImVec2(80, 0)))
                showImport_ = false;
            ImGui::EndPopup();
        }
    }
}

// ── Music Tab ───────────────────────────────────────────────

void SoundManager::DrawMusicTab()
{
    static int playIdx = -1;

    if (ImGui::Button("Refresh"))
        LoadMusicList();
    ImGui::SameLine();
    ImGui::Text("Music directory: assets/music/");

    ImGui::Separator();

    int vol = musicVolume_;
    ImGui::SliderInt("Music Volume", &vol, 0, MIX_MAX_VOLUME);
    if (vol != musicVolume_) SetMusicVolume(static_cast<float>(vol) / MIX_MAX_VOLUME);

    if (ImGui::Button("⏹ Stop"))
    {
        StopMusic();
        playIdx = -1;
    }

    ImGui::Separator();

    ImGui::BeginChild("##musicList", ImVec2(0, 0), false);

    for (int i = 0; i < static_cast<int>(musicTracks_.size()); ++i)
    {
        auto& mt = musicTracks_[i];
        ImGui::PushID(i);

        bool isPlaying = (playIdx == i && Mix_PlayingMusic());

        ImGui::TextColored(isPlaying ? ImVec4(0.3f, 0.9f, 0.3f, 1.0f)
                                     : ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
            "%s", mt.name.c_str());

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 100);
        if (isPlaying)
        {
            if (ImGui::SmallButton("⏹"))
            {
                StopMusic();
                playIdx = -1;
            }
        }
        else
        {
            if (ImGui::SmallButton("▶"))
            {
                PlayMusic(mt.name);
                playIdx = i;
            }
        }

        ImGui::PopID();
    }

    if (musicTracks_.empty())
    {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
            "No music tracks found.\nPlace .ogg or .mp3 files in assets/music/");
    }

    ImGui::EndChild();
}

// ── Event Binding Grid ──────────────────────────────────────

void SoundManager::DrawEventBindingGrid()
{
    if (ImGui::Button("Add Binding"))
    {
        EventBinding eb;
        eb.eventName = kEvents[0];
        bindings_.push_back(eb);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save"))
        SaveSoundMeta();
    ImGui::SameLine();
    if (ImGui::Button("Load"))
        LoadSoundMeta();

    ImGui::Separator();

    static int typeFilter = 0;
    ImGui::SetNextItemWidth(180);
    if (ImGui::BeginCombo("Filter", typeFilter == 0 ? "All" :
            typeFilter == 1 ? "Global only" :
            (typeFilter - 2 < static_cast<int>(unitTypes_.size())
                ? unitTypes_[typeFilter - 2].c_str() : "???")))
    {
        if (ImGui::Selectable("All", typeFilter == 0)) typeFilter = 0;
        if (ImGui::Selectable("Global only", typeFilter == 1)) typeFilter = 1;
        for (int i = 0; i < static_cast<int>(unitTypes_.size()); ++i)
            if (ImGui::Selectable(unitTypes_[i].c_str(), typeFilter == i + 2))
                typeFilter = i + 2;
        ImGui::EndCombo();
    }

    ImGui::Separator();

    ImGui::BeginChild("##bindingGrid", ImVec2(0, 0), false);

    if (ImGui::BeginTable("##bindTable", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY))
    {
        ImGui::TableSetupColumn("Event", ImGuiTableColumnFlags_WidthFixed, 150);
        ImGui::TableSetupColumn("Sound", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Unit Override", ImGuiTableColumnFlags_WidthFixed, 140);
        ImGui::TableSetupColumn("Actions", ImGuiTableColumnFlags_WidthFixed, 60);
        ImGui::TableHeadersRow();

        int removeIdx = -1;
        for (int i = 0; i < static_cast<int>(bindings_.size()); ++i)
        {
            auto& b = bindings_[i];

            if (typeFilter == 1 && !b.unitTypeOverride.empty()) continue;
            if (typeFilter >= 2)
            {
                std::string targetType = unitTypes_[typeFilter - 2];
                if (b.unitTypeOverride != targetType) continue;
            }

            ImGui::TableNextRow();
            ImGui::PushID(i);

            ImGui::TableSetColumnIndex(0);
            if (ImGui::BeginCombo("##event", b.eventName.c_str()))
            {
                for (int e = 0; e < kEventCount; ++e)
                    if (ImGui::Selectable(kEvents[e], b.eventName == kEvents[e]))
                        b.eventName = kEvents[e];
                ImGui::EndCombo();
            }

            ImGui::TableSetColumnIndex(1);
            if (ImGui::BeginCombo("##sound", b.soundPath.empty() ? "(none)" : b.soundPath.c_str()))
            {
                if (ImGui::Selectable("(none)", b.soundPath.empty()))
                    b.soundPath.clear();
                for (auto& s : sounds_)
                    if (ImGui::Selectable(s.path.c_str(), b.soundPath == s.path))
                        b.soundPath = s.path;
                ImGui::EndCombo();
            }

            ImGui::TableSetColumnIndex(2);
            if (ImGui::BeginCombo("##unit", b.unitTypeOverride.empty() ? "(global)" : b.unitTypeOverride.c_str()))
            {
                if (ImGui::Selectable("(global)", b.unitTypeOverride.empty()))
                    b.unitTypeOverride.clear();
                for (auto& ut : unitTypes_)
                    if (ImGui::Selectable(ut.c_str(), b.unitTypeOverride == ut))
                        b.unitTypeOverride = ut;
                ImGui::EndCombo();
            }

            ImGui::TableSetColumnIndex(3);
            if (ImGui::SmallButton("✕"))
                removeIdx = i;

            ImGui::PopID();
        }

        if (removeIdx >= 0)
            bindings_.erase(bindings_.begin() + removeIdx);

        ImGui::EndTable();
    }

    ImGui::EndChild();
}

} // namespace beigebox
