#include "Game/FirstPersonGame.h"

#include "Collision/Collision.h"
#include "Gameplay/Player/FirstPersonTuning.h"
#include "Gameplay/SimulationTiming.h"

#include <algorithm>
#include <cmath>
#include <utility>

FirstPersonGame::FirstPersonGame(const GameConfig& config,
    const Vector3& initialEyePosition, const Vector3& targetPosition)
    : FirstPersonGame(config, initialEyePosition,
        std::vector<FirstPersonTarget>{{"target", targetPosition,
            FirstPersonTuning::kTargetHitPoints}}) {
}

FirstPersonGame::FirstPersonGame(const GameConfig& config,
    const Vector3& initialEyePosition, std::vector<FirstPersonTarget> targets)
    : player_(std::max(config.maxMovementDeltaTime,
        static_cast<float>(SimulationTiming::kFixedStepSeconds))),
      targets_(std::move(targets)),
      initialTargets_(targets_),
      maxArmDeltaTime_(std::max(config.maxMovementDeltaTime,
          static_cast<float>(SimulationTiming::kFixedStepSeconds))) {
    player_.Reset({ initialEyePosition.x,
        initialEyePosition.y - FirstPersonTuning::kEyeHeight,
        initialEyePosition.z });
    camera_.Reset(initialEyePosition);
    leftArm_.Reset(player_.GetPosition(), player_.GetBodyYaw());
    rightArm_.Reset(player_.GetPosition(), player_.GetBodyYaw());
}

bool FirstPersonGame::SpawnTarget(const FirstPersonTarget& target) {
    if (!target.id.starts_with("target_") || target.id.size() == 7 ||
        target.hitPoints <= 0 || !std::isfinite(target.position.x) ||
        !std::isfinite(target.position.y) || !std::isfinite(target.position.z) ||
        std::find_if(targets_.begin(), targets_.end(), [&](const FirstPersonTarget& existing) {
            return existing.id == target.id;
        }) != targets_.end()) {
        return false;
    }
    targets_.push_back(target);
    return true;
}

bool FirstPersonGame::MoveTarget(std::string_view id, const Vector3& position) {
    if (!std::isfinite(position.x) || !std::isfinite(position.y) ||
        !std::isfinite(position.z)) {
        return false;
    }
    const auto target = std::find_if(targets_.begin(), targets_.end(),
        [&](const FirstPersonTarget& item) { return item.id == id; });
    if (target == targets_.end() || target->hitPoints <= 0) {
        return false;
    }
    target->position = position;
    return true;
}

bool FirstPersonGame::RemoveTarget(std::string_view id) {
    const auto target = std::find_if(targets_.begin(), targets_.end(),
        [&](const FirstPersonTarget& item) { return item.id == id; });
    if (target == targets_.end()) {
        return false;
    }
    targets_.erase(target);
    return true;
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
    const auto armInput = [&](const PunchArm& arm, bool keyPunchHeld,
        bool mouseHeld) {
        PunchArmInput result{};
        result.punchHeld = keyPunchHeld;
        if (mouseHeld) {
            const bool continuePunch = arm.IsChargingOrCharged()
                ? arm.GetAttackType() == PunchArmAttackType::Punch : input.shiftDown;
            if (continuePunch) {
                result.punchHeld = true;
            } else {
                result.rocketHeld = true;
            }
        }
        return result;
    };
    leftArm_.Update(armDeltaTime, playerPosition, bodyYaw,
        armInput(leftArm_, input.leftPunchKeyDown, input.leftMouseDown));
    rightArm_.Update(armDeltaTime, playerPosition, bodyYaw,
        armInput(rightArm_, input.rightPunchKeyDown, input.rightMouseDown));
    if (input.resetTarget.pressed) {
        ResetTarget();
        leftArm_.Reset(playerPosition, bodyYaw);
        rightArm_.Reset(playerPosition, bodyYaw);
    } else {
        ProcessTargetHits();
    }
    events_.leftRocketLaunched = leftArm_.DidLaunchRocketThisFrame();
    events_.rightRocketLaunched = rightArm_.DidLaunchRocketThisFrame();
    events_.leftRocketReconnected = leftArm_.DidReconnectThisFrame();
    events_.rightRocketReconnected = rightArm_.DidReconnectThisFrame();
    if (events_.punchHit) {
        camera_.AddShake(FirstPersonTuning::kPunchShakeStrength,
            FirstPersonTuning::kPunchShakeDuration);
    }
    if (events_.leftRocketLaunched || events_.rightRocketLaunched) {
        camera_.AddShake(FirstPersonTuning::kRocketLaunchShakeStrength,
            FirstPersonTuning::kRocketLaunchShakeDuration);
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
    const auto hitsTarget = [&](const PunchArm& arm,
        const Collision::Sphere& targetSphere) {
        const float fistRadius = FirstPersonTuning::kFistRadius;
        if (arm.IsPunchDamageActiveThisFrame()) {
            return Collision::Intersects({arm.GetFistPosition(), fistRadius}, targetSphere);
        }
        return arm.IsRocketDamageActiveThisFrame() &&
            Collision::SweepSphereAgainstSphere(arm.GetPreviousFistPosition(),
                arm.GetFistPosition(), fistRadius, targetSphere).didHit;
    };
    for (FirstPersonTarget& target : targets_) {
        if (target.hitPoints <= 0) {
            continue;
        }
        const Collision::Sphere targetSphere{
            target.position, FirstPersonTuning::kTargetRadius};
        const PunchArm* hittingArm = nullptr;
        if (hitsTarget(leftArm_, targetSphere)) {
            hittingArm = &leftArm_;
        } else if (hitsTarget(rightArm_, targetSphere)) {
            hittingArm = &rightArm_;
        }
        if (hittingArm != nullptr) {
            target.hitPoints = std::max(0,
                target.hitPoints - FirstPersonTuning::kPunchDamage);
            events_.targetDestroyed |= target.hitPoints == 0;
            events_.punchHit |= hittingArm->GetAttackType() == PunchArmAttackType::Punch;
            events_.rocketHit |= hittingArm->GetAttackType() == PunchArmAttackType::RocketPunch;
        }
    }
}

void FirstPersonGame::ResetTarget() {
    targets_ = initialTargets_;
    events_.targetReset = true;
}
