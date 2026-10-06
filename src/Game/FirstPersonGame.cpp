#include "Game/FirstPersonGame.h"

#include "Collision/Collision.h"
#include "Gameplay/Player/FirstPersonTuning.h"
#include "Gameplay/Combat/CombatHitResolver.h"
#include "Gameplay/SimulationTiming.h"

#include <algorithm>
#include <cmath>
#include <utility>

FirstPersonGame::FirstPersonGame(const GameConfig& config,
    const Vector3& initialEyePosition, const Vector3& targetPosition,
    PlayerCombatConfig combatConfig)
    : FirstPersonGame(config, initialEyePosition,
        std::vector<FirstPersonTarget>{{"target", targetPosition,
            FirstPersonTuning::kTargetHitPoints}}, combatConfig) {
}

FirstPersonGame::FirstPersonGame(const GameConfig& config,
    const Vector3& initialEyePosition, std::vector<FirstPersonTarget> targets,
    PlayerCombatConfig combatConfig)
    : combatConfig_(combatConfig),
      player_(std::max(config.maxMovementDeltaTime,
          static_cast<float>(SimulationTiming::kFixedStepSeconds)), combatConfig_),
      camera_(combatConfig_),
      leftArm_(PunchArmSide::Left, combatConfig_),
      rightArm_(PunchArmSide::Right, combatConfig_),
      vitality_(combatConfig_),
      initialEyePosition_(initialEyePosition),
      targets_(std::move(targets)),
      maxArmDeltaTime_(std::max(config.maxMovementDeltaTime,
          static_cast<float>(SimulationTiming::kFixedStepSeconds))) {
    for (FirstPersonTarget& target : targets_) {
        target.previousPosition = target.position;
        target.movedSinceLastStep = false;
    }
    initialTargets_ = targets_;
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
    FirstPersonTarget spawned = target;
    spawned.previousPosition = spawned.position;
    spawned.movedSinceLastStep = false;
    targets_.push_back(std::move(spawned));
    leftHitIds_.erase(target.id);
    rightHitIds_.erase(target.id);
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
    if (!target->movedSinceLastStep) {
        target->previousPosition = target->position;
    }
    target->position = position;
    target->movedSinceLastStep = true;
    return true;
}

bool FirstPersonGame::RemoveTarget(std::string_view id) {
    const auto target = std::find_if(targets_.begin(), targets_.end(),
        [&](const FirstPersonTarget& item) { return item.id == id; });
    if (target == targets_.end()) {
        return false;
    }
    const std::string removedId = target->id;
    targets_.erase(target);
    leftHitIds_.erase(removedId);
    rightHitIds_.erase(removedId);
    return true;
}

void FirstPersonGame::Update(double deltaSeconds, const GameInput& input,
    float mouseDeltaX, float mouseDeltaY) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0) {
        return;
    }
    events_ = {};
    camera_.UpdateLook(mouseDeltaX, mouseDeltaY);
    const float armDeltaTime = static_cast<float>(std::min(
        deltaSeconds, static_cast<double>(maxArmDeltaTime_)));
    vitality_.Update(armDeltaTime);
    player_.Update(deltaSeconds, input.moveX, -input.moveY, camera_.GetYaw());
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
    const bool leftWasActive = leftArm_.IsPunchDamageActiveThisFrame() ||
        leftArm_.IsRocketDamageActiveThisFrame();
    const bool rightWasActive = rightArm_.IsPunchDamageActiveThisFrame() ||
        rightArm_.IsRocketDamageActiveThisFrame();
    const bool allowNewCharge = player_.GetImpactState() == PlayerImpactState::None;
    leftArm_.Update(armDeltaTime, playerPosition, bodyYaw,
        armInput(leftArm_, input.leftPunchKeyDown, input.leftMouseDown), allowNewCharge);
    rightArm_.Update(armDeltaTime, playerPosition, bodyYaw,
        armInput(rightArm_, input.rightPunchKeyDown, input.rightMouseDown), allowNewCharge);
    if (!leftWasActive && (leftArm_.IsPunchDamageActiveThisFrame() ||
            leftArm_.IsRocketDamageActiveThisFrame())) {
        leftHitIds_.clear();
    }
    if (!rightWasActive && (rightArm_.IsPunchDamageActiveThisFrame() ||
            rightArm_.IsRocketDamageActiveThisFrame())) {
        rightHitIds_.clear();
    }
    if (input.resetTarget.pressed) {
        ResetTarget();
        player_.Reset({initialEyePosition_.x,
            initialEyePosition_.y - FirstPersonTuning::kEyeHeight,
            initialEyePosition_.z});
        camera_.Reset(initialEyePosition_);
        vitality_.Reset();
        leftArm_.Reset(player_.GetPosition(), player_.GetBodyYaw());
        rightArm_.Reset(player_.GetPosition(), player_.GetBodyYaw());
        leftHitIds_.clear();
        rightHitIds_.clear();
    } else {
        ProcessTargetHits();
        if (input.testProjectileHit.pressed) {
            ReceivePlayerHit(PlayerHitType::Projectile);
        } else if (input.testChargeHit.pressed) {
            ReceivePlayerHit(PlayerHitType::ChargeUnit);
        } else if (input.testBurstHit.pressed) {
            ReceivePlayerHit(PlayerHitType::BossSpreadBurst);
        }
    }
    events_.leftPunchBegan = leftArm_.DidBeginPunchThisFrame();
    events_.rightPunchBegan = rightArm_.DidBeginPunchThisFrame();
    events_.leftRocketLaunched = leftArm_.DidLaunchRocketThisFrame();
    events_.rightRocketLaunched = rightArm_.DidLaunchRocketThisFrame();
    events_.leftRocketReconnected = leftArm_.DidReconnectThisFrame();
    events_.rightRocketReconnected = rightArm_.DidReconnectThisFrame();
    if (events_.punchHit) {
        camera_.AddShake(combatConfig_.punchShakeStrength,
            combatConfig_.punchShakeDuration);
    }
    if (events_.leftRocketLaunched || events_.rightRocketLaunched) {
        camera_.AddShake(combatConfig_.rocketLaunchShakeStrength,
            combatConfig_.rocketLaunchShakeDuration);
    }
    const Vector3 finalPlayerPosition = player_.GetPosition();
    const Vector3& velocity = player_.GetVelocity();
    camera_.Follow({finalPlayerPosition.x,
        finalPlayerPosition.y + FirstPersonTuning::kEyeHeight,
        finalPlayerPosition.z}, deltaSeconds,
        std::hypot(velocity.x, velocity.z));
    elapsedSeconds_ += deltaSeconds;
    if (input.changeBackground.pressed) {
        warmBackground_ = !warmBackground_;
    }
}

void FirstPersonGame::ProcessTargetHits() {
    std::vector<CombatHitTarget> candidates;
    candidates.reserve(targets_.size());
    for (const FirstPersonTarget& target : targets_) {
        if (target.hitPoints > 0) {
            candidates.push_back({target.id, target.previousPosition, target.position,
                FirstPersonTuning::kTargetRadius});
        }
    }
    const auto applyHits = [&](const PunchArm& arm, PunchArmSide side,
        std::unordered_set<std::string>& registered) {
        for (const CombatHitEvent& hit : FindArmHits(arm, side, candidates, registered)) {
            auto target = std::find_if(targets_.begin(), targets_.end(),
                [&](const FirstPersonTarget& item) { return item.id == hit.targetId; });
            if (target == targets_.end() || target->hitPoints <= 0) {
                continue;
            }
            registered.insert(hit.targetId);
            target->hitPoints = std::max(0, target->hitPoints - FirstPersonTuning::kPunchDamage);
            events_.targetDestroyed |= target->hitPoints == 0;
            events_.punchHit |= hit.attackType == PunchArmAttackType::Punch;
            events_.rocketHit |= hit.attackType == PunchArmAttackType::RocketPunch;
        }
    };
    applyHits(leftArm_, PunchArmSide::Left, leftHitIds_);
    applyHits(rightArm_, PunchArmSide::Right, rightHitIds_);
    for (FirstPersonTarget& target : targets_) {
        target.previousPosition = target.position;
        target.movedSinceLastStep = false;
    }
}

PlayerHitResult FirstPersonGame::ReceivePlayerHit(PlayerHitType type,
    const Vector3& pushDirection) {
    const PlayerHitResult result = vitality_.ReceiveHit(type);
    events_.playerHit = result.wasApplied;
    events_.playerHitType = type;
    events_.playerHitResult = result;
    events_.playerDefeated |= vitality_.IsDefeated();
    if (type == PlayerHitType::ChargeUnit) {
        player_.StartChargeImpact(pushDirection, result.shieldLayersLost > 0);
        camera_.AddShake(combatConfig_.chargeHitShakeStrength,
            combatConfig_.chargeHitShakeDuration);
    } else if (type == PlayerHitType::BossSpreadBurst) {
        player_.StartBurstImpact(pushDirection);
        camera_.AddShake(combatConfig_.chargeHitShakeStrength,
            combatConfig_.chargeHitShakeDuration);
    } else if (type == PlayerHitType::Projectile) {
        camera_.AddShake(result.didDamageHitPoints
                ? combatConfig_.projectileHitPointShakeStrength
                : combatConfig_.projectileShieldShakeStrength,
            result.didDamageHitPoints
                ? combatConfig_.projectileHitPointShakeDuration
                : combatConfig_.projectileShieldShakeDuration);
    }
    return result;
}

void FirstPersonGame::ResetTarget() {
    targets_ = initialTargets_;
    events_.targetReset = true;
}
