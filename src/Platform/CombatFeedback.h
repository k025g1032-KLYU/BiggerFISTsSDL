#pragma once

#include <SDL3/SDL.h>

#include "Data/CombatFeedbackConfig.h"

#include <filesystem>
#include <cstddef>
#include <string>
#include <vector>

class CombatFeedback {
public:
    CombatFeedback() = default;
    ~CombatFeedback();
    CombatFeedback(const CombatFeedback&) = delete;
    CombatFeedback& operator=(const CombatFeedback&) = delete;

    bool Initialize(const std::filesystem::path& soundDirectory,
        const CombatFeedbackConfig& config, std::string& error,
        bool enableGamepad = true);
    void ProcessEvent(const SDL_Event& event);
    void PlayPunchHit();
    void PlayRocketHit();
    void PlayRocketLaunch();
    void Shutdown();
    bool HasAudioDevice() const { return audioDevice_ != 0; }
    bool HasGamepad() const { return gamepad_ != nullptr; }

private:
    struct Clip {
        SDL_AudioSpec spec{};
        std::vector<Uint8> samples;
        std::vector<SDL_AudioStream*> voices;
        size_t nextVoice = 0;
    };

    bool LoadClip(const std::filesystem::path& path, Clip& clip, std::string& error);
    bool CreateVoices(Clip& clip, size_t count);
    void PlayClip(Clip& clip, float gain);
    void OpenFirstGamepad();
    void Rumble(Uint16 lowFrequency, Uint16 highFrequency,
        Uint32 durationMilliseconds, int priority);

    Clip impact_;
    Clip rocketLaunch_;
    CombatFeedbackConfig config_{};
    SDL_AudioDeviceID audioDevice_ = 0;
    SDL_Gamepad* gamepad_ = nullptr;
    bool audioInitialized_ = false;
    bool gamepadInitialized_ = false;
    bool rumbleSupported_ = true;
    Uint64 rumbleUntil_ = 0;
    int rumblePriority_ = 0;
};
