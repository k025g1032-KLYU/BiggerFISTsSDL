#pragma once

#include "Data/GameConfig.h"
#include "Data/PlayerCombatConfig.h"
#include "Gameplay/Combat/CombatHitResolver.h"
#include "Gameplay/Player/PlayerMovement.h"
#include "Gameplay/Player/PunchArm.h"
#include "Gameplay/Player/PlayerVitality.h"
#include "Input/GameInput.h"
#include "World/FirstPersonCamera.h"

#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>

enum class TargetDisplayKind { Static, Enemy, Boss, Projectile };

struct FirstPersonTarget {
    std::string id;
    Vector3 position{};
    int hitPoints = 1;
    Vector3 previousPosition{};
    bool movedSinceLastStep = false;
    float radius = 1.25f;
    float punchRadiusMultiplier = 1.0f;
    TargetDisplayKind displayKind = TargetDisplayKind::Static;
};

struct PlayerHitFeedback {
    PlayerHitType type = PlayerHitType::Projectile;
    PlayerHitResult result{};
    bool bossNeedleHit = false;
};

struct FirstPersonEvents {
    bool targetDestroyed = false;
    bool targetReset = false;
    std::vector<std::string> destroyedTargetIds;
    std::vector<CombatHitEvent> targetHits;
    bool punchHit = false;
    bool rocketHit = false;
    bool leftRocketLaunched = false;
    bool rightRocketLaunched = false;
    bool leftRocketReconnected = false;
    bool rightRocketReconnected = false;
    bool leftPunchBegan = false;
    bool rightPunchBegan = false;
    bool ultimateLaunched = false;
    bool ultimateReturned = false;
    bool systemRestarted = false;
    bool systemOfflineStarted = false;
    bool footstep = false;
    bool dodgeStarted = false;
    bool playerHit = false;
    bool playerDefeated = false;
    std::vector<PlayerHitFeedback> playerHits;
    PlayerHitType playerHitType = PlayerHitType::Projectile;
    bool playerHitWasBossNeedle = false;
    PlayerHitResult playerHitResult{};
};

enum class UltimateState { Idle, Forming, Ready, Flying, Returning, Offline };

class FirstPersonGame {
public:
    FirstPersonGame(const GameConfig& config, const Vector3& initialEyePosition,
        const Vector3& targetPosition, PlayerCombatConfig combatConfig = {});
    FirstPersonGame(const GameConfig& config, const Vector3& initialEyePosition,
        std::vector<FirstPersonTarget> targets, PlayerCombatConfig combatConfig = {});
    void Update(double deltaSeconds, const GameInput& input,
        float mouseDeltaX, float mouseDeltaY, bool deferTargetHits = false);
    void ResolveTargetHits();
    void ResetCombatState();
    void UpdateLookOnly(float mouseDeltaX, float mouseDeltaY);
    bool SpawnTarget(const FirstPersonTarget& target);
    bool MoveTarget(std::string_view id, const Vector3& position);
    bool RemoveTarget(std::string_view id);
    PlayerHitResult ReceivePlayerHit(PlayerHitType type,
        const Vector3& pushDirection = {0.0f, 0.0f, 1.0f},
        bool bossNeedleHit = false);
    void StartNeedleStun();
    void StartBossBodyPush(const Vector3& direction);
    void EndNeedleStun();
    void TriggerPracticeOffline();
    float GetOfflineRemaining() const { return needleOfflineTimer_ > 0.0f
        ? needleOfflineTimer_ : ultimateOfflineTimer_; }
    const PlayerMovement& GetPlayer() const { return player_; }
    const FirstPersonCamera& GetCamera() const { return camera_; }
    void BeginTutorialVisualCheck() { camera_.BeginTutorialVisualCheck(); }
    void BeginTutorialCockpitAlignment() {
        camera_.BeginTutorialCockpitAlignment(player_.GetBodyYaw());
    }
    const PunchArm& GetLeftArm() const { return leftArm_; }
    const PunchArm& GetRightArm() const { return rightArm_; }
    const PlayerVitality& GetVitality() const { return vitality_; }
    const std::vector<FirstPersonTarget>& GetTargets() const { return targets_; }
    int GetTargetHitPoints() const {
        return targets_.empty() ? 0 : targets_.front().hitPoints;
    }
    bool IsWarmBackground() const { return warmBackground_; }
    void EnableUltimate(bool enabled) { ultimateEnabled_ = enabled; }
    void EnableBaseCollision(bool enabled) {
        player_.SetResearchInstituteCollisionEnabled(enabled);
    }
    UltimateState GetUltimateState() const { return ultimateState_; }
    float GetUltimateChargeRatio() const;
    bool IsSystemOffline() const {
        return ultimateShutdown_ ||
            needleOfflineTimer_ > 0.0f;
    }
    bool IsUltimateDamageActiveThisFrame() const {
        return ultimateDamageActiveThisFrame_;
    }
    const Vector3& GetUltimatePosition() const { return ultimatePosition_; }
    const Vector3& GetPreviousUltimatePosition() const { return previousUltimatePosition_; }
    const Vector3& GetUltimateDirection() const { return ultimateDirection_; }
    float GetUltimateSpeed() const { return combatConfig_.ultimateSpeed; }
    Vector3 GetUltimateFormationPosition() const;
    float GetUltimateFormationRatio() const;
    float GetUltimateRadius() const { return combatConfig_.ultimateRadius; }
    Vector3 GetUltimateReturnFistPosition(PunchArmSide side) const;
    double GetElapsedSeconds() const { return elapsedSeconds_; }
    const FirstPersonEvents& GetEvents() const { return events_; }

private:
    void ProcessTargetHits();
    void ResetTarget();
    void UpdateUltimate(float seconds, const GameInput& input,
        const PunchArmInput& leftInput, const PunchArmInput& rightInput,
        bool& lockArms);

    PlayerCombatConfig combatConfig_;
    PlayerMovement player_;
    FirstPersonCamera camera_;
    PunchArm leftArm_;
    PunchArm rightArm_;
    PlayerVitality vitality_;
    Vector3 initialEyePosition_{};
    std::vector<FirstPersonTarget> targets_;
    std::vector<FirstPersonTarget> initialTargets_;
    bool warmBackground_ = false;
    double elapsedSeconds_ = 0.0;
    float maxArmDeltaTime_ = 0.05f;
    FirstPersonEvents events_{};
    std::unordered_set<std::string> leftHitIds_;
    std::unordered_set<std::string> rightHitIds_;
    bool ultimateEnabled_ = false;
    UltimateState ultimateState_ = UltimateState::Idle;
    bool ultimateDamageActiveThisFrame_ = false;
    bool ultimateShutdown_ = false;
    Vector3 ultimatePosition_{};
    Vector3 previousUltimatePosition_{};
    Vector3 ultimateDirection_{0.0f, 0.0f, 1.0f};
    float ultimateChargeTimer_ = 0.0f;
    float ultimateTravel_ = 0.0f;
    float ultimateRecoilTimer_ = 0.0f;
    float ultimateOfflineTimer_ = 0.0f;
    float needleShutdownDelay_ = 0.0f;
    float needleOfflineTimer_ = 0.0f;
};
