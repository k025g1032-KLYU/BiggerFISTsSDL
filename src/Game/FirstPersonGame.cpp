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
        target.hitPoints <= 0 || !std::isfinite(target.radius) ||
        target.radius <= 0.0f || !std::isfinite(target.punchRadiusMultiplier) ||
        target.punchRadiusMultiplier <= 0.0f ||
        !std::isfinite(target.position.x) ||
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
    float mouseDeltaX, float mouseDeltaY, bool deferTargetHits) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0) {
        return;
    }
    events_ = {};
    camera_.UpdateLook(mouseDeltaX, mouseDeltaY);
    const float armDeltaTime = static_cast<float>(std::min(
        deltaSeconds, static_cast<double>(maxArmDeltaTime_)));
    vitality_.Update(armDeltaTime);
    if (input.dodge.pressed &&
        (ultimateState_ == UltimateState::Idle ||
            ultimateState_ == UltimateState::Forming)) {
        events_.dodgeStarted = player_.TryStartDodge(input.moveX,
            -input.moveY);
    }
    const bool systemOffline = ultimateState_ == UltimateState::Flying ||
        ultimateState_ == UltimateState::Returning ||
        ultimateState_ == UltimateState::Offline;
    player_.Update(deltaSeconds, systemOffline ? 0.0f : input.moveX,
        systemOffline ? 0.0f : -input.moveY, camera_.GetYaw());
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
    const PunchArmInput leftInput = armInput(leftArm_, input.leftPunchKeyDown,
        input.leftMouseDown);
    const PunchArmInput rightInput = armInput(rightArm_, input.rightPunchKeyDown,
        input.rightMouseDown);
    bool lockArms = false;
    UpdateUltimate(armDeltaTime, input, leftInput, rightInput, lockArms);
    const bool allowNewCharge = player_.GetImpactState() == PlayerImpactState::None &&
        player_.GetDodgeState() != PlayerDodgeState::Boosting && !lockArms;
    leftArm_.Update(armDeltaTime, playerPosition, bodyYaw,
        lockArms ? PunchArmInput{false, true} : leftInput, allowNewCharge);
    rightArm_.Update(armDeltaTime, playerPosition, bodyYaw,
        lockArms ? PunchArmInput{false, true} : rightInput, allowNewCharge);
    if (ultimateEnabled_ && ultimateState_ == UltimateState::Idle &&
        leftInput.rocketHeld && rightInput.rocketHeld &&
        leftArm_.GetState() == PunchArmState::Charged &&
        rightArm_.GetState() == PunchArmState::Charged &&
        leftArm_.GetAttackType() == PunchArmAttackType::RocketPunch &&
        rightArm_.GetAttackType() == PunchArmAttackType::RocketPunch) {
        ultimateState_ = UltimateState::Forming;
        ultimateChargeTimer_ = 0.0f;
    }
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
        ResetCombatState();
    } else {
        if (!deferTargetHits) {
            ProcessTargetHits();
        }
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

void FirstPersonGame::ResetCombatState() {
    player_.Reset({initialEyePosition_.x,
        initialEyePosition_.y - FirstPersonTuning::kEyeHeight,
        initialEyePosition_.z});
    camera_.Reset(initialEyePosition_);
    vitality_.Reset();
    leftArm_.Reset(player_.GetPosition(), player_.GetBodyYaw());
    rightArm_.Reset(player_.GetPosition(), player_.GetBodyYaw());
    ultimateState_ = UltimateState::Idle;
    ultimateChargeTimer_ = 0.0f;
    ultimateTravel_ = 0.0f;
    ultimateReturnTimer_ = 0.0f;
    ultimateOfflineTimer_ = 0.0f;
    ultimateDamageActiveThisFrame_ = false;
    leftHitIds_.clear();
    rightHitIds_.clear();
    events_ = {};
}

void FirstPersonGame::UpdateLookOnly(float mouseDeltaX, float mouseDeltaY) {
    events_ = {};
    camera_.UpdateLook(mouseDeltaX, mouseDeltaY);
}

void FirstPersonGame::ResolveTargetHits() {
    ProcessTargetHits();
    if (events_.punchHit) {
        camera_.AddShake(combatConfig_.punchShakeStrength,
            combatConfig_.punchShakeDuration);
    }
}

void FirstPersonGame::UpdateUltimate(float seconds, const GameInput&,
    const PunchArmInput& leftInput, const PunchArmInput& rightInput,
    bool& lockArms) {
    ultimateDamageActiveThisFrame_ = false;
    previousUltimatePosition_ = ultimatePosition_;
    if (!ultimateEnabled_) {
        return;
    }
    const bool bothHeld = leftInput.rocketHeld && rightInput.rocketHeld;
    if (ultimateState_ == UltimateState::Forming ||
        ultimateState_ == UltimateState::Ready) {
        if (ultimateState_ == UltimateState::Forming && !bothHeld) {
            ultimateState_ = UltimateState::Idle;
            ultimateChargeTimer_ = 0.0f;
            return;
        }
        lockArms = true;
        if (ultimateState_ == UltimateState::Forming) {
            ultimateChargeTimer_ += seconds;
            if (ultimateChargeTimer_ >= combatConfig_.ultimateChargeTime) {
                ultimateState_ = UltimateState::Ready;
            }
        }
        if (ultimateState_ == UltimateState::Ready &&
            !leftInput.rocketHeld && !rightInput.rocketHeld) {
            const Vector3& position = player_.GetPosition();
            const float yaw = player_.GetBodyYaw();
            ultimateDirection_ = {std::sin(yaw), 0.0f, std::cos(yaw)};
            ultimatePosition_ = {position.x + ultimateDirection_.x * 4.0f,
                position.y + FirstPersonTuning::kShoulderHeight,
                position.z + ultimateDirection_.z * 4.0f};
            previousUltimatePosition_ = ultimatePosition_;
            ultimateTravel_ = 0.0f;
            ultimateOfflineTimer_ = combatConfig_.ultimateSystemOfflineDuration;
            ultimateState_ = UltimateState::Flying;
            player_.StartUltimateRecoil();
            events_.ultimateLaunched = true;
        }
        return;
    }
    if (ultimateState_ == UltimateState::Idle) {
        return;
    }
    lockArms = true;
    ultimateOfflineTimer_ = std::max(0.0f, ultimateOfflineTimer_ - seconds);
    if (ultimateState_ == UltimateState::Flying) {
        const float distance = std::min(combatConfig_.ultimateSpeed * seconds,
            combatConfig_.ultimateMaximumRange - ultimateTravel_);
        ultimatePosition_.x += ultimateDirection_.x * distance;
        ultimatePosition_.z += ultimateDirection_.z * distance;
        ultimateTravel_ += distance;
        ultimateDamageActiveThisFrame_ = true;
        if (ultimateTravel_ >= combatConfig_.ultimateMaximumRange) {
            ultimateState_ = UltimateState::Returning;
            ultimateSplitPosition_ = ultimatePosition_;
            ultimateReturnTimer_ = 0.0f;
        }
    } else if (ultimateState_ == UltimateState::Returning) {
        ultimateReturnTimer_ += seconds;
        if (ultimateReturnTimer_ >= combatConfig_.ultimateReturnDuration) {
            ultimateState_ = UltimateState::Offline;
            events_.ultimateReturned = true;
        }
    } else if (ultimateOfflineTimer_ <= 0.0f) {
        ultimateState_ = UltimateState::Idle;
        events_.systemRestarted = true;
        ultimateChargeTimer_ = 0.0f;
        leftArm_.Reset(player_.GetPosition(), player_.GetBodyYaw());
        rightArm_.Reset(player_.GetPosition(), player_.GetBodyYaw());
        leftHitIds_.clear();
        rightHitIds_.clear();
        lockArms = false;
    }
}

Vector3 FirstPersonGame::GetUltimateReturnFistPosition(PunchArmSide side) const {
    const float sideSign = side == PunchArmSide::Left ? -1.0f : 1.0f;
    const float yaw = player_.GetBodyYaw();
    const Vector3& playerPosition = player_.GetPosition();
    const Vector3 home{playerPosition.x + std::cos(yaw) * sideSign *
            FirstPersonTuning::kShoulderHalfWidth + std::sin(yaw) *
            FirstPersonTuning::kReadyFistForwardOffset,
        playerPosition.y + FirstPersonTuning::kShoulderHeight,
        playerPosition.z - std::sin(yaw) * sideSign *
            FirstPersonTuning::kShoulderHalfWidth + std::cos(yaw) *
            FirstPersonTuning::kReadyFistForwardOffset};
    const Vector3 start{ultimateSplitPosition_.x + std::cos(yaw) * sideSign * 1.8f,
        ultimateSplitPosition_.y, ultimateSplitPosition_.z -
            std::sin(yaw) * sideSign * 1.8f};
    const float ratio = std::clamp(ultimateReturnTimer_ /
        combatConfig_.ultimateReturnDuration, 0.0f, 1.0f);
    return {start.x + (home.x - start.x) * ratio,
        start.y + (home.y - start.y) * ratio,
        start.z + (home.z - start.z) * ratio};
}

void FirstPersonGame::ProcessTargetHits() {
    std::vector<CombatHitTarget> candidates;
    candidates.reserve(targets_.size());
    for (const FirstPersonTarget& target : targets_) {
        if (target.hitPoints > 0) {
            candidates.push_back({target.id, target.previousPosition, target.position,
                target.radius, target.punchRadiusMultiplier});
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
            events_.targetHits.push_back(hit);
            target->hitPoints = std::max(0, target->hitPoints - FirstPersonTuning::kPunchDamage);
            events_.targetDestroyed |= target->hitPoints == 0;
            if (target->hitPoints == 0) {
                events_.destroyedTargetIds.push_back(target->id);
            }
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
