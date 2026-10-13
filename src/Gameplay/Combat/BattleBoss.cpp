#include "Gameplay/Combat/BattleBoss.h"

#include "Collision/Collision.h"
#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
Vector3 DirectionTo(const Vector3& from, const Vector3& to) {
    const float x = to.x - from.x;
    const float z = to.z - from.z;
    const float length = std::hypot(x, z);
    return length > 0.00001f ? Vector3{x / length, 0.0f, z / length}
        : Vector3{0.0f, 0.0f, -1.0f};
}

float BlendAngle(float current, float target, float amount) {
    return current + std::remainder(target - current,
        2.0f * std::numbers::pi_v<float>) * amount;
}

Vector3 BlendPosition(const Vector3& current, const Vector3& target,
    float amount) {
    return {current.x + (target.x - current.x) * amount,
        current.y + (target.y - current.y) * amount,
        current.z + (target.z - current.z) * amount};
}

Vector3 TipToward(const Vector3& root, const Vector3& target,
    const Vector3& fallbackDirection, float offset) {
    const Vector3 delta{target.x - root.x, target.y - root.y,
        target.z - root.z};
    const float length = std::hypot(delta.x, delta.y, delta.z);
    const Vector3 direction = length > 0.00001f
        ? Vector3{delta.x / length, delta.y / length, delta.z / length}
        : fallbackDirection;
    return {root.x + direction.x * offset,
        root.y + direction.y * offset,
        root.z + direction.z * offset};
}

Vector3 BlendRotation(const Vector3& current, const Vector3& target,
    float amount) {
    return {BlendAngle(current.x, target.x, amount),
        BlendAngle(current.y, target.y, amount),
        BlendAngle(current.z, target.z, amount)};
}

bool MoveTowards(Vector3& position, const Vector3& target, float distance) {
    const Vector3 delta{target.x - position.x, target.y - position.y,
        target.z - position.z};
    const float remaining = std::sqrt(delta.x * delta.x + delta.y * delta.y +
        delta.z * delta.z);
    if (remaining <= distance || remaining <= 0.00001f) {
        position = target;
        return true;
    }
    position.x += delta.x * distance / remaining;
    position.y += delta.y * distance / remaining;
    position.z += delta.z * distance / remaining;
    return false;
}
}

BattleBoss::BattleBoss(BossCombatConfig config) : config_(config) {
    Reset({0.0f, config_.entranceSpawnHeight, 34.5f});
}

void BattleBoss::Reset(const Vector3& position) {
    position_ = position;
    previousPosition_ = position;
    mode_ = BossMode::Entrance;
    needlePose_ = BossNeedlePose::ChargeForm;
    modeTime_ = 0.0f;
    barragePhase_ = BarragePhase::TurningUp;
    barragePhaseTime_ = 0.0f;
    barragePitch_ = 0.0f;
    barrageShotTimer_ = 0.0f;
    entranceRecovering_ = false;
    entranceRecoveryTime_ = 0.0f;
    attackTimer_ = 0.0f;
    hitPoints_ = config_.coreHitPoints;
    coreHitFlashRemaining_ = 0.0f;
    behaviorRandomState_ = 0x6d2b79f5u;
    lastIdleAction_ = BossMode::Entrance;
    boxerCooldown_ = 0.0f;
    chargeCooldown_ = 0.0f;
    chargePhase_ = ChargePhase::Preparing;
    chargePhaseTime_ = 0.0f;
    chargeSequenceCount_ = 0;
    chargeTipHit_ = false;
    chargeBodyContact_ = false;
    chargeTipAttackId_ = 0;
    chargeBodyAttackId_ = 0;
    cruiseCooldown_ = 0.0f;
    cruisePhase_ = CruisePhase::TakingOff;
    cruiseTarget_ = position;
    cruiseFlightTime_ = 0.0f;
    lastCruiseCandidate_ = -1;
    cruiseTargetCandidate_ = -1;
    lastPlayerQuadrant_ = -1;
    cruiseRetargetCount_ = 0;
    nextAttackId_ = 1;
    activeAttackId_ = 0;
    strikeTime_ = 0.0f;
    boxerPhase_ = BoxerPhase::Approaching;
    boxerPhaseTime_ = 0.0f;
    boxerNeedleIndex_ = -1;
    boxerVisualStart_ = {};
    boxerVisualPullback_ = {};
    boxerVisualTarget_ = {};
    boxerVisualDirection_ = {};
    boxerEntryDashDistance_ = 0.0f;
    boxerClosingStepCooldown_ = 0.0f;
    usedBoxerEntryDash_ = false;
    boxerCombatStarted_ = false;
    boxerEntryDirection_ = {};
    needleDashComplete_ = false;
    needleVisualInitialized_ = false;
    needleVisualDirection_ = {};
    needleVisualTarget_ = {};
    needleVisualStart_.fill({});
    needleVisualPullback_.fill({});
    needleVisualCurrent_.fill({});
    needleFirstHit_ = false;
    needleFirstAttackId_ = 0;
    needleImpactTime_ = 0.0f;
    needleImpactAttackId_ = 0;
    nextSpikeSlot_ = 0;
    nextCubeSlot_ = 0;
    spikeShotSequence_ = 0;
    cubeShotSequence_ = 0;
    crystalAngle_ = 0.0f;
    spikeSlots_.fill({});
    cubeSlots_.fill({});
    attackCount_ = 0;
    attackRegistered_ = false;
    needleComboPending_ = false;
    needleComboUsed_ = false;
    fullBarragePending_ = false;
    fullBarrageUsed_ = false;
    fullBarrageDamageLocked_ = false;
    idleStrafeTimer_ = 0.0f;
    idleStrafeSide_ = 1.0f;
    spreadPressureTimer_ = 0.0f;
    spreadCooldown_ = 0.0f;
    spreadPressureArmed_ = true;
    crystalSpikeTimer_ = config_.crystalSpikeInitialDelay;
    crystalCubeTimer_ = config_.crystalCubeInitialDelay;
    chargedRocketAimTimers_.fill(0.0f);
    evaluatedChargedRocketAim_.fill(false);
    rocketDodgeTimer_ = 0.0f;
    rocketDodgeCooldown_ = 0.0f;
    boxerRocketDodgeCooldown_ = 0.0f;
    rocketDodgeSpeed_ = 0.0f;
    beganRocketDodgeThisStep_ = false;
    beganCrystalLaunchThisStep_ = false;
    beganChargeThisStep_ = false;
    beganStrikeThisStep_ = false;
    lastPlayerPosition_ = {};
    defeatVelocity_ = {};
    defeatRotation_ = {};
    defeatStartPitch_ = 0.0f;
    defeatSmallExplosionPosition_ = {};
    defeatPhase_ = DefeatPhase::Airborne;
    defeatPhaseTime_ = 0.0f;
    defeatSmallExplosionTime_ = 0.0f;
    defeatSmallExplosionCount_ = 0;
    emittedDefeatSmallExplosionThisStep_ = false;
    visualYaw_ = 0.0f;
    const BossVisualPose initialPose = CalculateTargetVisualPose();
    visualCones_ = initialPose.cones;
    spreadStartCones_ = visualCones_;
    visualCoreRotation_ = initialPose.coreRotation;
    previousVisualPose_ = GetVisualPose();
}

BossVisualPose BattleBoss::GetVisualPose() const {
    BossVisualPose pose = CalculateTargetVisualPose();
    pose.cones = visualCones_;
    pose.coreRotation = visualCoreRotation_;
    return pose;
}

BossVisualPose BattleBoss::GetInterpolatedVisualPose(float alpha) const {
    const BossVisualPose current = GetVisualPose();
    const float amount = std::clamp(alpha, 0.0f, 1.0f);
    BossVisualPose result = current;
    result.rootRotation = BlendRotation(previousVisualPose_.rootRotation,
        current.rootRotation, amount);
    result.coreRotation = BlendRotation(previousVisualPose_.coreRotation,
        current.coreRotation, amount);
    result.crystalAngle = BlendAngle(previousVisualPose_.crystalAngle,
        current.crystalAngle, amount);
    result.spreadSpin = BlendAngle(previousVisualPose_.spreadSpin,
        current.spreadSpin, amount);
    const auto blendPart = [amount](const BossPartPose& previous,
        const BossPartPose& next) {
        return BossPartPose{BlendPosition(previous.translation,
                next.translation, amount),
            BlendRotation(previous.rotation, next.rotation, amount),
            BlendPosition(previous.rootTranslation,
                next.rootTranslation, amount)};
    };
    for (std::size_t index = 0; index < result.cones.size(); ++index) {
        result.cones[index] = blendPart(previousVisualPose_.cones[index],
            current.cones[index]);
    }
    for (std::size_t index = 0; index < result.spikes.size(); ++index) {
        result.spikes[index] = blendPart(previousVisualPose_.spikes[index],
            current.spikes[index]);
    }
    for (std::size_t index = 0; index < result.cubes.size(); ++index) {
        result.cubes[index] = blendPart(previousVisualPose_.cubes[index],
            current.cubes[index]);
    }
    return result;
}

BossVisualPose BattleBoss::CalculateTargetVisualPose() const {
    constexpr float pi = std::numbers::pi_v<float>;
    constexpr std::array<BossPartPose, 3> cruise{{
        {{1.0f, 0.0f, 0.0f}, {0.30f, pi - 0.70f, 0.10f}},
        {{-1.0f, 0.0f, 0.0f}, {0.30f, pi + 0.70f, -0.10f}},
        {{0.0f, 0.0f, 0.0f}, {-0.72f, pi, 0.0f}}
    }};
    constexpr std::array<BossPartPose, 3> spread{{
        {{}, {0.30f, -0.70f, 0.10f}},
        {{}, {0.30f, 0.70f, -0.10f}},
        {{}, {-0.72f, 0.0f, 0.0f}}
    }};
    constexpr std::array<BossPartPose, 3> needle{{
        {{2.10f, 1.20f, 2.50f}, {0.0f, pi / 18.0f, 0.0f}},
        {{-2.10f, 1.20f, 2.50f}, {0.0f, -pi / 18.0f, 0.0f}},
        {{0.0f, -2.10f, 2.50f}, {pi / 18.0f, 0.0f, 0.0f}}
    }};
    BossVisualPose pose;
    pose.rootRotation.y = visualYaw_;
    switch (needlePose_) {
    case BossNeedlePose::Cruise: pose.cones = cruise; break;
    case BossNeedlePose::Spread: pose.cones = spread; break;
    case BossNeedlePose::NeedleStrike: pose.cones = needle; break;
    case BossNeedlePose::ChargeForm:
        pose.cones = {};
        pose.coreRotation.x = pi * 0.5f;
        break;
    }
    pose.crystalAngle = crystalAngle_;
    if (mode_ == BossMode::Entrance) {
        const float recoveryRatio = entranceRecovering_
            ? std::clamp(entranceRecoveryTime_ /
                config_.entranceFlipDuration, 0.0f, 1.0f)
            : 0.0f;
        pose.rootRotation.x = -pi * 0.5f * (1.0f - recoveryRatio);
    } else if (mode_ == BossMode::Boxer && boxerNeedleIndex_ >= 0 &&
        (boxerPhase_ == BoxerPhase::WindingUp ||
            boxerPhase_ == BoxerPhase::Holding ||
            boxerPhase_ == BoxerPhase::Striking ||
            boxerPhase_ == BoxerPhase::Recovering)) {
        const std::size_t index = static_cast<std::size_t>(boxerNeedleIndex_);
        Vector3 worldPosition = boxerVisualStart_;
        if (boxerPhase_ == BoxerPhase::WindingUp) {
            worldPosition = BlendPosition(boxerVisualStart_,
                boxerVisualPullback_, std::clamp(boxerPhaseTime_ /
                    config_.boxerWindupDuration, 0.0f, 1.0f));
        } else if (boxerPhase_ == BoxerPhase::Holding) {
            worldPosition = boxerVisualPullback_;
        } else if (boxerPhase_ == BoxerPhase::Striking) {
            worldPosition = BlendPosition(boxerVisualPullback_,
                boxerVisualTarget_, std::clamp(boxerPhaseTime_ /
                    config_.boxerStrikeDuration, 0.0f, 1.0f));
        } else {
            const Vector3& base = needle[index].translation;
            const float sine = std::sin(visualYaw_);
            const float cosine = std::cos(visualYaw_);
            const Vector3 returnPosition{
                position_.x + base.x * cosine + base.z * sine,
                position_.y + base.y,
                position_.z - base.x * sine + base.z * cosine};
            worldPosition = BlendPosition(boxerVisualTarget_,
                returnPosition, std::clamp(boxerPhaseTime_ /
                    config_.boxerRecoveryDuration, 0.0f, 1.0f));
        }
        const float sine = std::sin(visualYaw_);
        const float cosine = std::cos(visualYaw_);
        const float offsetX = worldPosition.x - position_.x;
        const float offsetZ = worldPosition.z - position_.z;
        pose.cones[index].rootTranslation = {
            offsetX * cosine - offsetZ * sine,
            worldPosition.y - position_.y,
            offsetX * sine + offsetZ * cosine};
        pose.cones[index].translation = {};
    } else if (mode_ == BossMode::NeedleCombo &&
        needleVisualInitialized_ && needleImpactAttackId_ == 0) {
        const float sine = std::sin(visualYaw_);
        const float cosine = std::cos(visualYaw_);
        for (std::size_t index = 0; index < pose.cones.size(); ++index) {
            const Vector3& world = needleVisualCurrent_[index];
            const float offsetX = world.x - position_.x;
            const float offsetZ = world.z - position_.z;
            pose.cones[index].rootTranslation = {
                offsetX * cosine - offsetZ * sine,
                world.y - position_.y,
                offsetX * sine + offsetZ * cosine};
            pose.cones[index].translation = {};
        }
    } else if (mode_ == BossMode::SpreadCounter) {
        const float spinTime = modeTime_ - config_.spreadWindupDuration;
        if (spinTime > 0.0f) {
            pose.spreadSpin = std::clamp(spinTime /
                config_.spreadSpinDuration, 0.0f, 1.0f) *
                4.0f * pi;
        }
    } else if (mode_ == BossMode::FullBarrage) {
        pose.rootRotation.x = barragePitch_;
    } else if (mode_ == BossMode::Defeating || mode_ == BossMode::Defeated) {
        pose.rootRotation.x = defeatStartPitch_ + defeatRotation_.x;
        pose.rootRotation.y += defeatRotation_.y;
        pose.rootRotation.z = defeatRotation_.z;
    }
    for (std::size_t index = 0; index < pose.spikes.size(); ++index) {
        const float angle = crystalAngle_ + 2.0f * pi *
            static_cast<float>(index) / static_cast<float>(pose.spikes.size());
        const float progress = std::clamp(spikeSlots_[index].launchTime /
            std::max(0.001f, config_.crystalLaunchDuration), 0.0f, 1.0f);
        pose.spikes[index] = {{
            std::cos(angle) * config_.crystalHaloRadius,
            std::sin(angle) * config_.crystalHaloRadius +
                config_.crystalLaunchHeight * progress,
            config_.crystalHaloBackOffset},
            {0.0f, 0.0f, angle - pi * 0.5f}};
    }
    for (std::size_t index = 0; index < pose.cubes.size(); ++index) {
        const float angle = crystalAngle_ + pi /
            static_cast<float>(pose.cubes.size()) + 2.0f * pi *
            static_cast<float>(index) / static_cast<float>(pose.cubes.size());
        const float progress = std::clamp(cubeSlots_[index].launchTime /
            std::max(0.001f, config_.crystalLaunchDuration), 0.0f, 1.0f);
        pose.cubes[index] = {{
            std::cos(angle) * config_.crystalHaloRadius * 0.76f,
            std::sin(angle) * config_.crystalHaloRadius * 0.76f +
                config_.crystalLaunchHeight * progress,
            config_.crystalHaloBackOffset},
            {crystalAngle_ * 0.55f, -crystalAngle_,
                static_cast<float>(index) * 0.45f}};
    }
    return pose;
}

void BattleBoss::UpdateVisualPose(float seconds) {
    const BossVisualPose target = CalculateTargetVisualPose();
    const float speed = mode_ == BossMode::Cruise
        ? config_.cruisePoseBlendSpeed : config_.poseBlendSpeed;
    const float amount = std::clamp(seconds * speed, 0.0f, 1.0f);
    for (std::size_t index = 0; index < visualCones_.size(); ++index) {
        BossPartPose& current = visualCones_[index];
        const BossPartPose& destination = target.cones[index];
        if (mode_ == BossMode::SpreadCounter &&
            modeTime_ < config_.spreadWindupDuration) {
            const float progress = std::clamp(modeTime_ /
                config_.spreadWindupDuration, 0.0f, 1.0f);
            current.translation = BlendPosition(
                spreadStartCones_[index].translation,
                destination.translation, progress);
            current.rotation = BlendRotation(spreadStartCones_[index].rotation,
                destination.rotation, progress);
            current.rootTranslation = BlendPosition(
                spreadStartCones_[index].rootTranslation,
                destination.rootTranslation, progress);
        } else if (mode_ == BossMode::SpreadCounter ||
            (mode_ == BossMode::Boxer && boxerNeedleIndex_ >= 0 &&
                (boxerPhase_ == BoxerPhase::WindingUp ||
                    boxerPhase_ == BoxerPhase::Holding ||
                    boxerPhase_ == BoxerPhase::Striking ||
                    boxerPhase_ == BoxerPhase::Recovering) &&
                index == static_cast<std::size_t>(boxerNeedleIndex_)) ||
            (mode_ == BossMode::NeedleCombo &&
                needleVisualInitialized_ && needleImpactAttackId_ == 0)) {
            current = destination;
        } else {
            current.translation = BlendPosition(current.translation,
                destination.translation, amount);
            current.rotation = BlendRotation(current.rotation,
                destination.rotation, amount);
            current.rootTranslation = BlendPosition(current.rootTranslation,
                destination.rootTranslation, amount);
        }
    }
    visualCoreRotation_ = BlendPosition(visualCoreRotation_,
        target.coreRotation, amount);
}

void BattleBoss::InitializeNeedleVisual(const Vector3& playerPosition) {
    needleVisualDirection_ = DirectionTo(position_, playerPosition);
    visualYaw_ = std::atan2(needleVisualDirection_.x,
        needleVisualDirection_.z) + std::numbers::pi_v<float>;
    const BossVisualPose basePose = CalculateTargetVisualPose();
    const float sine = std::sin(visualYaw_);
    const float cosine = std::cos(visualYaw_);
    for (std::size_t index = 0; index < needleVisualStart_.size(); ++index) {
        const Vector3& local = basePose.cones[index].translation;
        needleVisualStart_[index] = {
            position_.x + local.x * cosine + local.z * sine,
            position_.y + local.y,
            position_.z - local.x * sine + local.z * cosine};
        needleVisualPullback_[index] = {
            needleVisualStart_[index].x - needleVisualDirection_.x *
                config_.needlePullbackDistance,
            needleVisualStart_[index].y,
            needleVisualStart_[index].z - needleVisualDirection_.z *
                config_.needlePullbackDistance};
        needleVisualCurrent_[index] = needleVisualStart_[index];
    }
    needleVisualInitialized_ = true;
}

void BattleBoss::BeginBehavior(BossMode mode) {
    if (mode_ == BossMode::Defeating || mode_ == BossMode::Defeated) {
        return;
    }
    if (mode == BossMode::SpreadCounter) {
        spreadStartCones_ = visualCones_;
    }
    mode_ = mode;
    switch (mode) {
    case BossMode::Entrance:
    case BossMode::Charge:
        needlePose_ = BossNeedlePose::ChargeForm;
        if (mode == BossMode::Charge) {
            chargePhase_ = ChargePhase::Preparing;
            chargePhaseTime_ = 0.0f;
            chargeSequenceCount_ = 0;
            chargeTipHit_ = false;
            chargeBodyContact_ = false;
            chargeTipAttackId_ = 0;
            chargeBodyAttackId_ = 0;
        }
        break;
    case BossMode::Cruise:
        needlePose_ = BossNeedlePose::Cruise;
        break;
    case BossMode::FullBarrage:
        needlePose_ = BossNeedlePose::Cruise;
        if (std::hypot(lastPlayerPosition_.x - position_.x,
                lastPlayerPosition_.z - position_.z) > 0.00001f) {
            visualYaw_ = std::atan2(lastPlayerPosition_.x - position_.x,
                lastPlayerPosition_.z - position_.z) +
                std::numbers::pi_v<float>;
        }
        barragePhase_ = BarragePhase::TurningUp;
        barragePhaseTime_ = 0.0f;
        barragePitch_ = 0.0f;
        barrageShotTimer_ = 0.0f;
        spikeSlots_.fill({});
        cubeSlots_.fill({});
        nextSpikeSlot_ = nextCubeSlot_ = 0;
        break;
    case BossMode::Idle:
    case BossMode::SpreadCounter:
        needlePose_ = BossNeedlePose::Spread;
        break;
    case BossMode::Boxer:
    case BossMode::NeedleCombo:
        needlePose_ = BossNeedlePose::NeedleStrike;
        break;
    case BossMode::Defeating:
    case BossMode::Defeated:
        break;
    }
    modeTime_ = 0.0f;
    attackTimer_ = 0.0f;
    attackCount_ = 0;
    attackRegistered_ = false;
    activeAttackId_ = 0;
    strikeTime_ = 0.0f;
    if (mode == BossMode::Boxer) {
        boxerPhase_ = BoxerPhase::Approaching;
        boxerPhaseTime_ = 0.0f;
        boxerEntryDashDistance_ = 0.0f;
        boxerClosingStepCooldown_ = 0.0f;
        usedBoxerEntryDash_ = false;
        boxerCombatStarted_ = false;
    } else if (mode == BossMode::NeedleCombo) {
        needleDashComplete_ = false;
        needleVisualInitialized_ = false;
        strikePrevious_ = position_;
        needleFirstHit_ = false;
        needleFirstAttackId_ = 0;
        needleImpactTime_ = 0.0f;
        needleImpactAttackId_ = 0;
    }
}

void BattleBoss::CyclePracticeNeedlePose() {
    if (mode_ != BossMode::Idle && mode_ != BossMode::Cruise) return;
    mode_ = BossMode::Idle;
    modeTime_ = 0.0f;
    switch (needlePose_) {
    case BossNeedlePose::Cruise: needlePose_ = BossNeedlePose::Spread; break;
    case BossNeedlePose::Spread: needlePose_ = BossNeedlePose::NeedleStrike; break;
    case BossNeedlePose::NeedleStrike: needlePose_ = BossNeedlePose::ChargeForm; break;
    case BossNeedlePose::ChargeForm: needlePose_ = BossNeedlePose::Cruise; break;
    }
}

float BattleBoss::NextBehaviorRandom() {
    behaviorRandomState_ = behaviorRandomState_ * 1664525u + 1013904223u;
    return static_cast<float>((behaviorRandomState_ >> 8) & 0x00ffffffu) /
        16777215.0f;
}

void BattleBoss::ChooseIdleAction(const Vector3& playerPosition,
    float playerDistance) {
    float boxerWeight = playerDistance < config_.idleMinimumDistance
        ? config_.nearBoxerWeight : playerDistance <= config_.idleMaximumDistance
        ? config_.middleBoxerWeight : config_.farBoxerWeight;
    float chargeWeight = playerDistance < config_.idleMinimumDistance
        ? config_.nearChargeWeight : playerDistance <= config_.idleMaximumDistance
        ? config_.middleChargeWeight : config_.farChargeWeight;
    float rangedWeight = playerDistance < config_.idleMinimumDistance
        ? config_.nearRangedWeight : playerDistance <= config_.idleMaximumDistance
        ? config_.middleRangedWeight : config_.farRangedWeight;
    float cruiseWeight = playerDistance < config_.idleMinimumDistance
        ? config_.nearCruiseWeight : playerDistance <= config_.idleMaximumDistance
        ? config_.middleCruiseWeight : config_.farCruiseWeight;
    if (boxerCooldown_ > 0.0f || lastIdleAction_ == BossMode::Boxer) {
        boxerWeight = 0.0f;
    }
    if (chargeCooldown_ > 0.0f || lastIdleAction_ == BossMode::Charge ||
        playerDistance < config_.chargeMinimumStartDistance) {
        chargeWeight = 0.0f;
    }
    if (cruiseCooldown_ > 0.0f || lastIdleAction_ == BossMode::Cruise) {
        cruiseWeight = 0.0f;
    }
    if (lastIdleAction_ == BossMode::Idle) {
        rangedWeight = 0.0f;
    }
    const float totalWeight = boxerWeight + chargeWeight + rangedWeight +
        cruiseWeight;
    if (totalWeight <= 0.0f) {
        lastIdleAction_ = BossMode::Idle;
        modeTime_ = config_.idleDecisionInterval -
            config_.idleNoActionRetryInterval;
        return;
    }
    const float roll = NextBehaviorRandom() * totalWeight;
    if (roll < boxerWeight) {
        boxerCooldown_ = config_.boxerCooldown;
        lastIdleAction_ = BossMode::Boxer;
        BeginBehavior(BossMode::Boxer);
    } else if (roll < boxerWeight + chargeWeight) {
        chargeCooldown_ = config_.chargeCooldown;
        lastIdleAction_ = BossMode::Charge;
        BeginBehavior(BossMode::Charge);
    } else if (roll >= boxerWeight + chargeWeight + rangedWeight) {
        cruiseCooldown_ = config_.cruiseCooldown;
        lastIdleAction_ = BossMode::Cruise;
        BeginCruise(playerPosition);
    } else {
        lastIdleAction_ = BossMode::Idle;
        modeTime_ = 0.0f;
    }
}

void BattleBoss::BeginCruise(const Vector3& playerPosition) {
    BeginBehavior(BossMode::Cruise);
    cruisePhase_ = CruisePhase::TakingOff;
    cruiseFlightTime_ = 0.0f;
    cruiseRetargetCount_ = 0;
    cruiseTargetQuadrant_ = ResolvePlayerQuadrant(playerPosition) ^ 3;
    if (SelectCruiseTarget(playerPosition, cruiseTargetQuadrant_)) {
        return;
    }
    for (const int quadrant : {cruiseTargetQuadrant_ ^ 1,
            cruiseTargetQuadrant_ ^ 2}) {
        if (SelectCruiseTarget(playerPosition, quadrant)) {
            cruiseTargetQuadrant_ = quadrant;
            return;
        }
    }
    cruiseTarget_ = position_;
}

int BattleBoss::ResolvePlayerQuadrant(const Vector3& playerPosition) {
    int quadrant = lastPlayerQuadrant_ < 0 ? 0 : lastPlayerQuadrant_;
    if (playerPosition.x < -config_.cruiseQuadrantDeadZoneX) {
        quadrant &= ~1;
    } else if (playerPosition.x > config_.cruiseQuadrantDeadZoneX) {
        quadrant |= 1;
    }
    if (playerPosition.z < 13.0f - config_.cruiseQuadrantDeadZoneZ) {
        quadrant &= ~2;
    } else if (playerPosition.z > 13.0f +
            config_.cruiseQuadrantDeadZoneZ) {
        quadrant |= 2;
    }
    lastPlayerQuadrant_ = quadrant;
    return quadrant;
}

bool BattleBoss::SelectCruiseTarget(const Vector3& playerPosition,
    int quadrant, int excludedCandidate) {
    const float minimumX = -24.0f + config_.coreRadius +
        config_.cruiseCandidateWallMargin;
    const float maximumX = 24.0f - config_.coreRadius -
        config_.cruiseCandidateWallMargin;
    const float minimumZ = -16.0f + config_.coreRadius +
        config_.cruiseCandidateWallMargin;
    const float maximumZ = 42.0f - config_.coreRadius -
        config_.cruiseCandidateWallMargin;
    const float centerX = (minimumX + maximumX) * 0.5f;
    const float centerZ = (minimumZ + maximumZ) * 0.5f;
    const float halfWidth = (maximumX - minimumX) * 0.5f;
    const float halfDepth = (maximumZ - minimumZ) * 0.5f;
    const float xSign = (quadrant & 1) != 0 ? 1.0f : -1.0f;
    const float zSign = (quadrant & 2) != 0 ? 1.0f : -1.0f;
    constexpr float xFactors[3]{0.55f, 0.78f, 0.38f};
    constexpr float zFactors[3]{0.55f, 0.38f, 0.78f};
    float bestScore = -1000000.0f;
    int bestCandidate = -1;
    Vector3 bestTarget = position_;
    for (int index = 0; index < 3; ++index) {
        const int candidateId = quadrant * 3 + index;
        if (candidateId == excludedCandidate) {
            continue;
        }
        const Vector3 candidate{centerX + xSign * halfWidth * xFactors[index],
            config_.hoverHeight,
            centerZ + zSign * halfDepth * zFactors[index]};
        if (std::hypot(candidate.x, candidate.z + 16.0f) <
            7.0f + config_.coreRadius + config_.cruiseCandidateBaseMargin) {
            continue;
        }
        const float playerDistance = std::hypot(candidate.x - playerPosition.x,
            candidate.z - playerPosition.z);
        const float relocationDistance = std::hypot(candidate.x - position_.x,
            candidate.z - position_.z);
        float score = playerDistance * 2.0f + relocationDistance * 0.35f;
        if (playerDistance < config_.cruiseMinimumLandingPlayerDistance) {
            score -= (config_.cruiseMinimumLandingPlayerDistance -
                playerDistance) * 100.0f;
        }
        if (relocationDistance < config_.cruiseMinimumRelocationDistance) {
            score -= (config_.cruiseMinimumRelocationDistance -
                relocationDistance) * 50.0f;
        }
        if (candidateId == lastCruiseCandidate_) {
            score -= 1000.0f;
        }
        if (score > bestScore) {
            bestScore = score;
            bestCandidate = candidateId;
            bestTarget = candidate;
        }
    }
    if (bestCandidate < 0) {
        return false;
    }
    cruiseTargetCandidate_ = bestCandidate;
    cruiseTarget_ = bestTarget;
    return true;
}

void BattleBoss::UpdateCruise(float seconds, const Vector3& playerPosition) {
    const float flightHeight = config_.hoverHeight + config_.cruiseTakeoffHeight;
    switch (cruisePhase_) {
    case CruisePhase::TakingOff:
        position_.y = std::min(flightHeight,
            position_.y + config_.cruiseTakeoffSpeed * seconds);
        if (position_.y >= flightHeight) {
            cruisePhase_ = CruisePhase::Flying;
            cruiseFlightTime_ = 0.0f;
        }
        break;
    case CruisePhase::Flying: {
        cruiseFlightTime_ += seconds;
        const float angle = cruiseFlightTime_ *
            config_.cruiseHoverOrbitAngularSpeed;
        const Vector3 target{
            std::clamp(cruiseTarget_.x + std::cos(angle) *
                config_.cruiseHoverOrbitRadius, -22.0f, 22.0f),
            flightHeight,
            std::clamp(cruiseTarget_.z + std::sin(angle) *
                config_.cruiseHoverOrbitRadius, -14.0f, 40.0f)};
        const float dx = target.x - position_.x;
        const float dz = target.z - position_.z;
        const float distance = std::hypot(dx, dz);
        const float step = std::min(distance,
            config_.cruiseFlightSpeed * seconds);
        if (distance > 0.00001f) {
            visualYaw_ = std::atan2(dx, dz) + std::numbers::pi_v<float>;
            position_.x += dx / distance * step;
            position_.z += dz / distance * step;
        }
        if (distance <= step + 0.00001f && cruiseFlightTime_ >=
            config_.cruiseMinimumFlightDuration) {
            const float playerDistance = std::hypot(
                playerPosition.x - position_.x,
                playerPosition.z - position_.z);
            if (playerDistance < config_.cruiseMinimumLandingPlayerDistance &&
                cruiseRetargetCount_ < config_.cruiseMaximumRetargetCount &&
                SelectCruiseTarget(playerPosition, cruiseTargetQuadrant_,
                    cruiseTargetCandidate_)) {
                ++cruiseRetargetCount_;
                break;
            }
            cruisePhase_ = CruisePhase::Landing;
        }
        break;
    }
    case CruisePhase::Landing:
        if (const Vector3 facing = DirectionTo(position_, playerPosition);
            std::hypot(facing.x, facing.z) > 0.00001f) {
            const float targetYaw = std::atan2(facing.x, facing.z) +
                std::numbers::pi_v<float>;
            const float turn = std::remainder(targetYaw - visualYaw_,
                2.0f * std::numbers::pi_v<float>);
            visualYaw_ += std::clamp(turn,
                -5.5f * seconds, 5.5f * seconds);
        }
        position_.y = std::max(config_.hoverHeight,
            position_.y - config_.cruiseLandingSpeed * seconds);
        if (position_.y <= config_.hoverHeight) {
            lastCruiseCandidate_ = cruiseTargetCandidate_;
            BeginBehavior(BossMode::Idle);
        }
        break;
    }
}

bool BattleBoss::BeginCrystalLaunch(bool spike) {
    const auto begin = [&](auto& slots, std::size_t& nextSlot) {
        for (std::size_t attempt = 0; attempt < slots.size(); ++attempt) {
            const std::size_t index = (nextSlot + attempt) % slots.size();
            BossCrystalSlot& slot = slots[index];
            if (!slot.consumed && slot.launchTime <= 0.0f &&
                slot.reloadTime <= 0.0f) {
                slot.launchTime = 0.00001f;
                nextSlot = (index + 1) % slots.size();
                return true;
            }
        }
        return false;
    };
    const bool launched = spike ? begin(spikeSlots_, nextSpikeSlot_)
        : begin(cubeSlots_, nextCubeSlot_);
    beganCrystalLaunchThisStep_ |= launched;
    return launched;
}

void BattleBoss::UpdateCrystals(float seconds,
    std::vector<BossAttackEvent>& attacks) {
    crystalAngle_ = std::remainder(crystalAngle_ +
        config_.crystalHaloRotationSpeed * seconds,
        2.0f * std::numbers::pi_v<float>);
    const auto update = [&](auto& slots, BossAttackType type,
        float radiusScale, float phase) {
        for (std::size_t index = 0; index < slots.size(); ++index) {
            BossCrystalSlot& slot = slots[index];
            const float angle = crystalAngle_ + phase +
                6.283185307f * static_cast<float>(index) /
                    static_cast<float>(slots.size());
            const float localX = std::cos(angle) *
                config_.crystalHaloRadius * radiusScale;
            const float localY = std::sin(angle) *
                config_.crystalHaloRadius * radiusScale;
            const float rightX = std::cos(visualYaw_);
            const float rightZ = -std::sin(visualYaw_);
            const float forwardX = std::sin(visualYaw_);
            const float forwardZ = std::cos(visualYaw_);
            slot.position = {
                position_.x + rightX * localX +
                    forwardX * config_.crystalHaloBackOffset,
                position_.y + localY,
                position_.z + rightZ * localX +
                    forwardZ * config_.crystalHaloBackOffset};
            if (slot.launchTime > 0.0f) {
                slot.launchTime += seconds;
                const float progress = std::min(1.0f, slot.launchTime /
                    config_.crystalLaunchDuration);
                slot.position.y += progress * config_.crystalLaunchHeight;
                if (progress >= 1.0f) {
                    const bool spike = type == BossAttackType::CrystalSpike;
                    const std::uint32_t sequence = spike
                        ? spikeShotSequence_++ : cubeShotSequence_++;
                    const float laneRatio = spike
                        ? static_cast<float>((sequence * 5u) % 11u) / 10.0f
                        : static_cast<float>((sequence * 7u + 3u) % 11u) / 10.0f;
                    const float minimumX = -24.0f + config_.coreRadius;
                    const float maximumX = 24.0f - config_.coreRadius;
                    const Vector3 spawn{
                        minimumX + (maximumX - minimumX) * laneRatio,
                        config_.hoverHeight,
                        42.0f + config_.crystalRemoteSpawnMargin};
                    const Vector3 towardBase{-spawn.x, 0.0f, -16.0f - spawn.z};
                    const float distance = std::hypot(towardBase.x, towardBase.z);
                    BossAttackEvent shot{type, spawn};
                    shot.direction = distance > 0.00001f
                        ? Vector3{towardBase.x / distance, 0.0f,
                            towardBase.z / distance}
                        : Vector3{0.0f, 0.0f, -1.0f};
                    shot.oscillationPhase = spike ? 0.0f
                        : static_cast<float>(sequence) *
                            std::numbers::pi_v<float> * 0.5f;
                    attacks.push_back(shot);
                    slot.launchTime = 0.0f;
                    if (mode_ == BossMode::FullBarrage) {
                        slot.consumed = true;
                    } else {
                        slot.reloadTime = config_.crystalReloadDuration;
                    }
                }
            } else {
                slot.reloadTime = std::max(0.0f,
                    slot.reloadTime - seconds);
            }
        }
    };
    update(spikeSlots_, BossAttackType::CrystalSpike, 1.0f, 0.0f);
    update(cubeSlots_, BossAttackType::CrystalCube, 0.76f, 0.78539816f);
}

void BattleBoss::UpdateFullBarrage(float seconds,
    const Vector3& playerPosition) {
    constexpr float halfPi = std::numbers::pi_v<float> * 0.5f;
    barragePhaseTime_ += seconds;
    const float flightHeight = config_.hoverHeight +
        config_.barrageFlightHeightOffset;
    const Vector3 flightTarget{0.0f, flightHeight,
        config_.barrageDeploymentZ};
    const Vector3 attackTarget{0.0f, config_.hoverHeight,
        config_.barrageDeploymentZ};
    const auto advance = [&](BarragePhase phase) {
        barragePhase_ = phase;
        barragePhaseTime_ = 0.0f;
    };
    switch (barragePhase_) {
    case BarragePhase::TurningUp: {
        const float ratio = std::clamp(barragePhaseTime_ /
            config_.barrageTurnUpDuration, 0.0f, 1.0f);
        barragePitch_ = halfPi * ratio;
        if (ratio >= 1.0f) advance(BarragePhase::Ascending);
        break;
    }
    case BarragePhase::Ascending:
        if (MoveTowards(position_, {position_.x, flightHeight, position_.z},
                config_.barrageAscendSpeed * seconds))
            advance(BarragePhase::LevelingOut);
        break;
    case BarragePhase::LevelingOut: {
        const float ratio = std::clamp(barragePhaseTime_ /
            config_.barrageLevelingDuration, 0.0f, 1.0f);
        barragePitch_ = halfPi * (1.0f - ratio);
        if (ratio >= 1.0f) advance(BarragePhase::MovingToDeployment);
        break;
    }
    case BarragePhase::MovingToDeployment: {
        const Vector3 travel{flightTarget.x - position_.x, 0.0f,
            flightTarget.z - position_.z};
        if (std::hypot(travel.x, travel.z) > 0.00001f) {
            visualYaw_ = std::atan2(travel.x, travel.z) +
                std::numbers::pi_v<float>;
        }
        if (MoveTowards(position_, flightTarget,
                config_.barrageFlightSpeed * seconds))
            advance(BarragePhase::TurningDown);
        break;
    }
    case BarragePhase::TurningDown: {
        const float ratio = std::clamp(barragePhaseTime_ /
            config_.barrageDiveTurnDuration, 0.0f, 1.0f);
        barragePitch_ = -halfPi * ratio;
        if (ratio >= 1.0f) advance(BarragePhase::Diving);
        break;
    }
    case BarragePhase::Diving:
        if (MoveTowards(position_, attackTarget,
                config_.barrageDiveSpeed * seconds)) {
            needlePose_ = BossNeedlePose::Spread;
            advance(BarragePhase::TurningToSpread);
        }
        break;
    case BarragePhase::TurningToSpread: {
        const Vector3 towardPlayer = DirectionTo(position_, playerPosition);
        const float targetYaw = std::atan2(towardPlayer.x,
            towardPlayer.z) + std::numbers::pi_v<float>;
        const float turn = std::remainder(targetYaw - visualYaw_,
            2.0f * std::numbers::pi_v<float>);
        const float maximumTurn = std::numbers::pi_v<float> * seconds /
            config_.barrageLandingTurnDuration;
        visualYaw_ += std::clamp(turn, -maximumTurn, maximumTurn);
        const float ratio = std::clamp(barragePhaseTime_ /
            config_.barrageLandingTurnDuration, 0.0f, 1.0f);
        barragePitch_ = -halfPi * (1.0f - ratio);
        if (ratio >= 1.0f) {
            barrageShotTimer_ = 0.0f;
            advance(BarragePhase::Firing);
        }
        break;
    }
    case BarragePhase::Firing: {
        barrageShotTimer_ -= seconds;
        if (attackCount_ < 10 && barrageShotTimer_ <= 0.0f) {
            const bool spike = attackCount_ % 2 == 0;
            if (BeginCrystalLaunch(spike) || BeginCrystalLaunch(!spike)) {
                ++attackCount_;
                barrageShotTimer_ = config_.barrageShotInterval;
            }
        }
        const auto launching = [](const BossCrystalSlot& slot) {
            return slot.launchTime > 0.0f;
        };
        if (attackCount_ >= 10 &&
            !std::any_of(spikeSlots_.begin(), spikeSlots_.end(), launching) &&
            !std::any_of(cubeSlots_.begin(), cubeSlots_.end(), launching)) {
            needlePose_ = BossNeedlePose::ChargeForm;
            advance(BarragePhase::ChargeTransition);
        }
        break;
    }
    case BarragePhase::ChargeTransition:
        if (barragePhaseTime_ >=
            config_.barrageChargeTransitionDuration) {
            fullBarrageDamageLocked_ = false;
            const float playerDistance = std::hypot(
                playerPosition.x - position_.x,
                playerPosition.z - position_.z);
            if (playerDistance >= config_.chargeMinimumStartDistance)
                BeginBehavior(BossMode::Charge);
            else
                BeginCruise(playerPosition);
        }
        break;
    }
}

bool BattleBoss::UpdateRocketDodge(float seconds,
    const Vector3& playerPosition, const BossPlayerThreat& threat) {
    bool confirmedAim = false;
    for (std::size_t arm = 0; arm < chargedRocketAimTimers_.size(); ++arm) {
        if (!threat.isRocketCharged[arm]) {
            chargedRocketAimTimers_[arm] = 0.0f;
            evaluatedChargedRocketAim_[arm] = false;
        } else if (!threat.isChargedRocketAimedAtCore[arm] ||
            evaluatedChargedRocketAim_[arm]) {
            chargedRocketAimTimers_[arm] = 0.0f;
        } else {
            chargedRocketAimTimers_[arm] += seconds;
            if (chargedRocketAimTimers_[arm] >=
                config_.chargedRocketAimDodgeDelay) {
                evaluatedChargedRocketAim_[arm] = true;
                confirmedAim = true;
            }
        }
    }
    if (rocketDodgeTimer_ > 0.0f) {
        const float dodgeStep = std::min(rocketDodgeTimer_, seconds);
        rocketDodgeTimer_ = std::max(0.0f,
            rocketDodgeTimer_ - seconds);
        const Vector3 toPlayer = DirectionTo(position_, playerPosition);
        position_.x -= toPlayer.z * idleStrafeSide_ *
            rocketDodgeSpeed_ * dodgeStep;
        position_.z += toPlayer.x * idleStrafeSide_ *
            rocketDodgeSpeed_ * dodgeStep;
        return true;
    }
    if (mode_ == BossMode::Idle &&
        (confirmedAim || threat.hasIncomingRocketPunch) &&
        rocketDodgeCooldown_ <= 0.0f) {
        idleStrafeSide_ = -idleStrafeSide_;
        rocketDodgeTimer_ = config_.idleRocketDodgeDuration;
        rocketDodgeSpeed_ = config_.idleRocketDodgeSpeed;
        rocketDodgeCooldown_ = config_.idleRocketDodgeCooldown;
        beganRocketDodgeThisStep_ = true;
        return true;
    }
    if (mode_ == BossMode::Boxer && confirmedAim &&
        (boxerPhase_ == BoxerPhase::Approaching ||
            boxerPhase_ == BoxerPhase::Orbiting ||
            boxerPhase_ == BoxerPhase::ClosingStep ||
            boxerPhase_ == BoxerPhase::Recovering) &&
        rocketDodgeCooldown_ <= 0.0f &&
        boxerRocketDodgeCooldown_ <= 0.0f) {
        idleStrafeSide_ = -idleStrafeSide_;
        rocketDodgeTimer_ = config_.boxerRocketDodgeDuration;
        rocketDodgeSpeed_ = config_.boxerRocketDodgeSpeed;
        rocketDodgeCooldown_ = config_.idleRocketDodgeCooldown;
        boxerRocketDodgeCooldown_ = config_.boxerRocketDodgeCooldown;
        beganRocketDodgeThisStep_ = true;
        return true;
    }
    return false;
}

bool BattleBoss::ReceiveHit(int damage) {
    if (damage <= 0 || mode_ == BossMode::Defeating ||
        mode_ == BossMode::Defeated || fullBarrageDamageLocked_) {
        return false;
    }
    const int nextHitPoints = hitPoints_ - damage;
    if (!fullBarrageUsed_ && nextHitPoints <= 1) {
        hitPoints_ = 1;
        fullBarragePending_ = true;
        fullBarrageDamageLocked_ = true;
    } else {
        hitPoints_ = std::max(0, nextHitPoints);
    }
    if (!needleComboUsed_ && hitPoints_ <= config_.coreHitPoints / 2) {
        needleComboPending_ = true;
    }
    coreHitFlashRemaining_ = hitPoints_ > 0 ? 0.18f : 0.0f;
    if (hitPoints_ == 0) {
        defeatStartPitch_ = CalculateTargetVisualPose().rootRotation.x;
        mode_ = BossMode::Defeating;
        modeTime_ = 0.0f;
        attackRegistered_ = false;
        defeatPhase_ = DefeatPhase::Airborne;
        defeatPhaseTime_ = 0.0f;
        defeatSmallExplosionTime_ = 0.0f;
        defeatSmallExplosionCount_ = 0;
        defeatRotation_ = {};
        const Vector3 direction{position_.x - lastPlayerPosition_.x, 0.0f,
            position_.z - lastPlayerPosition_.z};
        SetDefeatLaunchDirection(direction);
    }
    return true;
}

void BattleBoss::SetDefeatLaunchDirection(const Vector3& direction) {
    if (mode_ != BossMode::Defeating) {
        return;
    }
    const float length = std::hypot(direction.x, direction.z);
    const float unitX = length > 0.00001f ? direction.x / length : 0.0f;
    const float unitZ = length > 0.00001f ? direction.z / length : 1.0f;
    defeatVelocity_ = {unitX * config_.defeatLaunchHorizontalSpeed,
        config_.defeatLaunchUpwardSpeed,
        unitZ * config_.defeatLaunchHorizontalSpeed};
}

bool BattleBoss::RegisterNeedleStrikeHit(std::uint64_t attackId) {
    if (mode_ == BossMode::NeedleCombo &&
        attackId == needleFirstAttackId_) {
        needleFirstHit_ = true;
        return true;
    }
    return false;
}

bool BattleBoss::RegisterChargeTipHit(std::uint64_t attackId) {
    if (mode_ != BossMode::Charge ||
        chargePhase_ != ChargePhase::Charging ||
        attackId != chargeTipAttackId_ || chargeTipHit_) {
        return false;
    }
    chargeTipHit_ = true;
    chargePhase_ = ChargePhase::Impact;
    chargePhaseTime_ = 0.0f;
    return true;
}

bool BattleBoss::RegisterChargeBodyContact(std::uint64_t attackId) {
    if (mode_ != BossMode::Charge ||
        chargePhase_ != ChargePhase::Charging ||
        attackId != chargeBodyAttackId_ || chargeBodyContact_) {
        return false;
    }
    chargeBodyContact_ = true;
    return true;
}

std::vector<BossAttackEvent> BattleBoss::Update(float seconds,
    const Vector3& playerPosition, const BossPlayerThreat& threat) {
    std::vector<BossAttackEvent> attacks;
    beganRocketDodgeThisStep_ = false;
    beganCrystalLaunchThisStep_ = false;
    beganChargeThisStep_ = false;
    beganStrikeThisStep_ = false;
    emittedDefeatSmallExplosionThisStep_ = false;
    if (!std::isfinite(seconds) || seconds < 0.0f ||
        mode_ == BossMode::Defeated) {
        return attacks;
    }
    previousVisualPose_ = GetVisualPose();
    coreHitFlashRemaining_ = std::max(0.0f,
        coreHitFlashRemaining_ - seconds);
    previousPosition_ = position_;
    modeTime_ += seconds;
    if (mode_ == BossMode::Defeating) {
        UpdateDefeat(seconds);
        UpdateVisualPose(seconds);
        return attacks;
    }
    lastPlayerPosition_ = playerPosition;
    if ((mode_ != BossMode::Entrance || entranceRecovering_) &&
        mode_ != BossMode::Defeating && mode_ != BossMode::FullBarrage &&
        mode_ != BossMode::NeedleCombo && mode_ != BossMode::Charge) {
        const bool isBoxerStriking = mode_ == BossMode::Boxer &&
            (boxerPhase_ == BoxerPhase::WindingUp ||
                boxerPhase_ == BoxerPhase::Holding ||
                boxerPhase_ == BoxerPhase::Striking ||
                boxerPhase_ == BoxerPhase::Recovering);
        const Vector3 facing = isBoxerStriking ? boxerVisualDirection_
            : DirectionTo(position_, playerPosition);
        const float targetYaw = std::atan2(facing.x,
            facing.z) + std::numbers::pi_v<float>;
        const float turn = std::remainder(targetYaw - visualYaw_,
            2.0f * std::numbers::pi_v<float>);
        visualYaw_ += std::clamp(turn, -5.5f * seconds, 5.5f * seconds);
    }
    spreadCooldown_ = std::max(0.0f, spreadCooldown_ - seconds);
    boxerCooldown_ = std::max(0.0f, boxerCooldown_ - seconds);
    chargeCooldown_ = std::max(0.0f, chargeCooldown_ - seconds);
    cruiseCooldown_ = std::max(0.0f, cruiseCooldown_ - seconds);
    rocketDodgeCooldown_ = std::max(0.0f,
        rocketDodgeCooldown_ - seconds);
    boxerRocketDodgeCooldown_ = std::max(0.0f,
        boxerRocketDodgeCooldown_ - seconds);
    if (fullBarragePending_ && mode_ != BossMode::Entrance) {
        fullBarragePending_ = false;
        fullBarrageUsed_ = true;
        BeginBehavior(BossMode::FullBarrage);
    } else if (needleComboPending_ && mode_ == BossMode::Idle) {
        needleComboPending_ = false;
        needleComboUsed_ = true;
        BeginBehavior(BossMode::NeedleCombo);
    }
    if (mode_ != BossMode::Idle && mode_ != BossMode::Boxer) {
        chargedRocketAimTimers_.fill(0.0f);
    }
    switch (mode_) {
    case BossMode::Entrance:
        if (!entranceRecovering_) {
            position_.y = std::max(config_.hoverHeight +
                config_.entranceFlipStartHeight,
                position_.y - config_.entranceDropSpeed * seconds);
            if (position_.y <= config_.hoverHeight +
                    config_.entranceFlipStartHeight) {
                entranceRecovering_ = true;
                entranceRecoveryTime_ = 0.0f;
            }
        } else {
            entranceRecoveryTime_ += seconds;
            const float ratio = std::clamp(entranceRecoveryTime_ /
                config_.entranceFlipDuration, 0.0f, 1.0f);
            position_.y = config_.hoverHeight +
                config_.entranceFlipStartHeight * (1.0f - ratio);
            if (ratio >= 1.0f) BeginBehavior(BossMode::Idle);
        }
        break;
    case BossMode::Cruise:
        UpdateCruise(seconds, playerPosition);
        break;
    case BossMode::Idle: {
        const float distance = std::hypot(playerPosition.x - position_.x,
            playerPosition.z - position_.z);
        const Vector3 toPlayer = DirectionTo(position_, playerPosition);
        if (distance >= config_.spreadPressureRearmDistance) {
            spreadPressureArmed_ = true;
        }
        if (spreadPressureArmed_ &&
            distance <= config_.spreadPressureDistance) {
            spreadPressureTimer_ = std::min(config_.spreadPressureDuration,
                spreadPressureTimer_ + seconds);
        } else {
            spreadPressureTimer_ = std::max(0.0f,
                spreadPressureTimer_ -
                    config_.spreadPressureDecaySpeed * seconds);
        }
        if (spreadPressureTimer_ >= config_.spreadPressureDuration &&
            spreadCooldown_ <= 0.0f) {
            spreadPressureArmed_ = false;
            spreadPressureTimer_ = 0.0f;
            spreadCooldown_ = config_.spreadBurstCooldown;
            BeginBehavior(BossMode::SpreadCounter);
            break;
        }
        if (UpdateRocketDodge(seconds, playerPosition, threat)) {
            break;
        }
        idleStrafeTimer_ += seconds;
        if (idleStrafeTimer_ >= config_.idleStrafePeriod) {
            idleStrafeTimer_ = 0.0f;
            idleStrafeSide_ = -idleStrafeSide_;
        }
        Vector3 moveDirection{};
        float moveSpeed = config_.idleMoveSpeed;
        if (distance < config_.chargeMinimumStartDistance) {
            moveDirection = {-toPlayer.x, 0.0f, -toPlayer.z};
            if (distance <= config_.spreadPressureDistance) {
                moveSpeed = config_.spreadPressureRetreatSpeed;
            }
        } else if (distance > config_.idleMaximumDistance) {
            moveDirection = toPlayer;
        } else {
            moveDirection = {-toPlayer.z * idleStrafeSide_, 0.0f,
                toPlayer.x * idleStrafeSide_};
        }
        position_.x += moveDirection.x * moveSpeed * seconds;
        position_.z += moveDirection.z * moveSpeed * seconds;
        crystalSpikeTimer_ -= seconds;
        crystalCubeTimer_ -= seconds;
        if (crystalSpikeTimer_ <= 0.0f) {
            if (BeginCrystalLaunch(true)) {
                crystalSpikeTimer_ = config_.crystalSpikeInterval;
            }
        }
        if (crystalCubeTimer_ <= 0.0f) {
            if (BeginCrystalLaunch(false)) {
                crystalCubeTimer_ = config_.crystalCubeInterval;
            }
        }
        if (modeTime_ >= config_.idleDecisionInterval) {
            ChooseIdleAction(playerPosition, distance);
        }
        break;
    }
    case BossMode::Charge:
        chargePhaseTime_ += seconds;
        if (chargePhase_ == ChargePhase::Preparing) {
            const Vector3 towardPlayer = DirectionTo(position_, playerPosition);
            const float targetYaw = std::atan2(towardPlayer.x,
                towardPlayer.z) + std::numbers::pi_v<float>;
            const float difference = std::remainder(targetYaw - visualYaw_,
                2.0f * std::numbers::pi_v<float>);
            visualYaw_ += std::clamp(difference,
                -config_.chargeAimTurnSpeed * seconds,
                config_.chargeAimTurnSpeed * seconds);
            if (chargePhaseTime_ > config_.chargeTurnDuration &&
                chargePhaseTime_ <= config_.chargeTurnDuration +
                    config_.chargePullbackDuration) {
                const float pullbackSpeed = config_.chargePullbackDistance /
                    config_.chargePullbackDuration;
                position_.x -= towardPlayer.x * pullbackSpeed * seconds;
                position_.z -= towardPlayer.z * pullbackSpeed * seconds;
            }
            if (chargePhaseTime_ >= config_.chargePrepareDuration) {
                chargeDirection_ = DirectionTo(position_, playerPosition);
                chargePhase_ = ChargePhase::Charging;
                chargePhaseTime_ = 0.0f;
                beganChargeThisStep_ = true;
                chargeTipHit_ = false;
                chargeBodyContact_ = false;
                chargeTipAttackId_ = nextAttackId_++;
                chargeBodyAttackId_ = nextAttackId_++;
            }
        } else if (chargePhase_ == ChargePhase::Charging) {
            if (MoveChargeWithinBattlefield(config_.chargeSpeed * seconds)) {
                chargePhase_ = ChargePhase::WallImpact;
                chargePhaseTime_ = 0.0f;
                break;
            }
            const Vector3 tip{position_.x + chargeDirection_.x *
                    config_.chargeTipOffset, position_.y,
                position_.z + chargeDirection_.z *
                    config_.chargeTipOffset};
            const Vector3 previousTip{previousPosition_.x +
                    chargeDirection_.x * config_.chargeTipOffset,
                previousPosition_.y, previousPosition_.z +
                    chargeDirection_.z * config_.chargeTipOffset};
            attacks.push_back({BossAttackType::ChargeImpact, tip,
                previousTip, config_.chargeTipRadius, chargeTipAttackId_});
            attacks.push_back({BossAttackType::ChargeBodyContact, position_,
                previousPosition_, config_.coreRadius,
                chargeBodyAttackId_});
        } else if ((chargePhase_ == ChargePhase::Impact &&
                chargePhaseTime_ >= config_.chargeImpactDuration) ||
            (chargePhase_ == ChargePhase::WallImpact &&
                chargePhaseTime_ >= config_.chargeWallImpactDuration)) {
            ++chargeSequenceCount_;
            const float distance = std::hypot(playerPosition.x - position_.x,
                playerPosition.z - position_.z);
            if (!chargeTipHit_ &&
                chargeSequenceCount_ < config_.chargeSequenceCount &&
                distance >= config_.chargeMinimumStartDistance) {
                chargePhase_ = ChargePhase::Preparing;
                chargePhaseTime_ = 0.0f;
            } else {
                BeginBehavior(BossMode::Idle);
            }
        }
        break;
    case BossMode::Boxer: {
        boxerClosingStepCooldown_ = std::max(0.0f,
            boxerClosingStepCooldown_ - seconds);
        if (UpdateRocketDodge(seconds, playerPosition, threat)) {
            break;
        }
        const float distance = std::hypot(playerPosition.x - position_.x,
            playerPosition.z - position_.z);
        const Vector3 direction = DirectionTo(position_, playerPosition);
        boxerPhaseTime_ += seconds;
        const auto boxerTipAt = [&](const Vector3& root) {
            return TipToward(root, boxerVisualTarget_,
                boxerVisualDirection_, config_.needleTipOffset);
        };
        const auto beginStrike = [&](const Vector3& strikeDirection) {
            beganStrikeThisStep_ = true;
            if (!boxerCombatStarted_) {
                boxerCombatStarted_ = true;
                modeTime_ = 0.0f;
            }
            const int nextNeedleIndex = (boxerNeedleIndex_ + 1) % 3;
            const BossPartPose base = CalculateTargetVisualPose().cones[
                static_cast<std::size_t>(nextNeedleIndex)];
            boxerPhase_ = BoxerPhase::WindingUp;
            boxerPhaseTime_ = 0.0f;
            boxerNeedleIndex_ = nextNeedleIndex;
            const float sine = std::sin(visualYaw_);
            const float cosine = std::cos(visualYaw_);
            boxerVisualStart_ = {
                position_.x + base.translation.x * cosine +
                    base.translation.z * sine,
                position_.y + base.translation.y,
                position_.z - base.translation.x * sine +
                    base.translation.z * cosine};
            boxerVisualTarget_ = {
                position_.x + strikeDirection.x * config_.boxerStrikeOffset,
                position_.y,
                position_.z + strikeDirection.z * config_.boxerStrikeOffset};
            const Vector3 towardTarget{
                boxerVisualTarget_.x - boxerVisualStart_.x,
                boxerVisualTarget_.y - boxerVisualStart_.y,
                boxerVisualTarget_.z - boxerVisualStart_.z};
            const float targetDistance = std::sqrt(
                towardTarget.x * towardTarget.x +
                towardTarget.y * towardTarget.y +
                towardTarget.z * towardTarget.z);
            const Vector3 pullbackDirection = targetDistance > 0.00001f
                ? Vector3{towardTarget.x / targetDistance,
                    towardTarget.y / targetDistance,
                    towardTarget.z / targetDistance}
                : Vector3{0.0f, 0.0f, -1.0f};
            boxerVisualPullback_ = {
                boxerVisualStart_.x - pullbackDirection.x *
                    config_.boxerStrikePullbackDistance,
                boxerVisualStart_.y - pullbackDirection.y *
                    config_.boxerStrikePullbackDistance,
                boxerVisualStart_.z - pullbackDirection.z *
                    config_.boxerStrikePullbackDistance};
            boxerVisualDirection_ = strikeDirection;
            visualYaw_ = std::atan2(strikeDirection.x, strikeDirection.z) +
                std::numbers::pi_v<float>;
            strikePrevious_ = boxerTipAt(boxerVisualStart_);
        };
        if (boxerPhase_ == BoxerPhase::Approaching) {
            if (!usedBoxerEntryDash_ &&
                distance > config_.boxerDesiredDistance +
                    config_.boxerDistanceTolerance &&
                distance <= config_.boxerEntryMaximumDistance) {
                boxerPhase_ = BoxerPhase::EntryWindup;
                boxerPhaseTime_ = 0.0f;
                break;
            }
            const float approachDistance = std::max(0.0f,
                distance - config_.boxerDesiredDistance);
            const float step = std::min(config_.boxerApproachSpeed * seconds,
                approachDistance);
            position_.x += direction.x * step;
            position_.z += direction.z * step;
            const float currentDistance = std::hypot(
                playerPosition.x - position_.x,
                playerPosition.z - position_.z);
            if (currentDistance <= config_.boxerDesiredDistance +
                    config_.boxerDistanceTolerance) {
                if (usedBoxerEntryDash_ && !boxerCombatStarted_ &&
                    currentDistance <= config_.boxerStrikeStartDistance) {
                    beginStrike(DirectionTo(position_, playerPosition));
                } else {
                    boxerPhase_ = BoxerPhase::Orbiting;
                    boxerPhaseTime_ = 0.0f;
                }
            }
        } else if (boxerPhase_ == BoxerPhase::Orbiting) {
            if (distance > config_.boxerClosingStepMaximumDistance) {
                boxerPhase_ = BoxerPhase::Approaching;
                boxerPhaseTime_ = 0.0f;
                break;
            }
            if (attackTimer_ >= config_.boxerStrikeInterval &&
                distance <= config_.boxerStrikeStartDistance) {
                beginStrike(direction);
                break;
            }
            if (attackTimer_ >= config_.boxerStrikeInterval &&
                distance > config_.boxerStrikeStartDistance &&
                boxerClosingStepCooldown_ <= 0.0f) {
                boxerPhase_ = BoxerPhase::ClosingStep;
                boxerPhaseTime_ = 0.0f;
                boxerClosingStepCooldown_ =
                    config_.boxerClosingStepCooldown;
                break;
            }
            const Vector3 tangent{-direction.z * idleStrafeSide_, 0.0f,
                direction.x * idleStrafeSide_};
            const float radial = std::clamp(distance -
                config_.boxerDesiredDistance, -1.0f, 1.0f);
            const Vector3 move = attackTimer_ >= config_.boxerStrikeInterval &&
                    distance > config_.boxerStrikeStartDistance
                ? direction
                : Vector3{tangent.x + direction.x * radial, 0.0f,
                    tangent.z + direction.z * radial};
            const float length = std::hypot(move.x, move.z);
            if (length > 0.00001f) {
                position_.x += move.x / length * config_.boxerOrbitSpeed * seconds;
                position_.z += move.z / length * config_.boxerOrbitSpeed * seconds;
            }
            attackTimer_ = std::min(config_.boxerStrikeInterval,
                attackTimer_ + seconds);
            if (attackTimer_ >= config_.boxerStrikeInterval &&
                std::hypot(playerPosition.x - position_.x,
                    playerPosition.z - position_.z) <=
                    config_.boxerStrikeStartDistance) {
                beginStrike(DirectionTo(position_, playerPosition));
            }
        } else if (boxerPhase_ == BoxerPhase::ClosingStep) {
            if (distance <= config_.boxerStrikeStartDistance) {
                beginStrike(direction);
                break;
            }
            if (distance > config_.boxerClosingStepMaximumDistance) {
                boxerPhase_ = BoxerPhase::Approaching;
                boxerPhaseTime_ = 0.0f;
                break;
            }
            const Vector3 tangent{-direction.z * idleStrafeSide_, 0.0f,
                direction.x * idleStrafeSide_};
            const float moveX = direction.x + tangent.x *
                config_.boxerClosingStepLateralWeight;
            const float moveZ = direction.z + tangent.z *
                config_.boxerClosingStepLateralWeight;
            const float length = std::hypot(moveX, moveZ);
            const float step = std::min(config_.boxerClosingStepSpeed * seconds,
                distance - config_.boxerStrikeStartDistance +
                    config_.boxerClosingStepEndMargin);
            if (length > 0.00001f) {
                position_.x += moveX / length * step;
                position_.z += moveZ / length * step;
            }
            const float currentDistance = std::hypot(
                playerPosition.x - position_.x,
                playerPosition.z - position_.z);
            if (currentDistance <= config_.boxerStrikeStartDistance) {
                beginStrike(DirectionTo(position_, playerPosition));
            } else if (boxerPhaseTime_ >=
                    config_.boxerClosingStepDuration) {
                boxerPhase_ = currentDistance >
                    config_.boxerClosingStepMaximumDistance
                    ? BoxerPhase::Approaching : BoxerPhase::Orbiting;
                boxerPhaseTime_ = 0.0f;
            }
        } else if (boxerPhase_ == BoxerPhase::EntryWindup) {
            position_.x -= direction.x * config_.boxerEntryPullbackDistance *
                seconds / config_.boxerEntryWindupDuration;
            position_.z -= direction.z * config_.boxerEntryPullbackDistance *
                seconds / config_.boxerEntryWindupDuration;
            if (boxerPhaseTime_ >= config_.boxerEntryWindupDuration) {
                boxerEntryDirection_ = DirectionTo(position_, playerPosition);
                boxerEntryDashDistance_ = 0.0f;
                usedBoxerEntryDash_ = true;
                boxerPhase_ = BoxerPhase::EntryDashing;
                boxerPhaseTime_ = 0.0f;
            }
        } else if (boxerPhase_ == BoxerPhase::EntryDashing) {
            const float forwardDistance =
                (playerPosition.x - position_.x) * boxerEntryDirection_.x +
                (playerPosition.z - position_.z) * boxerEntryDirection_.z;
            const float dashDistance = std::max(0.0f, std::min({
                config_.boxerEntryDashSpeed * seconds,
                config_.boxerEntryDashMaximumDistance -
                    boxerEntryDashDistance_,
                forwardDistance - config_.boxerStrikeStartDistance}));
            position_.x += boxerEntryDirection_.x * dashDistance;
            position_.z += boxerEntryDirection_.z * dashDistance;
            boxerEntryDashDistance_ += dashDistance;
            const float remainingDistance = std::hypot(
                playerPosition.x - position_.x,
                playerPosition.z - position_.z);
            if (forwardDistance - dashDistance <=
                    config_.boxerStrikeStartDistance + 0.001f &&
                remainingDistance <= config_.boxerStrikeStartDistance +
                    0.8f) {
                beginStrike(boxerEntryDirection_);
            } else if (boxerPhaseTime_ >=
                    config_.boxerEntryDashMaximumDuration ||
                boxerEntryDashDistance_ >=
                    config_.boxerEntryDashMaximumDistance - 0.001f ||
                dashDistance <= 0.0f) {
                boxerPhase_ = BoxerPhase::Approaching;
                boxerPhaseTime_ = 0.0f;
            }
        } else if (boxerPhase_ == BoxerPhase::WindingUp) {
            const float progress = std::clamp(boxerPhaseTime_ /
                config_.boxerWindupDuration, 0.0f, 1.0f);
            strikePrevious_ = boxerTipAt(BlendPosition(boxerVisualStart_,
                boxerVisualPullback_, progress));
            if (boxerPhaseTime_ >= config_.boxerWindupDuration) {
                boxerPhase_ = BoxerPhase::Holding;
                boxerPhaseTime_ = 0.0f;
                strikePrevious_ = boxerTipAt(boxerVisualPullback_);
            }
        } else if (boxerPhase_ == BoxerPhase::Holding) {
            strikePrevious_ = boxerTipAt(boxerVisualPullback_);
            if (boxerPhaseTime_ >= config_.boxerWindupHoldDuration) {
                boxerPhase_ = BoxerPhase::Striking;
                boxerPhaseTime_ = 0.0f;
                activeAttackId_ = nextAttackId_++;
                strikeTime_ = 0.0f;
            }
        } else if (boxerPhase_ == BoxerPhase::Striking) {
            strikeTime_ += seconds;
            const float progress = std::clamp(strikeTime_ /
                config_.boxerStrikeDuration, 0.0f, 1.0f);
            const Vector3 root = BlendPosition(boxerVisualPullback_,
                boxerVisualTarget_, progress);
            const Vector3 tip = boxerTipAt(root);
            attacks.push_back({BossAttackType::BoxerStrike, tip,
                strikePrevious_, config_.boxerStrikeRadius, activeAttackId_});
            strikePrevious_ = tip;
            if (progress >= 1.0f) {
                activeAttackId_ = 0;
                boxerPhase_ = BoxerPhase::Recovering;
                boxerPhaseTime_ = 0.0f;
            }
        } else if (boxerPhase_ == BoxerPhase::Recovering &&
            boxerPhaseTime_ >= config_.boxerRecoveryDuration) {
            boxerPhase_ = BoxerPhase::Orbiting;
            boxerPhaseTime_ = 0.0f;
            attackTimer_ = 0.0f;
            idleStrafeSide_ = -idleStrafeSide_;
            if (boxerNeedleIndex_ >= 0) {
                visualCones_[static_cast<std::size_t>(boxerNeedleIndex_)] =
                    CalculateTargetVisualPose().cones[
                        static_cast<std::size_t>(boxerNeedleIndex_)];
            }
        }
        if (modeTime_ >= (boxerCombatStarted_
                ? config_.boxerBehaviorDuration
                : config_.boxerEntryTimeout) &&
            (boxerPhase_ == BoxerPhase::Approaching ||
                boxerPhase_ == BoxerPhase::Orbiting ||
                boxerPhase_ == BoxerPhase::ClosingStep)) {
            cruiseCooldown_ = config_.cruiseCooldown;
            BeginCruise(playerPosition);
        }
        break;
    }
    case BossMode::NeedleCombo: {
        if (!needleVisualInitialized_) {
            InitializeNeedleVisual(playerPosition);
        }
        const float strikePeriod = config_.needleStrikeDuration +
            config_.needleStrikePauseDuration;
        if (attackCount_ == 1 && activeAttackId_ == 0 &&
            strikeTime_ >= config_.needleStrikeDuration &&
            !needleFirstHit_) {
            BeginBehavior(BossMode::Boxer);
            break;
        }
        if (!needleDashComplete_) {
            if (modeTime_ < config_.needleWindupDuration) {
                strikePrevious_ = position_;
                const float progress = std::clamp(modeTime_ /
                    config_.needleWindupDuration, 0.0f, 1.0f);
                for (std::size_t index = 0;
                        index < needleVisualCurrent_.size(); ++index) {
                    needleVisualCurrent_[index] = BlendPosition(
                        needleVisualStart_[index],
                        needleVisualPullback_[index], progress);
                }
                break;
            }
            if (modeTime_ - seconds < config_.needleWindupDuration) {
                needleVisualCurrent_ = needleVisualPullback_;
            }
            strikePrevious_ = position_;
            const float distance = std::hypot(playerPosition.x - position_.x,
                playerPosition.z - position_.z);
            if (distance > config_.needleFrontDistance) {
                const Vector3 direction = DirectionTo(position_,
                    playerPosition);
                needleVisualDirection_ = direction;
                visualYaw_ = std::atan2(direction.x, direction.z) +
                    std::numbers::pi_v<float>;
                const float dashDistance = std::min(
                    config_.needleDashSpeed * seconds,
                    distance - config_.needleFrontDistance);
                position_.x += direction.x * dashDistance;
                position_.z += direction.z * dashDistance;
                for (Vector3& current : needleVisualCurrent_) {
                    current.x += direction.x * dashDistance;
                    current.z += direction.z * dashDistance;
                }
            }
            if (std::hypot(playerPosition.x - position_.x,
                    playerPosition.z - position_.z) >
                config_.needleFrontDistance + 0.8f) {
                break;
            }
            needleDashComplete_ = true;
            attackTimer_ = 0.0f;
        }
        attackTimer_ += seconds;
        const float activeTime = attackTimer_;
        if (activeTime >= 0.0f && attackCount_ < 3 &&
            activeTime >= attackCount_ * strikePeriod) {
            beganStrikeThisStep_ = true;
            activeAttackId_ = nextAttackId_++;
            if (attackCount_ == 0) {
                needleFirstAttackId_ = activeAttackId_;
            }
            strikeTime_ = 0.0f;
            needleVisualTarget_ = {
                position_.x + needleVisualDirection_.x *
                    config_.needleStrikeOffset,
                position_.y,
                position_.z + needleVisualDirection_.z *
                    config_.needleStrikeOffset};
            ++attackCount_;
        }
        bool enteredImpactThisStep = false;
        if (activeAttackId_ != 0) {
            strikeTime_ += seconds;
            const float progress = std::clamp(strikeTime_ /
                config_.needleStrikeDuration, 0.0f, 1.0f);
            const std::size_t visualIndex = static_cast<std::size_t>(
                std::clamp(attackCount_ - 1, 0, 2));
            needleVisualCurrent_[visualIndex] = BlendPosition(
                needleVisualPullback_[visualIndex], needleVisualTarget_,
                progress);
            const Vector3& root = needleVisualCurrent_[visualIndex];
            const Vector3 tip{
                root.x + needleVisualDirection_.x * config_.needleTipOffset,
                root.y,
                root.z + needleVisualDirection_.z * config_.needleTipOffset};
            attacks.push_back({BossAttackType::NeedleStrike, tip,
                strikePrevious_, config_.needleStrikeRadius, activeAttackId_});
            strikePrevious_ = tip;
            if (progress >= 1.0f) {
                activeAttackId_ = 0;
                if (attackCount_ == 3) {
                    needleImpactAttackId_ = nextAttackId_++;
                    enteredImpactThisStep = true;
                    needlePose_ = BossNeedlePose::ChargeForm;
                    const BossVisualPose chargePose =
                        CalculateTargetVisualPose();
                    visualCones_ = chargePose.cones;
                    visualCoreRotation_ = chargePose.coreRotation;
                    previousVisualPose_ = GetVisualPose();
                }
            }
        }
        if (!enteredImpactThisStep && needleImpactAttackId_ != 0 &&
            needleImpactTime_ < 0.12f) {
            needleImpactTime_ += seconds;
            const Vector3 impact{
                position_.x + needleVisualDirection_.x *
                    config_.needleStrikeOffset,
                position_.y,
                position_.z + needleVisualDirection_.z *
                    config_.needleStrikeOffset};
            attacks.push_back({BossAttackType::NeedleImpact,
                impact, strikePrevious_,
                config_.needleStrikeRadius, needleImpactAttackId_});
            strikePrevious_ = impact;
        }
        if (activeTime >= 3.0f * strikePeriod + 2.0f) {
            BeginBehavior(BossMode::Idle);
        }
        break;
    }
    case BossMode::FullBarrage:
        UpdateFullBarrage(seconds, playerPosition);
        break;
    case BossMode::SpreadCounter:
        if (!attackRegistered_ && modeTime_ >=
                config_.spreadWindupDuration + config_.spreadSpinDuration) {
            attackRegistered_ = true;
            if (std::hypot(playerPosition.x - position_.x,
                    playerPosition.z - position_.z) <= config_.spreadBurstRadius +
                    FirstPersonTuning::kPlayerColliderRadius) {
                attacks.push_back({BossAttackType::SpreadBurst, position_});
            }
        }
        if (modeTime_ >= config_.spreadWindupDuration +
                config_.spreadSpinDuration + 1.0f) {
            BeginBehavior(BossMode::Idle);
        }
        break;
    case BossMode::Defeating:
        break;
    case BossMode::Defeated:
        break;
    }
    UpdateCrystals(seconds, attacks);
    position_.x = std::clamp(position_.x,
        FirstPersonTuning::kBattlefieldMinimumX + config_.coreRadius,
        FirstPersonTuning::kBattlefieldMaximumX - config_.coreRadius);
    position_.z = std::clamp(position_.z,
        FirstPersonTuning::kBattlefieldMinimumZ + config_.coreRadius,
        FirstPersonTuning::kBattlefieldMaximumZ - config_.coreRadius);
    const float fromBaseX = position_.x;
    const float fromBaseZ = position_.z + 16.0f;
    const float fromBaseLength = std::hypot(fromBaseX, fromBaseZ);
    const float baseClearance = 7.0f + config_.coreRadius;
    if (fromBaseLength > 0.00001f &&
        fromBaseLength < baseClearance) {
        position_.x = fromBaseX * baseClearance / fromBaseLength;
        position_.z = -16.0f + fromBaseZ * baseClearance / fromBaseLength;
    }
    UpdateVisualPose(seconds);
    return attacks;
}

bool BattleBoss::MoveChargeWithinBattlefield(float distance) {
    const Vector3 start = position_;
    position_.x += chargeDirection_.x * distance;
    position_.z += chargeDirection_.z * distance;
    const float unclampedX = position_.x;
    const float unclampedZ = position_.z;
    position_.x = std::clamp(position_.x,
        FirstPersonTuning::kBattlefieldMinimumX + config_.coreRadius,
        FirstPersonTuning::kBattlefieldMaximumX - config_.coreRadius);
    position_.z = std::clamp(position_.z,
        FirstPersonTuning::kBattlefieldMinimumZ + config_.coreRadius,
        FirstPersonTuning::kBattlefieldMaximumZ - config_.coreRadius);
    if (std::abs(position_.x - unclampedX) > 0.00001f ||
        std::abs(position_.z - unclampedZ) > 0.00001f) {
        return true;
    }

    const float baseZ = FirstPersonTuning::kBattlefieldMinimumZ;
    const float clearance = FirstPersonTuning::kResearchInstituteColliderRadius +
        config_.coreRadius;
    const float offsetX = start.x;
    const float offsetZ = start.z - baseZ;
    const float moveX = position_.x - start.x;
    const float moveZ = position_.z - start.z;
    const float a = moveX * moveX + moveZ * moveZ;
    const float c = offsetX * offsetX + offsetZ * offsetZ -
        clearance * clearance;
    if (a <= 0.00001f || c < 0.0f) {
        return false;
    }
    const float b = 2.0f * (offsetX * moveX + offsetZ * moveZ);
    const float discriminant = b * b - 4.0f * a * c;
    if (discriminant < 0.0f) {
        return false;
    }
    const float entry = (-b - std::sqrt(discriminant)) / (2.0f * a);
    if (entry < 0.0f || entry > 1.0f) {
        return false;
    }
    position_.x = start.x + moveX * entry;
    position_.z = start.z + moveZ * entry;
    return true;
}

void BattleBoss::UpdateDefeat(float seconds) {
    defeatPhaseTime_ += seconds;
    if (defeatPhase_ == DefeatPhase::Pause) {
        if (defeatPhaseTime_ >= config_.defeatPauseDuration) {
            mode_ = BossMode::Defeated;
        }
        return;
    }

    defeatVelocity_.y -= config_.defeatGravity * seconds;
    position_.x += defeatVelocity_.x * seconds;
    position_.y += defeatVelocity_.y * seconds;
    position_.z += defeatVelocity_.z * seconds;
    const float minimumX = -24.0f + config_.coreRadius;
    const float maximumX = 24.0f - config_.coreRadius;
    const float minimumZ = -16.0f + config_.coreRadius;
    const float maximumZ = 42.0f - config_.coreRadius;
    if (position_.x < minimumX) {
        position_.x = minimumX;
        defeatVelocity_.x = std::abs(defeatVelocity_.x) *
            config_.defeatWallBounceRetention;
    } else if (position_.x > maximumX) {
        position_.x = maximumX;
        defeatVelocity_.x = -std::abs(defeatVelocity_.x) *
            config_.defeatWallBounceRetention;
    }
    if (position_.z < minimumZ) {
        position_.z = minimumZ;
        defeatVelocity_.z = std::abs(defeatVelocity_.z) *
            config_.defeatWallBounceRetention;
    } else if (position_.z > maximumZ) {
        position_.z = maximumZ;
        defeatVelocity_.z = -std::abs(defeatVelocity_.z) *
            config_.defeatWallBounceRetention;
    }
    if (position_.y <= config_.coreRadius) {
        position_.y = config_.coreRadius;
        defeatPhaseTime_ = 0.0f;
        if (defeatPhase_ == DefeatPhase::Airborne) {
            defeatPhase_ = DefeatPhase::Bounce;
            defeatVelocity_.y = config_.defeatGroundBounceSpeed;
            defeatVelocity_.x *= config_.defeatGroundHorizontalRetention;
            defeatVelocity_.z *= config_.defeatGroundHorizontalRetention;
        } else {
            defeatPhase_ = DefeatPhase::Pause;
            defeatVelocity_ = {};
        }
    }
    defeatRotation_.x += config_.defeatRotationXSpeed * seconds;
    defeatRotation_.y += config_.defeatRotationYSpeed * seconds;
    defeatRotation_.z += config_.defeatRotationZSpeed * seconds;
    defeatSmallExplosionTime_ += seconds;
    if (defeatSmallExplosionTime_ >=
            config_.defeatSmallExplosionInterval) {
        defeatSmallExplosionTime_ -= config_.defeatSmallExplosionInterval;
        const float angle = static_cast<float>(defeatSmallExplosionCount_) *
            2.4f;
        const float verticalOffset = static_cast<float>(
            defeatSmallExplosionCount_ % 3u) - 1.0f;
        defeatSmallExplosionPosition_ = {
            position_.x + std::cos(angle) * 2.2f,
            std::max(0.3f, position_.y + verticalOffset * 1.1f),
            position_.z + std::sin(angle) * 2.2f};
        ++defeatSmallExplosionCount_;
        emittedDefeatSmallExplosionThisStep_ = true;
    }
}
