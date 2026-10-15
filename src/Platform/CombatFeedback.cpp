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
        if (!SDL_BindAudioStream(audioDevice_, stream)) {
            SDL_DestroyAudioStream(stream);
            return false;
        }
        clip.voices.push_back(stream);
        clip.busyUntil.push_back(0);
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
    for (const auto& [name, cue] : config_.cues) {
        if (!LoadClip(soundDirectory / cue.wav, cues_[name], error)) {
            Shutdown();
            return false;
        }
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
                clip->busyUntil.clear();
            }
            impact_.voices.clear();
            impact_.busyUntil.clear();
            rocketLaunch_.voices.clear();
            rocketLaunch_.busyUntil.clear();
            punchSwing_.voices.clear();
            punchSwing_.busyUntil.clear();
            rocketReconnect_.voices.clear();
            rocketReconnect_.busyUntil.clear();
            chargeHit_.voices.clear();
            chargeHit_.busyUntil.clear();
            dodge_.voices.clear();
            dodge_.busyUntil.clear();
            if (audioDevice_ != 0) {
                SDL_CloseAudioDevice(audioDevice_);
                audioDevice_ = 0;
            }
            SDL_QuitSubSystem(SDL_INIT_AUDIO);
            audioInitialized_ = false;
        }
        if (audioDevice_ != 0) {
            for (auto& [name, clip] : cues_) {
                if (!CreateVoices(clip, name == "engineLoop" ? 1 : 2)) {
                    error = "Could not create sound stream for '" + name +
                        "': " + SDL_GetError();
                    Shutdown();
                    return false;
                }
            }
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
    size_t selected = clip.voices.size();
    const Uint64 now = SDL_GetTicksNS();
    for (size_t index = 0; index < clip.voices.size(); ++index) {
        const size_t candidate = (clip.nextVoice + index) % clip.voices.size();
        if (now >= clip.busyUntil[candidate] &&
            SDL_GetAudioStreamQueued(clip.voices[candidate]) == 0) {
            selected = candidate;
            break;
        }
    }
    if (selected == clip.voices.size()) {
        if (!CreateVoices(clip, 1)) {
            SDL_Log("Could not create overlapping sound stream: %s", SDL_GetError());
            return;
        }
        selected = clip.voices.size() - 1;
    }
    SDL_AudioStream* voice = clip.voices[selected];
    clip.nextVoice = (selected + 1) % clip.voices.size();
    if (!SDL_ClearAudioStream(voice) || !SDL_SetAudioStreamGain(voice, gain) ||
        !SDL_PutAudioStreamData(voice, clip.samples.data(),
            static_cast<int>(clip.samples.size())) ||
        !SDL_FlushAudioStream(voice)) {
        SDL_Log("Could not play combat sound: %s", SDL_GetError());
    } else {
        const Uint64 frames = clip.samples.size() /
            static_cast<Uint64>(SDL_AUDIO_FRAMESIZE(clip.spec));
        clip.busyUntil[selected] = now + frames * 1000000000ULL /
            static_cast<Uint64>(clip.spec.freq);
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
    PlayCue("cockpitStart");
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

void CombatFeedback::PlayPlayerHit(PlayerHitType type,
    bool damagedHitPoints, bool bossNeedleHit) {
    if (type == PlayerHitType::ChargeUnit ||
        type == PlayerHitType::BossSpreadBurst ||
        type == PlayerHitType::NeedleFinisher) {
        PlayClip(chargeHit_, config_.chargeHitGain);
        Rumble(config_.chargeHitRumbleLow, config_.chargeHitRumbleHigh,
            config_.chargeHitRumbleMs, 3);
    } else {
        if (bossNeedleHit) {
            PlayClip(bossSharpHit_, damagedHitPoints
                ? config_.bossSharpHitPointGain
                : config_.bossSharpShieldHitGain);
        }
        Rumble(damagedHitPoints ? config_.projectileHitPointRumbleLow
                : config_.projectileShieldRumbleLow,
            damagedHitPoints ? config_.projectileHitPointRumbleHigh
                : config_.projectileShieldRumbleHigh,
            config_.projectileRumbleMs, 3);
    }
}

void CombatFeedback::PlayCue(const std::string& name) {
    const auto found = cues_.find(name);
    const auto setting = config_.cues.find(name);
    if (found != cues_.end() && setting != config_.cues.end() &&
        name != "engineLoop") {
        PlayClip(found->second, setting->second.gain);
    }
}

void CombatFeedback::StopCue(const std::string& name) {
    const auto found = cues_.find(name);
    if (found == cues_.end()) return;
    for (SDL_AudioStream* voice : found->second.voices)
        SDL_ClearAudioStream(voice);
    std::fill(found->second.busyUntil.begin(),
        found->second.busyUntil.end(), 0);
}

void CombatFeedback::SetCueGain(const std::string& name, float gain) {
    const auto found = cues_.find(name);
    if (found == cues_.end()) return;
    for (SDL_AudioStream* voice : found->second.voices)
        SDL_SetAudioStreamGain(voice, std::clamp(gain, 0.0f, 2.0f));
}

void CombatFeedback::SetRocketLaunchGain(float gain) {
    for (SDL_AudioStream* voice : rocketLaunch_.voices)
        SDL_SetAudioStreamGain(voice, std::clamp(gain, 0.0f, 2.0f));
}

void CombatFeedback::StopBattleAudio() {
    for (Clip* clip : {&impact_, &rocketLaunch_, &punchSwing_,
            &rocketReconnect_, &chargeHit_, &dodge_, &bossChangeMode_,
            &bossSharpHit_, &bossRiser_, &battleExplosion_}) {
        for (SDL_AudioStream* voice : clip->voices)
            SDL_ClearAudioStream(voice);
        std::fill(clip->busyUntil.begin(), clip->busyUntil.end(), 0);
    }
    for (auto& [name, clip] : cues_) {
        for (SDL_AudioStream* voice : clip.voices)
            SDL_ClearAudioStream(voice);
        std::fill(clip.busyUntil.begin(), clip.busyUntil.end(), 0);
    }
    engineRequested_ = false;
    engineFadeRemaining_ = 0.0f;
}

void CombatFeedback::SetEngineLoop(bool enabled) {
    if (engineRequested_ == enabled) return;
    engineRequested_ = enabled;
    engineFadeRemaining_ = enabled ? 2.0f : 0.0f;
    const auto found = cues_.find("engineLoop");
    if (!enabled && found != cues_.end() && !found->second.voices.empty()) {
        SDL_ClearAudioStream(found->second.voices.front());
    }
}

bool CombatFeedback::IsEngineLoopQueued() const {
    const auto found = cues_.find("engineLoop");
    return found != cues_.end() && !found->second.voices.empty() &&
        SDL_GetAudioStreamQueued(found->second.voices.front()) > 0;
}

size_t CombatFeedback::GetAllocatedBattleVoiceCount() const {
    size_t count = 0;
    for (const Clip* clip : {&impact_, &rocketLaunch_, &punchSwing_,
            &rocketReconnect_, &chargeHit_, &dodge_, &bossChangeMode_,
            &bossSharpHit_, &bossRiser_, &battleExplosion_})
        count += clip->voices.size();
    for (const auto& [name, clip] : cues_)
        count += clip.voices.size();
    return count;
}

size_t CombatFeedback::GetQueuedBattleVoiceCount() const {
    size_t count = 0;
    for (const Clip* clip : {&impact_, &rocketLaunch_, &punchSwing_,
            &rocketReconnect_, &chargeHit_, &dodge_, &bossChangeMode_,
            &bossSharpHit_, &bossRiser_, &battleExplosion_}) {
        for (SDL_AudioStream* voice : clip->voices)
            count += SDL_GetAudioStreamQueued(voice) > 0 ? 1 : 0;
    }
    for (const auto& [name, clip] : cues_) {
        for (SDL_AudioStream* voice : clip.voices)
            count += SDL_GetAudioStreamQueued(voice) > 0 ? 1 : 0;
    }
    return count;
}

void CombatFeedback::PlayBattleEvent(BattleFeedbackEvent event) {
    switch (event) {
    case BattleFeedbackEvent::PunchSwing:
        PlayPunchSwing();
        break;
    case BattleFeedbackEvent::RocketReconnect:
        PlayRocketReconnect();
        break;
    case BattleFeedbackEvent::PunchImpact:
        PlayPunchHit();
        break;
    case BattleFeedbackEvent::RocketImpact:
        PlayRocketHit();
        break;
    case BattleFeedbackEvent::EnemyDefeated:
        PlayClip(impact_, 0.9f);
        Rumble(config_.punchRumbleLow, config_.punchRumbleHigh,
            config_.punchRumbleMs, 2);
        break;
    case BattleFeedbackEvent::EnemyExploded:
        PlayCue("projectileExplosion");
        break;
    case BattleFeedbackEvent::BossDefeatSmallExplosion:
        PlayClip(battleExplosion_, config_.bossSmallExplosionGain);
        break;
    case BattleFeedbackEvent::BossDefeated:
        PlayClip(battleExplosion_, config_.bossFinalExplosionGain);
        Rumble(config_.punchRumbleLow, config_.punchRumbleHigh,
            config_.punchRumbleMs, 2);
        break;
    case BattleFeedbackEvent::ProjectileLaunched:
        PlayCue("missileLaunch");
        break;
    case BattleFeedbackEvent::BossNeedlePoseChanged:
        PlayClip(bossChangeMode_, config_.bossModeChangeGain);
        break;
    case BattleFeedbackEvent::BossCrystalLaunched:
        PlayClip(bossRiser_, config_.bossCrystalRiserGain);
        break;
    case BattleFeedbackEvent::BossDamaged:
        PlayClip(impact_, config_.rocketImpactGain);
        Rumble(config_.punchRumbleLow, config_.punchRumbleHigh,
            config_.punchRumbleMs, 2);
        break;
    case BattleFeedbackEvent::BaseDamaged:
        PlayCue("projectileExplosion");
        Rumble(config_.projectileHitPointRumbleLow,
            config_.projectileHitPointRumbleHigh,
            config_.projectileRumbleMs, 2);
        break;
    case BattleFeedbackEvent::EnemyLanded:
        PlayCue("landing");
        break;
    case BattleFeedbackEvent::ChargePreparing:
        PlayCue("chargePrepare");
        break;
    case BattleFeedbackEvent::ChargeDashing:
        PlayCue("chargeDash");
        break;
    case BattleFeedbackEvent::ChargeWallImpact:
        PlayClip(chargeHit_, config_.chargeHitGain);
        break;
    case BattleFeedbackEvent::BossDodged:
        PlayCue("bossDash");
        break;
    case BattleFeedbackEvent::BossStriking:
        PlayCue("bossStrike");
        break;
    case BattleFeedbackEvent::BossNeedleStarted:
        PlayCue("bossNeedle");
        break;
    case BattleFeedbackEvent::BossBarrageStarted:
        PlayCue("bossBarrage");
        PlayClip(rocketLaunch_, config_.rocketLaunchGain);
        break;
    case BattleFeedbackEvent::BossChargeLaunched:
        PlayClip(rocketLaunch_, config_.rocketLaunchGain);
        break;
    case BattleFeedbackEvent::ProjectileExploded:
        PlayCue("projectileExplosion");
        break;
    case BattleFeedbackEvent::ShieldedProjectileExploded:
        PlayClip(cues_.at("projectileExplosion"),
            config_.shieldedProjectileExplosionGain);
        break;
    case BattleFeedbackEvent::BossSpreadBurstExploded:
        PlayClip(battleExplosion_, config_.bossSpreadBurstExplosionGain);
        break;
    }
}

bool CombatFeedback::InitializeFullGameAudio(
    const std::filesystem::path& soundDirectory, std::string& error) {
    for (const char* name : {"elevator", "hisAction", "energetic", "boss", "win"}) {
        Clip& clip = music_[name];
        if (!LoadClip(soundDirectory / "BGM" / (std::string(name) + ".wav"),
                clip, error)) return false;
        if (audioDevice_ != 0 && !CreateVoices(clip, 1)) {
            error = std::string("Could not create music stream for ") + name + ": " + SDL_GetError();
            return false;
        }
    }
    for (const char* name : {"personnel.wav", "system/system_check_start.wav",
            "system/system_visual_link_pass.wav", "system/system_mobility_pass.wav",
            "system/system_punch_pass.wav", "system/system_rocket_pass.wav",
            "system/system_defense_pass.wav", "system/system_dodge_pass.wav",
            "system/system_all_green.wav"}) {
        Clip& clip = tutorialVoices_[name];
        if (!LoadClip(soundDirectory / "Tutorial" / name, clip, error)) return false;
        if (audioDevice_ != 0 && !CreateVoices(clip, 1)) {
            error = std::string("Could not create tutorial voice stream for ") + name +
                ": " + SDL_GetError();
            return false;
        }
    }
    return true;
}

void CombatFeedback::SetMusic(const std::string& name) {
    if (name == currentMusic_) return;
    if (!name.empty() && !music_.contains(name)) return;
    if (!currentMusic_.empty() && audioDevice_ != 0)
        SDL_ClearAudioStream(music_[currentMusic_].voices.front());
    if (name == "elevator") {
        tutorialCheckPending_ = false;
        tutorialCheckPlayed_ = false;
    }
    currentMusic_ = name;
    if (audioDevice_ != 0 && !currentMusic_.empty())
        UpdateMusic(0.0f);
}

bool CombatFeedback::IsMusicQueued(const std::string& name) const {
    const auto found = music_.find(name);
    return found != music_.end() && !found->second.voices.empty() &&
        SDL_GetAudioStreamQueued(found->second.voices.front()) > 0;
}

void CombatFeedback::PlayTutorialVoice(const std::string& name) {
    const auto found = tutorialVoices_.find(name);
    if (found == tutorialVoices_.end()) return;
    if (!currentVoice_.empty()) {
        Clip& previous = tutorialVoices_[currentVoice_];
        for (SDL_AudioStream* stream : previous.voices)
            SDL_ClearAudioStream(stream);
        std::fill(previous.busyUntil.begin(), previous.busyUntil.end(), 0);
    }
    currentVoice_ = name;
    PlayClip(found->second, name == "personnel.wav"
        ? config_.tutorialPersonnelGain : 0.9f);
    if (name == "personnel.wav" && !tutorialCheckPlayed_)
        tutorialCheckPending_ = true;
}

void CombatFeedback::StopTutorialVoice() {
    if (const auto found = tutorialVoices_.find(currentVoice_);
        found != tutorialVoices_.end()) {
        for (SDL_AudioStream* stream : found->second.voices)
            SDL_ClearAudioStream(stream);
        std::fill(found->second.busyUntil.begin(),
            found->second.busyUntil.end(), 0);
    }
    currentVoice_.clear();
    tutorialCheckPending_ = false;
}

bool CombatFeedback::IsTutorialVoicePlaying() const {
    if (currentVoice_.empty()) return false;
    const auto found = tutorialVoices_.find(currentVoice_);
    return found != tutorialVoices_.end() && !found->second.voices.empty() &&
        SDL_GetAudioStreamQueued(found->second.voices.front()) > 0;
}

bool CombatFeedback::IsTutorialSystemVoicePlaying() const {
    return currentVoice_.starts_with("system/") && IsTutorialVoicePlaying();
}

void CombatFeedback::UpdateMusic(float seconds) {
    if (audioDevice_ == 0) return;
    if (engineRequested_) {
        const auto found = cues_.find("engineLoop");
        if (found != cues_.end() && !found->second.voices.empty()) {
            Clip& clip = found->second;
            SDL_AudioStream* stream = clip.voices.front();
            engineFadeRemaining_ = std::max(0.0f,
                engineFadeRemaining_ - seconds);
            const float gain = 0.55f + (config_.cues.at("engineLoop").gain -
                0.55f) * (1.0f - engineFadeRemaining_ / 2.0f);
            SDL_SetAudioStreamGain(stream, gain);
            if (SDL_GetAudioStreamQueued(stream) <=
                    static_cast<int>(clip.samples.size() / 4)) {
                SDL_PutAudioStreamData(stream, clip.samples.data(),
                    static_cast<int>(clip.samples.size()));
                SDL_FlushAudioStream(stream);
            }
        }
    }
    if (tutorialCheckPending_ && !IsTutorialVoicePlaying()) {
        tutorialCheckPending_ = false;
        tutorialCheckPlayed_ = true;
        PlayTutorialVoice("system/system_check_start.wav");
    }
    if (currentMusic_.empty()) return;
    Clip& clip = music_[currentMusic_];
    SDL_AudioStream* stream = clip.voices.front();
    const float gain = currentMusic_ == "elevator"
        ? (IsTutorialVoicePlaying() ? 0.20f : 0.45f)
        : currentMusic_ == "hisAction" ? 0.42f
        : currentMusic_ == "boss" ? 0.48f
        : currentMusic_ == "win" ? 0.50f : 0.45f;
    SDL_SetAudioStreamGain(stream, gain);
    if (SDL_GetAudioStreamQueued(stream) <=
            static_cast<int>(clip.samples.size() / 4)) {
        if (!SDL_PutAudioStreamData(stream, clip.samples.data(),
                static_cast<int>(clip.samples.size())) || !SDL_FlushAudioStream(stream)) {
            SDL_Log("Could not loop music %s: %s", currentMusic_.c_str(), SDL_GetError());
        }
    }
}

void CombatFeedback::Shutdown() {
    for (auto* collection : {&music_, &tutorialVoices_, &cues_}) {
        for (auto& [name, clip] : *collection) {
            for (SDL_AudioStream* stream : clip.voices) SDL_DestroyAudioStream(stream);
            clip.voices.clear();
            clip.busyUntil.clear();
        }
        collection->clear();
    }
    currentMusic_.clear();
    currentVoice_.clear();
    tutorialCheckPending_ = false;
    tutorialCheckPlayed_ = false;
    engineRequested_ = false;
    engineFadeRemaining_ = 0.0f;
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
        clip->busyUntil.clear();
        clip->samples.clear();
    }
    impact_.voices.clear();
    impact_.busyUntil.clear();
    rocketLaunch_.voices.clear();
    rocketLaunch_.busyUntil.clear();
    punchSwing_.voices.clear();
    punchSwing_.busyUntil.clear();
    rocketReconnect_.voices.clear();
    rocketReconnect_.busyUntil.clear();
    chargeHit_.voices.clear();
    chargeHit_.busyUntil.clear();
    dodge_.voices.clear();
    dodge_.busyUntil.clear();
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
