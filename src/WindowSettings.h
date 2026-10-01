#pragma once

#include <SDL3/SDL.h>

#include <string>

struct DisplaySettings {
    int windowWidth = 1280;
    int windowHeight = 720;
    int displayIndex = 0;
    bool fullscreen = false;
};

DisplaySettings LoadDisplaySettings(const char* filePath = nullptr);

class WindowSettings {
public:
    explicit WindowSettings(SDL_Window* window);

    bool Apply(const DisplaySettings& settings);
    void ProcessEvent(const SDL_Event& event);
    bool ApplyPending();
    void UpdateOutputSize(Uint32 width, Uint32 height);

    const DisplaySettings& GetSettings() const;

private:
    void Refresh();
    void Report();

    SDL_Window* window_;
    std::string baseTitle_;
    SDL_DisplayID displayId_ = 0;
    DisplaySettings settings_{};
    DisplaySettings pending_{};
    bool hasPending_ = false;
    int windowedX_ = 0;
    int windowedY_ = 0;
    Uint32 outputWidth_ = 0;
    Uint32 outputHeight_ = 0;
    std::string lastTitle_;
};
