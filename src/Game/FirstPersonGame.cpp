#include "Game/FirstPersonGame.h"

#include "Collision/Collision.h"
#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

FirstPersonGame::FirstPersonGame(const GameConfig& config, ModelScene initialScene)
    : scene_(std::move(initialScene)), player_(config.maxMovementDeltaTime),
      maxArmDeltaTime_(config.maxMovementDeltaTime) {
    player_.Reset({ scene_.cameraPosition.x,
        scene_.cameraPosition.y - FirstPersonTuning::kEyeHeight,
        scene_.cameraPosition.z });
    camera_.Reset(scene_.cameraPosition);
    scene_.cameraTarget = camera_.GetTarget();
    leftArm_.Reset(player_.GetPosition(), player_.GetBodyYaw());
    rightArm_.Reset(player_.GetPosition(), player_.GetBodyYaw());
    if (const ModelInstance* target = FindModelById(scene_, "target")) {
        initialTarget_ = *target;
        targetHitPoints_ = FirstPersonTuning::kTargetHitPoints;
    }
    UpdateFistPoses();
}

void FirstPersonGame::Update(double deltaSeconds, const GameInput& input,
    float mouseDeltaX, float mouseDeltaY) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0) {
        return;
    }
    targetDestroyedThisFrame_ = false;
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
    UpdateFistPoses();
    if (input.resetTarget.pressed) {
        ResetTarget();
        leftArm_.Reset(playerPosition, bodyYaw);
        rightArm_.Reset(playerPosition, bodyYaw);
        UpdateFistPoses();
    } else {
        ProcessTargetHits();
    }
    camera_.Follow({ playerPosition.x,
        playerPosition.y + FirstPersonTuning::kEyeHeight,
        playerPosition.z }, deltaSeconds);
    scene_.cameraPosition = camera_.GetPosition();
    scene_.cameraTarget = camera_.GetTarget();
    scene_.elapsedSeconds += deltaSeconds;
    if (input.changeBackground.pressed) {
        scene_.warmBackground = !scene_.warmBackground;
    }
}

void FirstPersonGame::ProcessTargetHits() {
    if (targetHitPoints_ <= 0) {
        return;
    }
    const ModelInstance* target = FindModelById(scene_, "target");
    if (target == nullptr) {
        return;
    }
    const Collision::Sphere targetSphere{target->position, FirstPersonTuning::kTargetRadius};
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
        scene_.models.erase(std::remove_if(scene_.models.begin(), scene_.models.end(),
            [](const ModelInstance& model) { return model.id == "target"; }),
            scene_.models.end());
        targetDestroyedThisFrame_ = true;
    }
}

void FirstPersonGame::ResetTarget() {
    if (!initialTarget_) {
        return;
    }
    if (ModelInstance* target = FindModelById(scene_, "target")) {
        *target = *initialTarget_;
    } else {
        scene_.models.insert(scene_.models.begin(), *initialTarget_);
    }
    targetHitPoints_ = FirstPersonTuning::kTargetHitPoints;
}

void FirstPersonGame::UpdateFistPoses() {
    const Vector3& playerPosition = player_.GetPosition();
    const float bodyYaw = player_.GetBodyYaw();
    const float sine = std::sin(bodyYaw);
    const float cosine = std::cos(bodyYaw);
    const auto updatePose = [&](const char* id, float sideSign, const PunchArm& arm) {
        ModelInstance* fist = FindModelById(scene_, id);
        if (fist == nullptr) {
            return;
        }
        const Vector3& fistPosition = arm.GetFistPosition();
        const float sideOffset = sideSign * FirstPersonTuning::kShoulderHalfWidth;
        const Vector3 shoulder{
            playerPosition.x + cosine * sideOffset,
            playerPosition.y + FirstPersonTuning::kShoulderHeight,
            playerPosition.z - sine * sideOffset
        };
        const float horizontalX = fistPosition.x - shoulder.x;
        const float horizontalZ = fistPosition.z - shoulder.z;
        const float halfHorizontalDistance = 0.5f * std::hypot(horizontalX, horizontalZ);
        const float upperArmLength = FirstPersonTuning::kUpperArmFixedLength;
        const float elbowDrop = std::sqrt(std::max(0.0f,
            upperArmLength * upperArmLength - halfHorizontalDistance * halfHorizontalDistance));
        const Vector3 elbow{
            (shoulder.x + fistPosition.x) * 0.5f,
            shoulder.y - elbowDrop,
            (shoulder.z + fistPosition.z) * 0.5f
        };
        const float forearmX = fistPosition.x - elbow.x;
        const float forearmY = fistPosition.y - elbow.y;
        const float forearmZ = fistPosition.z - elbow.z;
        fist->position = fistPosition;
        fist->initialXDegrees = -std::atan2(forearmY, std::hypot(forearmX, forearmZ)) *
            180.0f / std::numbers::pi_v<float>;
        fist->initialYDegrees = std::atan2(forearmX, forearmZ) *
            180.0f / std::numbers::pi_v<float>;
        fist->initialZDegrees = (arm.GetState() == PunchArmState::Ready ||
            arm.GetState() == PunchArmState::Recovery) ? -sideSign * 90.0f : 0.0f;
    };
    updatePose("left_fist", -1.0f, leftArm_);
    updatePose("right_fist", 1.0f, rightArm_);
}
