#include "Gameplay/Player/PunchArm.h"

#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <cmath>
#include <numbers>

PunchArm::PunchArm(PunchArmSide side)
    : side_(side) {
}

void PunchArm::Reset(const Vector3& playerPosition, float bodyYaw) {
    state_ = PunchArmState::Ready;
    chargeTimer_ = 0.0f;
    stateTimer_ = 0.0f;
    punchDirection_ = { std::sin(bodyYaw), 0.0f, std::cos(bodyYaw) };
    fistPosition_ = CalculateHomePosition(playerPosition, bodyYaw);
    previousFistPosition_ = fistPosition_;
    punchDamageActiveThisFrame_ = false;
}

float PunchArm::GetChargeRatio() const {
    return std::clamp(chargeTimer_ / FirstPersonTuning::kPunchChargeTime, 0.0f, 1.0f);
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

void PunchArm::BeginPunch(const Vector3& playerPosition, float bodyYaw) {
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

void PunchArm::Update(float deltaTime, const Vector3& playerPosition, float bodyYaw,
    bool punchHeld) {
    previousFistPosition_ = fistPosition_;
    punchDamageActiveThisFrame_ = false;
    if (!std::isfinite(deltaTime) || deltaTime < 0.0f ||
        !std::isfinite(bodyYaw) || !std::isfinite(playerPosition.x) ||
        !std::isfinite(playerPosition.y) || !std::isfinite(playerPosition.z)) {
        return;
    }

    const Vector3 home = CalculateHomePosition(playerPosition, bodyYaw);
    const float forwardX = std::sin(bodyYaw);
    const float forwardZ = std::cos(bodyYaw);
    switch (state_) {
    case PunchArmState::Ready:
        fistPosition_ = home;
        if (punchHeld) {
            state_ = PunchArmState::Charging;
            chargeTimer_ = 0.0f;
        }
        break;

    case PunchArmState::Charging:
        if (!punchHeld) {
            state_ = PunchArmState::Ready;
            chargeTimer_ = 0.0f;
            fistPosition_ = home;
            break;
        }
        chargeTimer_ = std::min(chargeTimer_ + deltaTime, FirstPersonTuning::kPunchChargeTime);
        fistPosition_ = {
            home.x - forwardX * FirstPersonTuning::kPunchChargeRetractDistance * GetChargeRatio(),
            home.y,
            home.z - forwardZ * FirstPersonTuning::kPunchChargeRetractDistance * GetChargeRatio()
        };
        if (chargeTimer_ >= FirstPersonTuning::kPunchChargeTime) {
            state_ = PunchArmState::Charged;
        }
        break;

    case PunchArmState::Charged:
        chargeTimer_ = FirstPersonTuning::kPunchChargeTime;
        fistPosition_ = {
            home.x - forwardX * FirstPersonTuning::kPunchChargeRetractDistance,
            home.y,
            home.z - forwardZ * FirstPersonTuning::kPunchChargeRetractDistance
        };
        if (!punchHeld) {
            BeginPunch(playerPosition, bodyYaw);
        }
        break;

    case PunchArmState::Punching: {
        punchDamageActiveThisFrame_ = true;
        stateTimer_ += deltaTime;
        const float progress = std::clamp(
            stateTimer_ / FirstPersonTuning::kPunchDuration, 0.0f, 1.0f);
        const float extension = std::sin(progress * std::numbers::pi_v<float>) *
            FirstPersonTuning::kPunchDistance;
        fistPosition_ = {
            home.x + punchDirection_.x * extension,
            home.y,
            home.z + punchDirection_.z * extension
        };
        if (stateTimer_ >= FirstPersonTuning::kPunchDuration) {
            state_ = PunchArmState::Recovery;
            stateTimer_ = 0.0f;
        }
        break;
    }

    case PunchArmState::Recovery:
        stateTimer_ += deltaTime;
        fistPosition_ = home;
        if (stateTimer_ >= FirstPersonTuning::kPunchRecoveryDuration) {
            state_ = PunchArmState::Ready;
            chargeTimer_ = 0.0f;
        }
        break;
    }
}
