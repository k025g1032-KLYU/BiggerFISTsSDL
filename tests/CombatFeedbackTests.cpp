#include "Platform/CombatFeedback.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::puts("FAIL: expected sound asset directory and config path");
        return 1;
    }
    CombatFeedback feedback;
    CombatFeedbackConfig config;
    std::string error;
    const std::filesystem::path soundDirectory = argv[1];
    if (!LoadCombatFeedbackConfig(argv[2], config, error) ||
        !feedback.Initialize(soundDirectory, config, error, false) ||
        !feedback.HasAudioDevice()) {
        std::printf("FAIL: audio initialization: %s\n", error.c_str());
        return 1;
    }
    feedback.PlayPunchHit();
    feedback.PlayRocketHit();
    feedback.PlayRocketLaunch();
    feedback.PlayUltimateLaunch();
    feedback.PlayDodge();
    feedback.PlaySystemRestart();
    feedback.PlayPunchSwing();
    feedback.PlayRocketReconnect();
    feedback.PlayPlayerHit(PlayerHitType::Projectile, false);
    feedback.PlayPlayerHit(PlayerHitType::Projectile, true);
    feedback.PlayPlayerHit(PlayerHitType::Projectile, false, true);
    feedback.PlayPlayerHit(PlayerHitType::ChargeUnit, false);
    feedback.PlayPlayerHit(PlayerHitType::NeedleFinisher, true);
    for (const auto& [name, cue] : config.cues) {
        if (name != "engineLoop") feedback.PlayCue(name);
    }
    feedback.SetEngineLoop(true);
    feedback.UpdateMusic(1.0f / 60.0f);
    if (!feedback.IsEngineLoopQueued()) {
        std::puts("FAIL: engine loop did not queue audio");
        return 1;
    }
    feedback.SetRocketLaunchGain(0.08f);
    feedback.StopBattleAudio();
    if (feedback.IsEngineLoopQueued()) {
        std::puts("FAIL: scene exit left engine audio queued");
        return 1;
    }
    feedback.SetRocketLaunchGain(config.rocketLaunchGain);
    feedback.SetEngineLoop(true);
    feedback.UpdateMusic(1.0f / 60.0f);
    feedback.SetEngineLoop(false);
    if (feedback.IsEngineLoopQueued()) {
        std::puts("FAIL: engine loop did not stop");
        return 1;
    }
    if (!feedback.InitializeFullGameAudio(soundDirectory, error)) {
        std::printf("FAIL: full game audio: %s\n", error.c_str());
        return 1;
    }
    feedback.SetMusic("elevator");
    feedback.UpdateMusic(1.0f / 60.0f);
    feedback.PlayTutorialVoice("personnel.wav");
    if (feedback.GetCurrentMusic() != "elevator" ||
        !feedback.IsTutorialVoicePlaying()) {
        std::puts("FAIL: tutorial music or voice did not start");
        return 1;
    }
    feedback.SetMusic("boss");
    feedback.UpdateMusic(0.5f);
    if (feedback.GetCurrentMusic() != "boss") {
        std::puts("FAIL: battle music did not switch after fade");
        return 1;
    }
    SDL_Delay(50);
    feedback.Shutdown();

    if (feedback.Initialize(soundDirectory / "missing", config, error, false) ||
        error.find("enemy_is_hit.wav") == std::string::npos) {
        std::puts("FAIL: missing WAV must report its filename");
        return 1;
    }
    if (LoadCombatFeedbackConfig(soundDirectory / "missing.cfg", config, error) ||
        error.find("missing.cfg") == std::string::npos) {
        std::puts("FAIL: missing config must report its filename");
        return 1;
    }
    const std::filesystem::path invalidConfig = std::filesystem::temp_directory_path() /
        ("BiggerFISTsSDL-feedback-" + std::to_string(SDL_GetTicksNS()) + ".cfg");
    {
        std::ofstream output(invalidConfig);
        output << "impactWav ../outside.wav\n";
    }
    const bool acceptedInvalidPath = LoadCombatFeedbackConfig(invalidConfig, config, error);
    std::filesystem::remove(invalidConfig);
    if (acceptedInvalidPath || error.find("line 1") == std::string::npos) {
        std::puts("FAIL: invalid WAV path must report its line");
        return 1;
    }
    {
        std::ofstream output(invalidConfig);
        output << "cue landing ../outside.wav 0.5\n";
    }
    const bool acceptedInvalidCue = LoadCombatFeedbackConfig(
        invalidConfig, config, error);
    std::filesystem::remove(invalidConfig);
    if (acceptedInvalidCue || error.find("line 1") == std::string::npos) {
        std::puts("FAIL: invalid cue must report its line");
        return 1;
    }
    std::puts("PASS: WAV loading, SDL audio playback and missing-file diagnostics");
    return 0;
}
