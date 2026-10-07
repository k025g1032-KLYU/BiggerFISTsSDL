#include "Gameplay/Combat/BattleBoss.h"

#include "Collision/Collision.h"
#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <cmath>

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
    const float minimumX = -24.0f + config_.coreRadius + 2.0f;
    const float maximumX = 24.0f - config_.coreRadius - 2.0f;
    const float minimumZ = -16.0f + config_.coreRadius + 2.0f;
    const float maximumZ = 42.0f - config_.coreRadius - 2.0f;
    const float centerX = (minimumX + maximumX) * 0.5f;
    const float centerZ = (minimumZ + maximumZ) * 0.5f;
    const float halfWidth = (maximumX - minimumX) * 0.5f;
    const float halfDepth = (maximumZ - minimumZ) * 0.5f;
    const float xSign = playerPosition.x < 0.0f ? 1.0f : -1.0f;
    const float zSign = playerPosition.z < 13.0f ? 1.0f : -1.0f;
    constexpr float xFactors[3]{0.55f, 0.78f, 0.38f};
    constexpr float zFactors[3]{0.55f, 0.38f, 0.78f};
    float bestScore = -1000000.0f;
    int bestCandidate = -1;
    for (int index = 0; index < 3; ++index) {
        const Vector3 candidate{centerX + xSign * halfWidth * xFactors[index],
            config_.hoverHeight,
            centerZ + zSign * halfDepth * zFactors[index]};
        if (std::hypot(candidate.x, candidate.z + 16.0f) <
            7.0f + config_.coreRadius + 2.0f) {
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
        if (relocationDistance < 8.0f) {
            score -= (8.0f - relocationDistance) * 50.0f;
        }
        if (index == lastCruiseCandidate_) {
            score -= 1000.0f;
        }
        if (score > bestScore) {
            bestScore = score;
            bestCandidate = index;
            cruiseTarget_ = candidate;
        }
    }
    lastCruiseCandidate_ = bestCandidate;
    if (bestCandidate < 0) {
        cruiseTarget_ = position_;
    }
}

void BattleBoss::UpdateCruise(float seconds) {
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
            cruisePhase_ = CruisePhase::Landing;
        }
        break;
    }
    case CruisePhase::Landing:
        position_.y = std::max(config_.hoverHeight,
            position_.y - config_.cruiseLandingSpeed * seconds);
        if (position_.y <= config_.hoverHeight) {
            BeginBehavior(BossMode::Idle);
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
        position_.y = std::max(config_.hoverHeight,
            position_.y - config_.entranceDropSpeed * seconds);
        if (position_.y <= config_.hoverHeight) {
            BeginBehavior(BossMode::Idle);
        }
        break;
    case BossMode::Cruise:
        UpdateCruise(seconds);
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
            attacks.push_back({BossAttackType::CrystalSpike, position_});
            crystalSpikeTimer_ = config_.crystalSpikeInterval;
        }
        if (crystalCubeTimer_ <= 0.0f) {
            attacks.push_back({BossAttackType::CrystalCube, position_});
            crystalCubeTimer_ = config_.crystalCubeInterval;
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
        if (distance > 7.8f) {
            position_.x += direction.x * config_.boxerApproachSpeed * seconds;
            position_.z += direction.z * config_.boxerApproachSpeed * seconds;
        }
        attackTimer_ += seconds;
        if (attackTimer_ >= config_.boxerStrikeInterval) {
            attackTimer_ -= config_.boxerStrikeInterval;
            if (distance < 6.8f) {
                attacks.push_back({BossAttackType::BoxerStrike, position_});
            }
        }
        if (modeTime_ >= 4.0f) {
            BeginBehavior(BossMode::Idle);
        }
        break;
    }
    case BossMode::NeedleCombo: {
        const float strikePeriod = config_.needleStrikeDuration +
            config_.needleStrikePauseDuration;
        const float activeTime = modeTime_ - config_.needleWindupDuration;
        if (activeTime >= 0.0f && attackCount_ < 3 &&
            activeTime >= attackCount_ * strikePeriod) {
            if (std::hypot(playerPosition.x - position_.x,
                    playerPosition.z - position_.z) < 6.0f) {
                attacks.push_back({BossAttackType::NeedleStrike, position_});
            }
            ++attackCount_;
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
            attacks.push_back({attackCount_ % 2 == 0
                ? BossAttackType::CrystalSpike : BossAttackType::CrystalCube,
                position_});
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
