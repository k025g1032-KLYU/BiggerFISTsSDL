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
}

BattleBoss::BattleBoss(BossCombatConfig config) : config_(config) {
    Reset({0.0f, config_.entranceSpawnHeight, 34.5f});
}

void BattleBoss::Reset(const Vector3& position) {
    position_ = position;
    previousPosition_ = position;
    mode_ = BossMode::Entrance;
    modeTime_ = 0.0f;
    entranceRecovering_ = false;
    entranceRecoveryTime_ = 0.0f;
    attackTimer_ = 0.0f;
    hitPoints_ = config_.coreHitPoints;
    behaviorRandomState_ = 0x6d2b79f5u;
    lastIdleAction_ = BossMode::Entrance;
    boxerCooldown_ = 0.0f;
    chargeCooldown_ = 0.0f;
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
    boxerEntryDashDistance_ = 0.0f;
    usedBoxerEntryDash_ = false;
    boxerEntryDirection_ = {};
    needleDashComplete_ = false;
    needleFirstHit_ = false;
    needleFirstAttackId_ = 0;
    needleImpactTime_ = 0.0f;
    needleImpactAttackId_ = 0;
    nextSpikeSlot_ = 0;
    nextCubeSlot_ = 0;
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
    visualYaw_ = 0.0f;
}

BossVisualPose BattleBoss::GetVisualPose() const {
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
    pose.cones = cruise;
    pose.crystalAngle = crystalAngle_;
    if (mode_ == BossMode::Entrance) {
        const float recoveryRatio = entranceRecovering_
            ? std::clamp(entranceRecoveryTime_ /
                config_.entranceFlipDuration, 0.0f, 1.0f)
            : 0.0f;
        pose.rootRotation.x = -pi * 0.5f * (1.0f - recoveryRatio);
    } else if (mode_ == BossMode::Charge) {
        pose.cones = {};
        pose.coreRotation.x = pi * 0.5f;
    } else if (mode_ == BossMode::NeedleCombo || mode_ == BossMode::Boxer) {
        pose.cones = needle;
        if (activeAttackId_ != 0) {
            const std::size_t index = mode_ == BossMode::NeedleCombo
                ? static_cast<std::size_t>(std::clamp(attackCount_ - 1, 0, 2))
                : 0;
            const Vector3 direction = DirectionTo(position_, strikePrevious_);
            const float distance = std::hypot(strikePrevious_.x - position_.x,
                strikePrevious_.z - position_.z);
            const float localAngle = std::atan2(direction.x, direction.z) - visualYaw_;
            pose.cones[index].rootTranslation = {
                std::sin(localAngle) * distance,
                strikePrevious_.y - position_.y,
                std::cos(localAngle) * distance};
            pose.cones[index].translation = {};
        }
    } else if (mode_ == BossMode::SpreadCounter) {
        pose.cones = spread;
        const float spinTime = modeTime_ - config_.spreadWindupDuration;
        if (spinTime > 0.0f && spinTime < config_.spreadSpinDuration) {
            pose.spreadSpin = std::clamp(spinTime /
                config_.spreadSpinDuration, 0.0f, 1.0f) *
                4.0f * pi;
        }
    } else if (mode_ == BossMode::FullBarrage) {
        const float height = position_.y - config_.hoverHeight;
        pose.rootRotation.x = pi * 0.5f *
            std::clamp(height / 12.0f, 0.0f, 1.0f);
    } else if (mode_ == BossMode::Defeating || mode_ == BossMode::Defeated) {
        pose.rootRotation.x = modeTime_ * 2.2f;
        pose.rootRotation.y += modeTime_ * 3.0f;
        pose.rootRotation.z = modeTime_ * 1.2f;
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

void BattleBoss::BeginBehavior(BossMode mode) {
    if (mode_ == BossMode::Defeating || mode_ == BossMode::Defeated) {
        return;
    }
    mode_ = mode;
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
        usedBoxerEntryDash_ = false;
    } else if (mode == BossMode::NeedleCombo) {
        needleDashComplete_ = false;
        needleFirstHit_ = false;
        needleFirstAttackId_ = 0;
        needleImpactTime_ = 0.0f;
        needleImpactAttackId_ = 0;
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
    return spike ? begin(spikeSlots_, nextSpikeSlot_)
        : begin(cubeSlots_, nextCubeSlot_);
}

void BattleBoss::UpdateCrystals(float seconds,
    std::vector<BossAttackEvent>& attacks) {
    crystalAngle_ += config_.crystalHaloRotationSpeed * seconds;
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
                    attacks.push_back({type, slot.position});
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
    if (hitPoints_ == 0) {
        mode_ = BossMode::Defeating;
        modeTime_ = 0.0f;
        attackRegistered_ = false;
    }
    return true;
}

bool BattleBoss::RegisterNeedleStrikeHit(std::uint64_t attackId) {
    if (mode_ == BossMode::NeedleCombo &&
        attackId == needleFirstAttackId_) {
        needleFirstHit_ = true;
        return true;
    }
    return false;
}

std::vector<BossAttackEvent> BattleBoss::Update(float seconds,
    const Vector3& playerPosition, const BossPlayerThreat& threat) {
    std::vector<BossAttackEvent> attacks;
    beganRocketDodgeThisStep_ = false;
    if (!std::isfinite(seconds) || seconds < 0.0f ||
        mode_ == BossMode::Defeated) {
        return attacks;
    }
    previousPosition_ = position_;
    modeTime_ += seconds;
    if ((mode_ != BossMode::Entrance || entranceRecovering_) &&
        mode_ != BossMode::Defeating) {
        const Vector3 towardPlayer = DirectionTo(position_, playerPosition);
        const float targetYaw = std::atan2(towardPlayer.x,
            towardPlayer.z) + std::numbers::pi_v<float>;
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
        if (modeTime_ < config_.chargePrepareDuration) {
            chargeDirection_ = DirectionTo(position_, playerPosition);
        } else if (modeTime_ < config_.chargePrepareDuration + 0.6f) {
            position_.x += chargeDirection_.x * config_.chargeSpeed * seconds;
            position_.z += chargeDirection_.z * config_.chargeSpeed * seconds;
            if (!attackRegistered_ &&
                Collision::SweepSphereAgainstMovingSphere(previousPosition_,
                    position_, config_.coreRadius,
                    {playerPosition.x, playerPosition.y +
                        FirstPersonTuning::kPlayerColliderCenterHeight,
                        playerPosition.z},
                    {playerPosition.x, playerPosition.y +
                        FirstPersonTuning::kPlayerColliderCenterHeight,
                        playerPosition.z},
                    FirstPersonTuning::kPlayerColliderRadius).didHit) {
                attacks.push_back({BossAttackType::ChargeImpact, position_});
                attackRegistered_ = true;
            }
        } else if (modeTime_ >= config_.chargePrepareDuration + 1.0f) {
            BeginBehavior(BossMode::Idle);
        }
        break;
    case BossMode::Boxer: {
        if (UpdateRocketDodge(seconds, playerPosition, threat)) {
            break;
        }
        const float distance = std::hypot(playerPosition.x - position_.x,
            playerPosition.z - position_.z);
        const Vector3 direction = DirectionTo(position_, playerPosition);
        boxerPhaseTime_ += seconds;
        const auto beginStrike = [&](const Vector3& strikeDirection) {
            boxerPhase_ = BoxerPhase::WindingUp;
            boxerPhaseTime_ = 0.0f;
            strikeStart_ = {position_.x + strikeDirection.x * 2.2f,
                position_.y, position_.z + strikeDirection.z * 2.2f};
            strikeEnd_ = {position_.x + strikeDirection.x *
                (config_.boxerStrikeOffset + config_.needleTipOffset),
                position_.y, position_.z + strikeDirection.z *
                (config_.boxerStrikeOffset + config_.needleTipOffset)};
            strikePrevious_ = strikeStart_;
        };
        if (boxerPhase_ == BoxerPhase::Approaching ||
            boxerPhase_ == BoxerPhase::Orbiting) {
            if (boxerPhase_ == BoxerPhase::Approaching &&
                !usedBoxerEntryDash_ &&
                distance > config_.boxerDesiredDistance + 0.8f &&
                distance <= config_.boxerEntryMaximumDistance) {
                boxerPhase_ = BoxerPhase::EntryWindup;
                boxerPhaseTime_ = 0.0f;
                break;
            }
            attackTimer_ = std::min(config_.boxerStrikeInterval,
                attackTimer_ + seconds);
            if (distance > config_.boxerDesiredDistance) {
                position_.x += direction.x * config_.boxerApproachSpeed * seconds;
                position_.z += direction.z * config_.boxerApproachSpeed * seconds;
                boxerPhase_ = BoxerPhase::Approaching;
            } else {
                boxerPhase_ = BoxerPhase::Orbiting;
                const Vector3 tangent{-direction.z * idleStrafeSide_, 0.0f,
                    direction.x * idleStrafeSide_};
                const float radial = std::clamp(distance -
                    config_.boxerDesiredDistance, -1.0f, 1.0f);
                float moveX = tangent.x + direction.x * radial;
                float moveZ = tangent.z + direction.z * radial;
                if (attackTimer_ >= config_.boxerStrikeInterval &&
                    distance > config_.boxerStrikeStartDistance) {
                    moveX = direction.x;
                    moveZ = direction.z;
                }
                const float length = std::hypot(moveX, moveZ);
                if (length > 0.00001f) {
                    const float speed = attackTimer_ >=
                        config_.boxerStrikeInterval
                        ? config_.boxerClosingStepSpeed
                        : config_.boxerOrbitSpeed;
                    position_.x += moveX / length * speed * seconds;
                    position_.z += moveZ / length * speed * seconds;
                }
            }
            if (attackTimer_ >= config_.boxerStrikeInterval &&
                std::hypot(playerPosition.x - position_.x,
                    playerPosition.z - position_.z) <=
                    config_.boxerStrikeStartDistance) {
                beginStrike(DirectionTo(position_, playerPosition));
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
        } else if (boxerPhase_ == BoxerPhase::WindingUp &&
            boxerPhaseTime_ >= config_.boxerWindupDuration) {
            boxerPhase_ = BoxerPhase::Holding;
            boxerPhaseTime_ = 0.0f;
        } else if (boxerPhase_ == BoxerPhase::Holding &&
            boxerPhaseTime_ >= config_.boxerWindupHoldDuration) {
            boxerPhase_ = BoxerPhase::Striking;
            boxerPhaseTime_ = 0.0f;
            activeAttackId_ = nextAttackId_++;
            strikeTime_ = 0.0f;
        } else if (boxerPhase_ == BoxerPhase::Striking) {
            strikeTime_ += seconds;
            const float progress = std::clamp(strikeTime_ /
                config_.boxerStrikeDuration, 0.0f, 1.0f);
            const Vector3 tip{strikeStart_.x +
                (strikeEnd_.x - strikeStart_.x) * progress,
                strikeStart_.y + (strikeEnd_.y - strikeStart_.y) * progress,
                strikeStart_.z + (strikeEnd_.z - strikeStart_.z) * progress};
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
        }
        if (modeTime_ >= 4.0f &&
            (boxerPhase_ == BoxerPhase::Approaching ||
                boxerPhase_ == BoxerPhase::Orbiting)) {
            BeginBehavior(BossMode::Idle);
        }
        break;
    }
    case BossMode::NeedleCombo: {
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
                break;
            }
            const float distance = std::hypot(playerPosition.x - position_.x,
                playerPosition.z - position_.z);
            if (distance > config_.needleFrontDistance) {
                const Vector3 direction = DirectionTo(position_,
                    playerPosition);
                const float dashDistance = std::min(
                    config_.needleDashSpeed * seconds,
                    distance - config_.needleFrontDistance);
                position_.x += direction.x * dashDistance;
                position_.z += direction.z * dashDistance;
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
            const Vector3 direction = DirectionTo(position_, playerPosition);
            activeAttackId_ = nextAttackId_++;
            if (attackCount_ == 0) {
                needleFirstAttackId_ = activeAttackId_;
            }
            strikeTime_ = 0.0f;
            const float lateral = static_cast<float>(attackCount_ - 1) * 1.4f;
            strikeStart_ = {position_.x + direction.x * 2.2f -
                direction.z * lateral, position_.y,
                position_.z + direction.z * 2.2f + direction.x * lateral};
            strikeEnd_ = {position_.x + direction.x *
                (config_.needleTipOffset + 4.75f) - direction.z * lateral,
                position_.y, position_.z + direction.z *
                (config_.needleTipOffset + 4.75f) + direction.x * lateral};
            strikePrevious_ = strikeStart_;
            ++attackCount_;
        }
        if (activeAttackId_ != 0) {
            strikeTime_ += seconds;
            const float progress = std::clamp(strikeTime_ /
                config_.needleStrikeDuration, 0.0f, 1.0f);
            const Vector3 tip{strikeStart_.x +
                (strikeEnd_.x - strikeStart_.x) * progress,
                strikeStart_.y + (strikeEnd_.y - strikeStart_.y) * progress,
                strikeStart_.z + (strikeEnd_.z - strikeStart_.z) * progress};
            attacks.push_back({BossAttackType::NeedleStrike, tip,
                strikePrevious_, config_.needleStrikeRadius, activeAttackId_});
            strikePrevious_ = tip;
            if (progress >= 1.0f) {
                activeAttackId_ = 0;
                if (attackCount_ == 3) {
                    needleImpactAttackId_ = nextAttackId_++;
                }
            }
        }
        if (needleImpactAttackId_ != 0 &&
            needleImpactTime_ < 0.12f) {
            needleImpactTime_ += seconds;
            attacks.push_back({BossAttackType::NeedleImpact,
                strikePrevious_, strikePrevious_,
                config_.needleStrikeRadius, needleImpactAttackId_});
        }
        if (activeTime >= 3.0f * strikePeriod + 2.0f) {
            BeginBehavior(BossMode::Idle);
        }
        break;
    }
    case BossMode::FullBarrage:
        if (attackCount_ < 10 || modeTime_ < 4.0f) {
            position_.y = std::min(config_.hoverHeight + 12.0f,
                position_.y + 30.0f * seconds);
        }
        attackTimer_ += seconds;
        if (attackTimer_ >= config_.barrageShotInterval && attackCount_ < 10) {
            attackTimer_ -= config_.barrageShotInterval;
            if (!BeginCrystalLaunch(attackCount_ % 2 == 0)) {
                BeginCrystalLaunch(attackCount_ % 2 != 0);
            }
            ++attackCount_;
        }
        if (attackCount_ >= 10 && modeTime_ >= 4.0f) {
            position_.y = std::max(config_.hoverHeight,
                position_.y - 30.0f * seconds);
            if (position_.y <= config_.hoverHeight) {
                fullBarrageDamageLocked_ = false;
                BeginBehavior(BossMode::Idle);
            }
        }
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
        position_.y = std::max(config_.coreRadius,
            position_.y - 6.0f * seconds);
        if (modeTime_ >= config_.defeatDuration) {
            mode_ = BossMode::Defeated;
        }
        break;
    case BossMode::Defeated:
        break;
    }
    UpdateCrystals(seconds, attacks);
    position_.x = std::clamp(position_.x, -22.0f, 22.0f);
    position_.z = std::clamp(position_.z, -14.0f, 40.0f);
    const float fromBaseX = position_.x;
    const float fromBaseZ = position_.z + 16.0f;
    const float fromBaseLength = std::hypot(fromBaseX, fromBaseZ);
    const float baseClearance = 7.0f + config_.coreRadius;
    if (fromBaseLength > 0.00001f &&
        fromBaseLength < baseClearance) {
        position_.x = fromBaseX * baseClearance / fromBaseLength;
        position_.z = -16.0f + fromBaseZ * baseClearance / fromBaseLength;
    }
    return attacks;
}
