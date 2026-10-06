#pragma once

#include "Data/GameConfig.h"
#include "Gameplay/Player/PlayerMovement.h"
#include "Gameplay/Player/PunchArm.h"
#include "Input/GameInput.h"
#include "World/FirstPersonCamera.h"

struct FirstPersonEvents {
    bool targetDestroyed = false;
    bool targetReset = false;
};

class FirstPersonGame {
public:
    FirstPersonGame(const GameConfig& config, const Vector3& initialEyePosition,
        const Vector3& targetPosition);
    void Update(double deltaSeconds, const GameInput& input,
        float mouseDeltaX, float mouseDeltaY);
    const PlayerMovement& GetPlayer() const { return player_; }
    const FirstPersonCamera& GetCamera() const { return camera_; }
    const PunchArm& GetLeftArm() const { return leftArm_; }
    const PunchArm& GetRightArm() const { return rightArm_; }
    const Vector3& GetTargetPosition() const { return targetPosition_; }
    int GetTargetHitPoints() const { return targetHitPoints_; }
    bool IsWarmBackground() const { return warmBackground_; }
    double GetElapsedSeconds() const { return elapsedSeconds_; }
    const FirstPersonEvents& GetEvents() const { return events_; }

private:
    void ProcessTargetHits();
    void ResetTarget();

    PlayerMovement player_;
    FirstPersonCamera camera_;
    PunchArm leftArm_{PunchArmSide::Left};
    PunchArm rightArm_{PunchArmSide::Right};
    Vector3 targetPosition_{};
    int targetHitPoints_ = 0;
    bool warmBackground_ = false;
    double elapsedSeconds_ = 0.0;
    float maxArmDeltaTime_ = 0.05f;
    FirstPersonEvents events_{};
};
