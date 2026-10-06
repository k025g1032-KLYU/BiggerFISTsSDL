#include "Game/FirstPersonGame.h"

#include "Collision/Collision.h"
#include "Gameplay/Player/FirstPersonTuning.h"
#include "Gameplay/SimulationTiming.h"

#include <algorithm>
#include <cmath>

FirstPersonGame::FirstPersonGame(const GameConfig& config,
    const Vector3& initialEyePosition, const Vector3& targetPosition)
    : player_(std::max(config.maxMovementDeltaTime,
        static_cast<float>(SimulationTiming::kFixedStepSeconds))),
      targetPosition_(targetPosition),
      targetHitPoints_(FirstPersonTuning::kTargetHitPoints),
      maxArmDeltaTime_(std::max(config.maxMovementDeltaTime,
          static_cast<float>(SimulationTiming::kFixedStepSeconds))) {
    player_.Reset({ initialEyePosition.x,
        initialEyePosition.y - FirstPersonTuning::kEyeHeight,
        initialEyePosition.z });
    camera_.Reset(initialEyePosition);
    leftArm_.Reset(player_.GetPosition(), player_.GetBodyYaw());
    rightArm_.Reset(player_.GetPosition(), player_.GetBodyYaw());
}

void FirstPersonGame::Update(double deltaSeconds, const GameInput& input,
    float mouseDeltaX, float mouseDeltaY) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0) {
        return;
    }
    events_ = {};
    camera_.UpdateLook(mouseDeltaX, mouseDeltaY);
    player_.Update(deltaSeconds, input.moveX, -input.moveY, camera_.GetYaw());
    const float armDeltaTime = static_cast<float>(std::min(
        deltaSeconds, static_cast<double>(maxArmDeltaTime_)));
    const Vector3 playerPosition = player_.GetPosition();
    const float bodyYaw = player_.GetBodyYaw();
    const bool leftPunchHeld = input.leftPunchKeyDown ||
        (input.leftMouseDown && (input.shiftDown || leftArm_.IsChargingOrCharged()));
    const bool rightPunchHeld = input.rightPunchKeyDown ||
        (input.rightMouseDown && (input.shiftDown || rightArm_.IsChargingOrCharged()));
    leftArm_.Update(armDeltaTime, playerPosition, bodyYaw, leftPunchHeld);
    rightArm_.Update(armDeltaTime, playerPosition, bodyYaw, rightPunchHeld);
    if (input.resetTarget.pressed) {
        ResetTarget();
        leftArm_.Reset(playerPosition, bodyYaw);
        rightArm_.Reset(playerPosition, bodyYaw);
    } else {
        ProcessTargetHits();
    }
    camera_.Follow({ playerPosition.x,
        playerPosition.y + FirstPersonTuning::kEyeHeight,
        playerPosition.z }, deltaSeconds);
    elapsedSeconds_ += deltaSeconds;
    if (input.changeBackground.pressed) {
        warmBackground_ = !warmBackground_;
    }
}

void FirstPersonGame::ProcessTargetHits() {
    if (targetHitPoints_ <= 0) {
        return;
    }
    const Collision::Sphere targetSphere{targetPosition_, FirstPersonTuning::kTargetRadius};
    const auto hitsTarget = [&](const PunchArm& arm) {
        if (!arm.IsPunchDamageActiveThisFrame()) {
            return false;
        }
        const float fistRadius = FirstPersonTuning::kFistRadius;
        return Collision::Intersects({arm.GetFistPosition(), fistRadius}, targetSphere) ||
            Collision::SweepSphereAgainstSphere(arm.GetPreviousFistPosition(),
                arm.GetFistPosition(), fistRadius, targetSphere).didHit;
    };
    if (!hitsTarget(leftArm_) && !hitsTarget(rightArm_)) {
        return;
    }
    targetHitPoints_ = std::max(0, targetHitPoints_ - FirstPersonTuning::kPunchDamage);
    if (targetHitPoints_ == 0) {
        events_.targetDestroyed = true;
    }
}

void FirstPersonGame::ResetTarget() {
    targetHitPoints_ = FirstPersonTuning::kTargetHitPoints;
    events_.targetReset = true;
}
