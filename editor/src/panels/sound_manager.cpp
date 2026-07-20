// editor/src/panels/sound_manager.cpp
#include "sound_manager.h"
#include "ai_chat.h"
#include <imgui.h>
#include <cstdio>
#include <cstring>

namespace beigebox {

SoundManager::SoundManager() {
    SDL_AudioSpec want = {};
    want.freq = 22050; want.format = AUDIO_S16; want.channels = 1; want.samples = 2048;
    dev_ = SDL_OpenAudioDevice(nullptr, 0, &want, nullptr, 0);
    if (dev_) SDL_PauseAudioDevice(dev_, 0);
}

SoundManager::~SoundManager() {
    for (auto& s : sounds_) SDL_FreeWAV(s.data);
    if (dev_) SDL_CloseAudioDevice(dev_);
}

void SoundManager::Play(const std::string& name) {
    for (auto& s : sounds_) {
        if (s.name == name && s.data) {
            SDL_ClearQueuedAudio(dev_);
            SDL_QueueAudio(dev_, s.data, s.len);
            return;
        }
    }
}

void SoundManager::Draw() {
    ImGui::Begin("Sound Manager");
    static char importPath[256] = "assets/sounds/";
    ImGui::InputText("Import WAV", importPath, sizeof(importPath));
    ImGui::SameLine();
    if (ImGui::Button("Load")) {
        Sound s; s.path = importPath;
        auto slash = s.path.find_last_of("/\\");
        s.name = (slash != std::string::npos) ? s.path.substr(slash+1) : s.path;
        if (SDL_LoadWAV(s.path.c_str(), &s.spec, &s.data, &s.len)) {
            sounds_.push_back(s);
            if (aiChat_) aiChat_->AppendMessage("system", "Loaded: " + s.name);
        }
    }
    ImGui::Separator();
    for (auto& s : sounds_) {
        ImGui::PushID(s.name.c_str());
        ImGui::Text("%s", s.name.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("▶")) Play(s.name);
        ImGui::PopID();
    }
    ImGui::End();
}

} // namespace beigebox
