// editor/src/panels/sound_manager.h
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <SDL2/SDL.h>
#include <SDL2/SDL_audio.h>

namespace beigebox {
class AIChatPanel;

class SoundManager {
public:
    SoundManager();
    ~SoundManager();
    void SetAIChat(AIChatPanel* c) { aiChat_ = c; }
    void Draw();
private:
    void Play(const std::string& name);
    struct Sound { std::string name, path; SDL_AudioSpec spec; Uint8* data = nullptr; Uint32 len = 0; };
    std::vector<Sound> sounds_;
    AIChatPanel* aiChat_ = nullptr;
    SDL_AudioDeviceID dev_ = 0;
};
} // namespace beigebox
