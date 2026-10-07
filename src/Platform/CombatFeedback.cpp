#include "Platform/CombatFeedback.h"

#include <algorithm>
#include <limits>

namespace {
std::string ToUtf8(const std::filesystem::path& path) {
    const std::u8string value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}
}

CombatFeedback::~CombatFeedback() {
    Shutdown();
}

bool CombatFeedback::LoadClip(const std::filesystem::path& path,
    Clip& clip, std::string& error) {
    const std::string filename = ToUtf8(path);
    Uint8* wavData = nullptr;
    Uint32 wavLength = 0;
    if (!SDL_LoadWAV(filename.c_str(), &clip.spec, &wavData, &wavLength)) {
        error = "Failed to load WAV '" + filename + "': " + SDL_GetError();
        return false;
    }
    if (wavLength == 0 || wavLength > static_cast<Uint32>(std::numeric_limits<int>::max())) {
        SDL_free(wavData);
        error = "Invalid WAV length in '" + filename + "'";
        return false;
    }
    clip.samples.assign(wavData, wavData + wavLength);
    SDL_free(wavData);
    return true;
}

bool CombatFeedback::CreateVoices(Clip& clip, size_t count) {
    for (size_t index = 0; index < count; ++index) {
        SDL_AudioStream* stream = SDL_CreateAudioStream(&clip.spec, nullptr);
        if (stream == nullptr) {
            return false;
        }
        clip.voices.push_back(stream);
        if (!SDL_BindAudioStream(audioDevice_, stream)) {
            return false;
        }
    }
    return true;
}

bool CombatFeedback::Initialize(const std::filesystem::path& soundDirectory,
    const CombatFeedbackConfig& config, std::string& error, bool enableGamepad) {
    Shutdown();
    error.clear();
    config_ = config;
    if (!LoadClip(soundDirectory / config_.impactWav, impact_, error) ||
        !LoadClip(soundDirectory / config_.rocketLaunchWav, rocketLaunch_, error) ||
        !LoadClip(soundDirectory / config_.punchSwingWav, punchSwing_, error) ||
        !LoadClip(soundDirectory / config_.rocketReconnectWav, rocketReconnect_, error) ||
        !LoadClip(soundDirectory / config_.chargeHitWav, chargeHit_, error) ||
        !LoadClip(soundDirectory / config_.dodgeWav, dodge_, error) ||
        !LoadClip(soundDirectory / config_.bossChangeModeWav,
            bossChangeMode_, error) ||
        !LoadClip(soundDirectory / config_.bossSharpHitWav,
            bossSharpHit_, error) ||
        !LoadClip(soundDirectory / config_.bossRiserWav,
            bossRiser_, error) ||
        !LoadClip(soundDirectory / config_.battleExplosionWav,
            battleExplosion_, error)) {
        Shutdown();
        return false;
    }

    if (SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        audioInitialized_ = true;
        audioDevice_ = SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, nullptr);
        if (audioDevice_ == 0 || !CreateVoices(impact_, 4) ||
            !CreateVoices(rocketLaunch_, 2) || !CreateVoices(punchSwing_, 2) ||
            !CreateVoices(rocketReconnect_, 2) || !CreateVoices(chargeHit_, 2) ||
            !CreateVoices(dodge_, 2) ||
            !CreateVoices(bossChangeMode_, 2) ||
            !CreateVoices(bossSharpHit_, 2) ||
            !CreateVoices(bossRiser_, 2) ||
            !CreateVoices(battleExplosion_, 2)) {
            SDL_Log("Audio playback unavailable: %s", SDL_GetError());
            for (SDL_AudioStream* stream : impact_.voices) {
                SDL_DestroyAudioStream(stream);
            }
            for (SDL_AudioStream* stream : rocketLaunch_.voices) {
                SDL_DestroyAudioStream(stream);
            }
            for (SDL_AudioStream* stream : punchSwing_.voices) {
                SDL_DestroyAudioStream(stream);
            }
            for (SDL_AudioStream* stream : rocketReconnect_.voices) {
                SDL_DestroyAudioStream(stream);
            }
            for (SDL_AudioStream* stream : chargeHit_.voices) {
                SDL_DestroyAudioStream(stream);
            }
            for (SDL_AudioStream* stream : dodge_.voices) {
                SDL_DestroyAudioStream(stream);
            }
            for (Clip* clip : {&bossChangeMode_, &bossSharpHit_,
                    &bossRiser_, &battleExplosion_}) {
                for (SDL_AudioStream* stream : clip->voices) {
                    SDL_DestroyAudioStream(stream);
                }
                clip->voices.clear();
            }
            impact_.voices.clear();
            rocketLaunch_.voices.clear();
            punchSwing_.voices.clear();
            rocketReconnect_.voices.clear();
            chargeHit_.voices.clear();
            dodge_.voices.clear();
            if (audioDevice_ != 0) {
                SDL_CloseAudioDevice(audioDevice_);
                audioDevice_ = 0;
            }
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            audioInitialized_ = false;
        }
    } else {
        SDL_Log("Audio playback unavailable: %s", SDL_GetError());
    }

    if (enableGamepad) {
        if (SDL_InitSubSystem(SDL_INIT_GAMEPAD)) {
            gamepadInitialized_ = true;
            OpenFirstGamepad();
        } else {
            SDL_Log("Gamepad vibration unavailable: %s", SDL_GetError());
        }
    }
    SDL_Log("Combat feedback: audio=%s, gamepad=%s",
        HasAudioDevice() ? "ready" : "unavailable",
        HasGamepad() ? "connected" : "not connected");
    return true;
}

void CombatFeedback::OpenFirstGamepad() {
    if (gamepad_ != nullptr) {
        return;
    }
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids != nullptr) {
        for (int index = 0; index < count && gamepad_ == nullptr; ++index) {
            gamepad_ = SDL_OpenGamepad(ids[index]);
        }
        SDL_free(ids);
    }
    rumbleSupported_ = true;
}

void CombatFeedback::ProcessEvent(const SDL_Event& event) {
    if (!gamepadInitialized_) {
        return;
    }
    if (event.type == SDL_EVENT_GAMEPAD_ADDED && gamepad_ == nullptr) {
        gamepad_ = SDL_OpenGamepad(event.gdevice.which);
        rumbleSupported_ = true;
    } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED && gamepad_ != nullptr &&
        SDL_GetGamepadID(gamepad_) == event.gdevice.which) {
        SDL_CloseGamepad(gamepad_);
        gamepad_ = nullptr;
        rumbleUntil_ = 0;
        rumblePriority_ = 0;
        OpenFirstGamepad();
    }
}

void CombatFeedback::PlayClip(Clip& clip, float gain) {
    if (audioDevice_ == 0 || clip.voices.empty()) {
        return;
    }
    size_t selected = clip.nextVoice;
    for (size_t index = 0; index < clip.voices.size(); ++index) {
        const size_t candidate = (clip.nextVoice + index) % clip.voices.size();
        if (SDL_GetAudioStreamQueued(clip.voices[candidate]) == 0) {
            selected = candidate;
            break;
        }
    }
    SDL_AudioStream* voice = clip.voices[selected];
    clip.nextVoice = (selected + 1) % clip.voices.size();
    if (!SDL_ClearAudioStream(voice) || !SDL_SetAudioStreamGain(voice, gain) ||
        !SDL_PutAudioStreamData(voice, clip.samples.data(),
            static_cast<int>(clip.samples.size())) ||
        !SDL_FlushAudioStream(voice)) {
        SDL_Log("Could not play combat sound: %s", SDL_GetError());
    }
}

void CombatFeedback::Rumble(Uint16 lowFrequency, Uint16 highFrequency,
    Uint32 durationMilliseconds, int priority) {
    if (gamepad_ == nullptr || !rumbleSupported_ || !SDL_GamepadConnected(gamepad_)) {
        return;
    }
    const Uint64 now = SDL_GetTicks();
    if (now < rumbleUntil_ && priority < rumblePriority_) {
        return;
    }
    if (!SDL_RumbleGamepad(gamepad_, lowFrequency, highFrequency,
            durationMilliseconds)) {
        SDL_Log("Gamepad vibration unavailable: %s", SDL_GetError());
        rumbleSupported_ = false;
        return;
    }
    rumbleUntil_ = now + durationMilliseconds;
    rumblePriority_ = priority;
}

void CombatFeedback::PlayPunchHit() {
    PlayClip(impact_, config_.punchImpactGain);
    Rumble(config_.punchRumbleLow, config_.punchRumbleHigh,
        config_.punchRumbleMs, 1);
}

void CombatFeedback::PlayRocketHit() {
    PlayClip(impact_, config_.rocketImpactGain);
}

void CombatFeedback::PlayRocketLaunch() {
    PlayClip(rocketLaunch_, config_.rocketLaunchGain);
    Rumble(config_.rocketLaunchRumbleLow, config_.rocketLaunchRumbleHigh,
        config_.rocketLaunchRumbleMs, 2);
}

void CombatFeedback::PlayUltimateLaunch() {
    PlayClip(rocketLaunch_, config_.ultimateLaunchGain);
    Rumble(config_.rocketLaunchRumbleLow, config_.rocketLaunchRumbleHigh,
        config_.rocketLaunchRumbleMs, 2);
}

void CombatFeedback::PlayDodge() {
    PlayClip(dodge_, config_.dodgeGain);
    Rumble(config_.dodgeRumbleLow, config_.dodgeRumbleHigh,
        config_.dodgeRumbleMs, 1);
}

void CombatFeedback::PlaySystemRestart() {
    Rumble(config_.systemRestartRumbleLow,
        config_.systemRestartRumbleHigh,
        config_.systemRestartRumbleMs, 3);
}

void CombatFeedback::PlayPunchSwing() {
    PlayClip(punchSwing_, config_.punchSwingGain);
}

void CombatFeedback::PlayRocketReconnect() {
    PlayClip(rocketReconnect_, config_.rocketReconnectGain);
}

void CombatFeedback::PlayPlayerHit(PlayerHitType type, bool damagedHitPoints) {
    if (type == PlayerHitType::ChargeUnit || type == PlayerHitType::BossSpreadBurst) {
        PlayClip(chargeHit_, config_.chargeHitGain);
        Rumble(config_.chargeHitRumbleLow, config_.chargeHitRumbleHigh,
            config_.chargeHitRumbleMs, 3);
    } else {
        Rumble(damagedHitPoints ? config_.projectileHitPointRumbleLow
                : config_.projectileShieldRumbleLow,
            damagedHitPoints ? config_.projectileHitPointRumbleHigh
                : config_.projectileShieldRumbleHigh,
            config_.projectileRumbleMs, 3);
    }
}

void CombatFeedback::PlayBattleEvent(BattleFeedbackEvent event) {
    switch (event) {
    case BattleFeedbackEvent::EnemyDefeated:
    case BattleFeedbackEvent::BossDefeated:
        PlayClip(battleExplosion_, config_.battleCueGain);
        Rumble(config_.punchRumbleLow, config_.punchRumbleHigh,
            config_.punchRumbleMs, 2);
        break;
    case BattleFeedbackEvent::ProjectileLaunched:
        PlayClip(bossRiser_, config_.battleCueGain * 0.35f);
        break;
    case BattleFeedbackEvent::BossModeChanged:
        PlayClip(bossChangeMode_, config_.battleCueGain);
        break;
    case BattleFeedbackEvent::BossCrystalLaunched:
        PlayClip(bossRiser_, config_.battleCueGain);
        break;
    case BattleFeedbackEvent::BossDamaged:
        PlayClip(bossSharpHit_, config_.battleCueGain);
        Rumble(config_.punchRumbleLow, config_.punchRumbleHigh,
            config_.punchRumbleMs, 2);
        break;
    case BattleFeedbackEvent::BaseDamaged:
        PlayClip(battleExplosion_, config_.battleCueGain * 0.6f);
        Rumble(config_.projectileHitPointRumbleLow,
            config_.projectileHitPointRumbleHigh,
            config_.projectileRumbleMs, 2);
        break;
    }
}

void CombatFeedback::Shutdown() {
    if (gamepad_ != nullptr) {
        if (SDL_GamepadConnected(gamepad_)) {
            SDL_RumbleGamepad(gamepad_, 0, 0, 0);
        }
        SDL_CloseGamepad(gamepad_);
        gamepad_ = nullptr;
    }
    if (gamepadInitialized_) {
        SDL_QuitSubSystem(SDL_INIT_GAMEPAD);
        gamepadInitialized_ = false;
    }
    for (SDL_AudioStream* stream : impact_.voices) {
        SDL_DestroyAudioStream(stream);
    }
    for (SDL_AudioStream* stream : rocketLaunch_.voices) {
        SDL_DestroyAudioStream(stream);
    }
    for (SDL_AudioStream* stream : punchSwing_.voices) {
        SDL_DestroyAudioStream(stream);
    }
    for (SDL_AudioStream* stream : rocketReconnect_.voices) {
        SDL_DestroyAudioStream(stream);
    }
    for (SDL_AudioStream* stream : chargeHit_.voices) {
        SDL_DestroyAudioStream(stream);
    }
    for (SDL_AudioStream* stream : dodge_.voices) {
        SDL_DestroyAudioStream(stream);
    }
    for (Clip* clip : {&bossChangeMode_, &bossSharpHit_,
            &bossRiser_, &battleExplosion_}) {
        for (SDL_AudioStream* stream : clip->voices) {
            SDL_DestroyAudioStream(stream);
        }
        clip->voices.clear();
        clip->samples.clear();
    }
    impact_.voices.clear();
    rocketLaunch_.voices.clear();
    punchSwing_.voices.clear();
    rocketReconnect_.voices.clear();
    chargeHit_.voices.clear();
    dodge_.voices.clear();
    impact_.samples.clear();
    rocketLaunch_.samples.clear();
    punchSwing_.samples.clear();
    rocketReconnect_.samples.clear();
    chargeHit_.samples.clear();
    dodge_.samples.clear();
    if (audioDevice_ != 0) {
        SDL_CloseAudioDevice(audioDevice_);
        audioDevice_ = 0;
    }
    if (audioInitialized_) {
        SDL_QuitSubSystem(SDL_INIT_AUDIO);
        audioInitialized_ = false;
    }
    rumbleUntil_ = 0;
    rumblePriority_ = 0;
}
