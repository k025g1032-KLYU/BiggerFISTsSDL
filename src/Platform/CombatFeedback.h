#pragma once

#include <SDL3/SDL.h>

#include "Data/CombatFeedbackConfig.h"
#include "Gameplay/Player/PlayerVitality.h"
#include "Gameplay/Combat/BattleFeedbackEvent.h"

#include <filesystem>
#include <cstddef>
#include <string>
#include <unordered_map>
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
    void PlayUltimateLaunch();
    void PlayDodge();
    void PlaySystemRestart();
    void PlayPunchSwing();
    void PlayRocketReconnect();
    void PlayPlayerHit(PlayerHitType type, bool damagedHitPoints);
    void PlayBattleEvent(BattleFeedbackEvent event);
    void PlayCue(const std::string& name);
    void StopCue(const std::string& name);
    void SetEngineLoop(bool enabled);
    bool IsEngineLoopQueued() const;
    void Shutdown();
    bool HasAudioDevice() const { return audioDevice_ != 0; }
    bool HasGamepad() const { return gamepad_ != nullptr; }
    SDL_Gamepad* GetGamepad() const { return gamepad_; }
    bool InitializeFullGameAudio(const std::filesystem::path& soundDirectory,
        std::string& error);
    void SetMusic(const std::string& name);
    const std::string& GetCurrentMusic() const { return currentMusic_; }
    void PlayTutorialVoice(const std::string& name);
    bool IsTutorialVoicePlaying() const;
    void UpdateMusic(float seconds);

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
    Clip punchSwing_;
    Clip rocketReconnect_;
    Clip chargeHit_;
    Clip dodge_;
    Clip bossChangeMode_;
    Clip bossSharpHit_;
    Clip bossRiser_;
    Clip battleExplosion_;
    std::unordered_map<std::string, Clip> music_;
    std::unordered_map<std::string, Clip> tutorialVoices_;
    std::unordered_map<std::string, Clip> cues_;
    std::string currentMusic_;
    std::string pendingMusic_;
    std::string currentVoice_;
    float musicFadeRemaining_ = 0.0f;
    bool tutorialCheckPending_ = false;
    bool tutorialCheckPlayed_ = false;
    bool engineRequested_ = false;
    float engineFadeRemaining_ = 0.0f;
    CombatFeedbackConfig config_{};
    SDL_AudioDeviceID audioDevice_ = 0;
    SDL_Gamepad* gamepad_ = nullptr;
    bool audioInitialized_ = false;
    bool gamepadInitialized_ = false;
    bool rumbleSupported_ = true;
    Uint64 rumbleUntil_ = 0;
    int rumblePriority_ = 0;
};
