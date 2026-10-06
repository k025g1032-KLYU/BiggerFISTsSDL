#pragma once

#include "Data/GameConfig.h"
#include "Data/PlayerCombatConfig.h"
#include "Gameplay/Player/PlayerMovement.h"
#include "Gameplay/Player/PunchArm.h"
#include "Gameplay/Player/PlayerVitality.h"
#include "Input/GameInput.h"
#include "World/FirstPersonCamera.h"

#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>

struct FirstPersonTarget {
    std::string id;
    Vector3 position{};
    int hitPoints = 1;
    Vector3 previousPosition{};
    bool movedSinceLastStep = false;
};

struct FirstPersonEvents {
    bool targetDestroyed = false;
    bool targetReset = false;
    bool punchHit = false;
    bool rocketHit = false;
    bool leftRocketLaunched = false;
    bool rightRocketLaunched = false;
    bool leftRocketReconnected = false;
    bool rightRocketReconnected = false;
    bool leftPunchBegan = false;
    bool rightPunchBegan = false;
    bool playerHit = false;
    bool playerDefeated = false;
    PlayerHitType playerHitType = PlayerHitType::Projectile;
    PlayerHitResult playerHitResult{};
};

class FirstPersonGame {
public:
    FirstPersonGame(const GameConfig& config, const Vector3& initialEyePosition,
        const Vector3& targetPosition, PlayerCombatConfig combatConfig = {});
    FirstPersonGame(const GameConfig& config, const Vector3& initialEyePosition,
        std::vector<FirstPersonTarget> targets, PlayerCombatConfig combatConfig = {});
    void Update(double deltaSeconds, const GameInput& input,
        float mouseDeltaX, float mouseDeltaY);
    bool SpawnTarget(const FirstPersonTarget& target);
    bool MoveTarget(std::string_view id, const Vector3& position);
    bool RemoveTarget(std::string_view id);
    PlayerHitResult ReceivePlayerHit(PlayerHitType type,
        const Vector3& pushDirection = {0.0f, 0.0f, 1.0f});
    const PlayerMovement& GetPlayer() const { return player_; }
    const FirstPersonCamera& GetCamera() const { return camera_; }
    const PunchArm& GetLeftArm() const { return leftArm_; }
    const PunchArm& GetRightArm() const { return rightArm_; }
    const PlayerVitality& GetVitality() const { return vitality_; }
    const std::vector<FirstPersonTarget>& GetTargets() const { return targets_; }
    int GetTargetHitPoints() const {
        return targets_.empty() ? 0 : targets_.front().hitPoints;
    }
    bool IsWarmBackground() const { return warmBackground_; }
    double GetElapsedSeconds() const { return elapsedSeconds_; }
    const FirstPersonEvents& GetEvents() const { return events_; }

private:
    void ProcessTargetHits();
    void ResetTarget();

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
};
