#include "Gameplay/Player/PunchArm.h"

#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace {
constexpr float kEpsilon = 0.00001f;

Vector3 Add(const Vector3& a, const Vector3& b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
}

Vector3 Subtract(const Vector3& a, const Vector3& b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
}

Vector3 Multiply(const Vector3& value, float factor) {
    return {value.x * factor, value.y * factor, value.z * factor};
}

float Length(const Vector3& value) {
    return std::sqrt(value.x * value.x + value.y * value.y + value.z * value.z);
}

Vector3 Lerp(const Vector3& a, const Vector3& b, float progress) {
    return Add(a, Multiply(Subtract(b, a), progress));
}

float SmoothStep(float progress) {
    const float clamped = std::clamp(progress, 0.0f, 1.0f);
    return clamped * clamped * (3.0f - 2.0f * clamped);
}

float NormalizeAngle(float angle) {
    return std::remainder(angle, 2.0f * std::numbers::pi_v<float>);
}

float MoveTowards(float current, float target, float maximumDelta) {
    const float difference = target - current;
    if (std::abs(difference) <= maximumDelta) {
        return target;
    }
    return current + std::copysign(maximumDelta, difference);
}

float MoveTowardsAngle(float current, float target, float maximumDelta) {
    const float difference = NormalizeAngle(target - current);
    return NormalizeAngle(current + std::clamp(difference, -maximumDelta, maximumDelta));
}

float FlightYaw(const Vector3& direction, float fallbackYaw) {
    if (direction.x * direction.x + direction.z * direction.z <= kEpsilon * kEpsilon) {
        return fallbackYaw;
    }
    return std::atan2(direction.x, direction.z);
}

float FlightPitch(const Vector3& direction) {
    return -std::asin(std::clamp(direction.y, -1.0f, 1.0f));
}
}

PunchArm::PunchArm(PunchArmSide side, PlayerCombatConfig config)
    : side_(side), config_(config) {
}

void PunchArm::Reset(const Vector3& playerPosition, float bodyYaw) {
    state_ = PunchArmState::Ready;
    attackType_ = PunchArmAttackType::None;
    chargeTimer_ = 0.0f;
    chargedVisualShakeTimer_ = 0.0f;
    stateTimer_ = 0.0f;
    punchDirection_ = { std::sin(bodyYaw), 0.0f, std::cos(bodyYaw) };
    fistPosition_ = CalculateHomePosition(playerPosition, bodyYaw);
    previousFistPosition_ = fistPosition_;
    punchDamageActiveThisFrame_ = false;
    rocketDamageActiveThisFrame_ = false;
    didLaunchRocketThisFrame_ = false;
    didBeginPunchThisFrame_ = false;
    didReconnectThisFrame_ = false;
    rocketTravelDistance_ = 0.0f;
    rocketMaximumRange_ = config_.rocketMaximumRange;
    rocketBaseAvoidanceDistance_ = std::numeric_limits<float>::infinity();
    rocketPosePitch_ = 0.0f;
    rocketPoseYaw_ = bodyYaw;
    returnPhaseStartPitch_ = 0.0f;
    returnPhaseStartYaw_ = bodyYaw;
    returnAscentDistance_ = 0.0f;
    returnPhaseStartPosition_ = fistPosition_;
    rocketReturnPhase_ = RocketReturnPhase::TurnUp;
}

float PunchArm::GetChargeRatio() const {
    return std::clamp(chargeTimer_ / config_.chargeTime, 0.0f, 1.0f);
}

Vector3 PunchArm::GetChargedVisualOffset() const {
    if (state_ != PunchArmState::Charged) {
        return {};
    }
    const float sidePhase = side_ == PunchArmSide::Left ? 0.0f :
        std::numbers::pi_v<float>;
    const float phase = chargedVisualShakeTimer_ * config_.chargedFistShakeFrequency;
    const float amplitude = config_.chargedFistShakeAmplitude;
    return {std::sin(phase + sidePhase) * amplitude,
        std::sin(phase * 1.71f + sidePhase + 0.8f) * amplitude * 0.65f,
        std::sin(phase * 1.29f + sidePhase + 1.6f) * amplitude * 0.8f};
}

Vector3 PunchArm::CalculateHomePosition(const Vector3& playerPosition, float bodyYaw) const {
    const float sideOffset = (side_ == PunchArmSide::Left ? -1.0f : 1.0f) *
        FirstPersonTuning::kShoulderHalfWidth;
    const float forwardOffset = FirstPersonTuning::kReadyFistForwardOffset;
    const float sine = std::sin(bodyYaw);
    const float cosine = std::cos(bodyYaw);
    return {
        playerPosition.x + cosine * sideOffset + sine * forwardOffset,
        playerPosition.y + FirstPersonTuning::kShoulderHeight,
        playerPosition.z - sine * sideOffset + cosine * forwardOffset
    };
}

Vector3 PunchArm::CalculateRocketDockPosition(const Vector3& playerPosition,
    float bodyYaw) const {
    const Vector3 home = CalculateHomePosition(playerPosition, bodyYaw);
    const float offset = FirstPersonTuning::kRocketChargeFinalForwardOffset -
        FirstPersonTuning::kReadyFistForwardOffset;
    return Add(home, {std::sin(bodyYaw) * offset, 0.0f, std::cos(bodyYaw) * offset});
}

RocketPreviewTrajectory PunchArm::GetRocketPreviewTrajectory(
    const Vector3& playerPosition, float bodyYaw) const {
    if (state_ != PunchArmState::Ready &&
        !((state_ == PunchArmState::Charging || state_ == PunchArmState::Charged) &&
            attackType_ == PunchArmAttackType::RocketPunch)) {
        return {};
    }
    const Vector3 start = CalculateRocketDockPosition(playerPosition, bodyYaw);
    const Vector3 direction{std::sin(bodyYaw), 0.0f, std::cos(bodyYaw)};
    return {start, direction, CalculateRocketMaximumRange(start, direction)};
}

Vector3 PunchArm::CalculateChargePosition(const Vector3& playerPosition,
    float bodyYaw) const {
    const Vector3 home = CalculateHomePosition(playerPosition, bodyYaw);
    if (attackType_ == PunchArmAttackType::RocketPunch) {
        return Lerp(home, CalculateRocketDockPosition(playerPosition, bodyYaw), GetChargeRatio());
    }
    const float retraction = FirstPersonTuning::kPunchChargeRetractDistance * GetChargeRatio();
    return Subtract(home, {std::sin(bodyYaw) * retraction, 0.0f,
        std::cos(bodyYaw) * retraction});
}

Vector3 PunchArm::CalculateReturnEntryPosition(const Vector3& playerPosition,
    float bodyYaw) const {
    return Add(CalculateHomePosition(playerPosition, bodyYaw),
        {0.0f, config_.rocketReturnEntryHeightAboveShoulder, 0.0f});
}

float PunchArm::CalculateRocketMaximumRange(const Vector3& start,
    const Vector3& direction) const {
    float entryDistance = -std::numeric_limits<float>::infinity();
    float exitDistance = std::numeric_limits<float>::infinity();
    const auto clipAxis = [&](float startValue, float directionValue,
        float minimum, float maximum) {
        if (std::abs(directionValue) <= kEpsilon) {
            return startValue >= minimum && startValue <= maximum;
        }
        float nearDistance = (minimum - startValue) / directionValue;
        float farDistance = (maximum - startValue) / directionValue;
        if (nearDistance > farDistance) {
            std::swap(nearDistance, farDistance);
        }
        entryDistance = std::max(entryDistance, nearDistance);
        exitDistance = std::min(exitDistance, farDistance);
        return entryDistance <= exitDistance;
    };
    if (!clipAxis(start.x, direction.x,
            FirstPersonTuning::kBattlefieldMinimumX,
            FirstPersonTuning::kBattlefieldMaximumX) ||
        !clipAxis(start.z, direction.z,
            FirstPersonTuning::kBattlefieldMinimumZ,
            FirstPersonTuning::kBattlefieldMaximumZ)) {
        return config_.rocketBattlefieldExitMargin;
    }
    return std::clamp(exitDistance + config_.rocketBattlefieldExitMargin,
        0.0f, config_.rocketMaximumRange);
}

float PunchArm::CalculateRocketBaseAvoidanceDistance(const Vector3& start,
    const Vector3& direction) const {
    const Vector3 baseCenter{0.0f, 0.0f, FirstPersonTuning::kBattlefieldMinimumZ};
    const Vector3 fromBase = Subtract(start, baseCenter);
    const float radius = FirstPersonTuning::kResearchInstituteColliderRadius +
        FirstPersonTuning::kFistRadius + config_.rocketBaseAvoidanceMargin;
    const float projection = DotVectors(fromBase, direction);
    const float perpendicularDistanceSquared = DotVectors(fromBase, fromBase) -
        projection * projection;
    if (perpendicularDistanceSquared > radius * radius) {
        return std::numeric_limits<float>::infinity();
    }
    const float halfChord = std::sqrt(std::max(0.0f,
        radius * radius - perpendicularDistanceSquared));
    const float exitDistance = -projection + halfChord;
    if (exitDistance < 0.0f) {
        return std::numeric_limits<float>::infinity();
    }
    return std::max(0.0f, -projection - halfChord);
}

void PunchArm::BeginPunch(const Vector3& playerPosition, float bodyYaw) {
    didBeginPunchThisFrame_ = true;
    const Vector3 home = CalculateHomePosition(playerPosition, bodyYaw);
    const float sine = std::sin(bodyYaw);
    const float cosine = std::cos(bodyYaw);
    const Vector3 target{
        playerPosition.x + sine * FirstPersonTuning::kPunchConvergenceTargetForwardDistance,
        playerPosition.y + FirstPersonTuning::kShoulderHeight,
        playerPosition.z + cosine * FirstPersonTuning::kPunchConvergenceTargetForwardDistance
    };
    const float differenceX = target.x - home.x;
    const float differenceZ = target.z - home.z;
    const float length = std::hypot(differenceX, differenceZ);
    punchDirection_ = length > 0.00001f
        ? Vector3{ differenceX / length, 0.0f, differenceZ / length }
        : Vector3{ sine, 0.0f, cosine };
    stateTimer_ = 0.0f;
    state_ = PunchArmState::Punching;
}

void PunchArm::BeginRocket(const Vector3& playerPosition, float bodyYaw) {
    punchDirection_ = {std::sin(bodyYaw), 0.0f, std::cos(bodyYaw)};
    fistPosition_ = CalculateRocketDockPosition(playerPosition, bodyYaw);
    rocketPosePitch_ = 0.0f;
    rocketPoseYaw_ = bodyYaw;
    rocketTravelDistance_ = 0.0f;
    rocketMaximumRange_ = CalculateRocketMaximumRange(fistPosition_, punchDirection_);
    rocketBaseAvoidanceDistance_ = CalculateRocketBaseAvoidanceDistance(
        fistPosition_, punchDirection_);
    stateTimer_ = 0.0f;
    state_ = PunchArmState::RocketFlying;
    didLaunchRocketThisFrame_ = true;
}

void PunchArm::BeginRocketReturn(bool avoidBase) {
    state_ = avoidBase ? PunchArmState::RocketEvacuating : PunchArmState::Returning;
    returnAscentDistance_ = avoidBase ? config_.rocketBaseEvacuationDistance :
        config_.rocketReturnAscentDistance;
    rocketReturnPhase_ = RocketReturnPhase::TurnUp;
    stateTimer_ = 0.0f;
    returnPhaseStartPosition_ = fistPosition_;
    returnPhaseStartPitch_ = rocketPosePitch_;
    returnPhaseStartYaw_ = rocketPoseYaw_;
}

void PunchArm::AdvanceRocketReturnPhase(RocketReturnPhase phase) {
    rocketReturnPhase_ = phase;
    stateTimer_ = 0.0f;
    returnPhaseStartPosition_ = fistPosition_;
    returnPhaseStartPitch_ = rocketPosePitch_;
    returnPhaseStartYaw_ = rocketPoseYaw_;
    if (phase == RocketReturnPhase::TurnHome) {
        state_ = PunchArmState::Returning;
    }
}

void PunchArm::UpdateRocketReturn(float deltaTime,
    const Vector3& playerPosition, float bodyYaw) {
    switch (rocketReturnPhase_) {
    case RocketReturnPhase::TurnUp: {
        stateTimer_ += deltaTime;
        const float progress = SmoothStep(stateTimer_ /
            config_.rocketReturnTurnUpDuration);
        rocketPosePitch_ = returnPhaseStartPitch_ +
            (-std::numbers::pi_v<float> * 0.5f - returnPhaseStartPitch_) * progress;
        const float ascent = std::min(returnAscentDistance_,
            config_.rocketReturnTurnUpAscentDistance);
        fistPosition_.y = returnPhaseStartPosition_.y + ascent * progress;
        if (stateTimer_ >= config_.rocketReturnTurnUpDuration) {
            AdvanceRocketReturnPhase(RocketReturnPhase::Ascend);
        }
        break;
    }
    case RocketReturnPhase::Ascend: {
        const float alreadyAscended = fistPosition_.y - returnPhaseStartPosition_.y;
        const float turnAscent = std::min(returnAscentDistance_,
            config_.rocketReturnTurnUpAscentDistance);
        const float remaining = std::max(0.0f,
            returnAscentDistance_ - turnAscent - alreadyAscended);
        const float step = std::min(config_.rocketReturnAscentSpeed * deltaTime,
            remaining);
        fistPosition_.y += step;
        if (step >= remaining - kEpsilon) {
            AdvanceRocketReturnPhase(RocketReturnPhase::TurnHome);
        }
        break;
    }
    case RocketReturnPhase::TurnHome: {
        stateTimer_ += deltaTime;
        const float progress = SmoothStep(stateTimer_ /
            config_.rocketReturnTurnHomeDuration);
        const Vector3 toEntry = Subtract(
            CalculateReturnEntryPosition(playerPosition, bodyYaw), fistPosition_);
        const float distance = Length(toEntry);
        if (distance > kEpsilon) {
            const Vector3 direction = Multiply(toEntry, 1.0f / distance);
            const float targetYaw = FlightYaw(direction, returnPhaseStartYaw_);
            rocketPoseYaw_ = NormalizeAngle(returnPhaseStartYaw_ +
                NormalizeAngle(targetYaw - returnPhaseStartYaw_) * progress);
            rocketPosePitch_ = returnPhaseStartPitch_ +
                (FlightPitch(direction) - returnPhaseStartPitch_) * progress;
        }
        if (stateTimer_ >= config_.rocketReturnTurnHomeDuration) {
            AdvanceRocketReturnPhase(RocketReturnPhase::FlyHome);
        }
        break;
    }
    case RocketReturnPhase::FlyHome: {
        const Vector3 toEntry = Subtract(
            CalculateReturnEntryPosition(playerPosition, bodyYaw), fistPosition_);
        const float distance = Length(toEntry);
        if (distance <= kEpsilon) {
            AdvanceRocketReturnPhase(RocketReturnPhase::Align);
            break;
        }
        const Vector3 direction = Multiply(toEntry, 1.0f / distance);
        const float step = std::min(config_.rocketReturnFlightSpeed * deltaTime,
            distance);
        fistPosition_ = Add(fistPosition_, Multiply(direction, step));
        const float maximumTurn = config_.rocketReturnTrackingTurnSpeed * deltaTime;
        rocketPoseYaw_ = MoveTowardsAngle(rocketPoseYaw_,
            FlightYaw(direction, rocketPoseYaw_), maximumTurn);
        rocketPosePitch_ = MoveTowards(rocketPosePitch_, FlightPitch(direction), maximumTurn);
        if (step >= distance - kEpsilon) {
            AdvanceRocketReturnPhase(RocketReturnPhase::Align);
        }
        break;
    }
    case RocketReturnPhase::Align: {
        stateTimer_ += deltaTime;
        const float progress = SmoothStep(stateTimer_ /
            config_.rocketReturnAlignDuration);
        fistPosition_ = returnPhaseStartPosition_;
        rocketPosePitch_ = returnPhaseStartPitch_ +
            (-std::numbers::pi_v<float> * 0.5f - returnPhaseStartPitch_) * progress;
        if (stateTimer_ >= config_.rocketReturnAlignDuration) {
            rocketPosePitch_ = -std::numbers::pi_v<float> * 0.5f;
            AdvanceRocketReturnPhase(RocketReturnPhase::Dock);
        }
        break;
    }
    case RocketReturnPhase::Dock: {
        stateTimer_ += deltaTime;
        const float progress = SmoothStep(stateTimer_ /
            config_.rocketReturnDockDuration);
        const Vector3 home = CalculateHomePosition(playerPosition, bodyYaw);
        fistPosition_ = Lerp(returnPhaseStartPosition_, home, progress);
        rocketPosePitch_ = -std::numbers::pi_v<float> * 0.5f;
        if (stateTimer_ >= config_.rocketReturnDockDuration) {
            fistPosition_ = home;
            didReconnectThisFrame_ = true;
            attackType_ = PunchArmAttackType::None;
            state_ = PunchArmState::Recovery;
            stateTimer_ = 0.0f;
        }
        break;
    }
    }
}

void PunchArm::Update(float deltaTime, const Vector3& playerPosition, float bodyYaw,
    const PunchArmInput& input, bool allowNewCharge) {
    previousFistPosition_ = fistPosition_;
    punchDamageActiveThisFrame_ = false;
    rocketDamageActiveThisFrame_ = false;
    didLaunchRocketThisFrame_ = false;
    didBeginPunchThisFrame_ = false;
    didReconnectThisFrame_ = false;
    if (!std::isfinite(deltaTime) || deltaTime < 0.0f ||
        !std::isfinite(bodyYaw) || !std::isfinite(playerPosition.x) ||
        !std::isfinite(playerPosition.y) || !std::isfinite(playerPosition.z)) {
        return;
    }

    const Vector3 home = CalculateHomePosition(playerPosition, bodyYaw);
    switch (state_) {
    case PunchArmState::Ready:
        fistPosition_ = home;
        if (allowNewCharge && (input.punchHeld || input.rocketHeld)) {
            state_ = PunchArmState::Charging;
            attackType_ = input.punchHeld ? PunchArmAttackType::Punch :
                PunchArmAttackType::RocketPunch;
            chargeTimer_ = 0.0f;
            stateTimer_ = 0.0f;
        }
        break;

    case PunchArmState::Charging: {
        const bool held = attackType_ == PunchArmAttackType::Punch ?
            input.punchHeld : input.rocketHeld;
        if (!held) {
            state_ = PunchArmState::Ready;
            attackType_ = PunchArmAttackType::None;
            chargeTimer_ = 0.0f;
            fistPosition_ = home;
            break;
        }
        chargeTimer_ = std::min(chargeTimer_ + deltaTime, config_.chargeTime);
        fistPosition_ = CalculateChargePosition(playerPosition, bodyYaw);
        if (chargeTimer_ >= config_.chargeTime) {
            state_ = PunchArmState::Charged;
        }
        break;
    }

    case PunchArmState::Charged: {
        const bool held = attackType_ == PunchArmAttackType::Punch ?
            input.punchHeld : input.rocketHeld;
        chargeTimer_ = config_.chargeTime;
        fistPosition_ = CalculateChargePosition(playerPosition, bodyYaw);
        if (!held) {
            if (attackType_ == PunchArmAttackType::Punch) {
                BeginPunch(playerPosition, bodyYaw);
            } else {
                BeginRocket(playerPosition, bodyYaw);
            }
        }
        break;
    }

    case PunchArmState::Punching: {
        punchDamageActiveThisFrame_ = true;
        stateTimer_ += deltaTime;
        const float progress = std::clamp(
            stateTimer_ / config_.punchDuration, 0.0f, 1.0f);
        const float extension = std::sin(progress * std::numbers::pi_v<float>) *
            config_.punchDistance;
        fistPosition_ = {
            home.x + punchDirection_.x * extension,
            home.y,
            home.z + punchDirection_.z * extension
        };
        if (stateTimer_ >= config_.punchDuration) {
            state_ = PunchArmState::Recovery;
            stateTimer_ = 0.0f;
        }
        break;
    }

    case PunchArmState::RocketFlying: {
        rocketDamageActiveThisFrame_ = true;
        const float remainingDistance = std::max(0.0f,
            rocketMaximumRange_ - rocketTravelDistance_);
        const float remainingBaseDistance = std::max(0.0f,
            rocketBaseAvoidanceDistance_ - rocketTravelDistance_);
        const float travel = std::min(config_.rocketSpeed * deltaTime,
            std::min(remainingDistance, remainingBaseDistance));
        fistPosition_ = Add(fistPosition_, Multiply(punchDirection_, travel));
        rocketTravelDistance_ += travel;
        if (rocketTravelDistance_ >= rocketBaseAvoidanceDistance_ - kEpsilon) {
            BeginRocketReturn(true);
            rocketDamageActiveThisFrame_ = false;
        } else if (rocketTravelDistance_ >= rocketMaximumRange_ - kEpsilon) {
            BeginRocketReturn(false);
        }
        break;
    }

    case PunchArmState::RocketEvacuating:
    case PunchArmState::Returning:
        UpdateRocketReturn(deltaTime, playerPosition, bodyYaw);
        break;

    case PunchArmState::Recovery:
        stateTimer_ += deltaTime;
        fistPosition_ = home;
        if (stateTimer_ >= config_.recoveryDuration) {
            state_ = PunchArmState::Ready;
            attackType_ = PunchArmAttackType::None;
            chargeTimer_ = 0.0f;
        }
        break;
    }
    chargedVisualShakeTimer_ = state_ == PunchArmState::Charged
        ? chargedVisualShakeTimer_ + deltaTime : 0.0f;
}
