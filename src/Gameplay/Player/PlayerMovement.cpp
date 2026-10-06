#include "Gameplay/Player/PlayerMovement.h"

#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
float MoveTowards(float current, float target, float maximumDelta) {
    const float difference = target - current;
    if (std::abs(difference) <= maximumDelta) {
        return target;
    }
    return current + std::copysign(maximumDelta, difference);
}

float NormalizeAngle(float angle) {
    return std::remainder(angle, 2.0f * std::numbers::pi_v<float>);
}

float MoveTowardsAngle(float current, float target, float maximumDelta) {
    const float difference = NormalizeAngle(target - current);
    return NormalizeAngle(current + std::clamp(difference, -maximumDelta, maximumDelta));
}
}

PlayerMovement::PlayerMovement(float maxDeltaTime, PlayerCombatConfig config)
    : maxDeltaTime_(maxDeltaTime), config_(config) {
    Reset({ 0.0f, FirstPersonTuning::kPlayerInitialHeight, 0.0f });
}

void PlayerMovement::Reset(const Vector3& position) {
    position_ = position;
    previousPosition_ = position;
    velocity_ = {};
    bodyYaw_ = 0.0f;
    ClearImpact();
}

void PlayerMovement::ClearImpact() {
    impactState_ = PlayerImpactState::None;
    impactTimer_ = 0.0f;
    impactInitialSpeed_ = 0.0f;
    impactDuration_ = 0.0f;
}

void PlayerMovement::StartChargeImpact(const Vector3& direction, bool wasShielded) {
    const float length = std::hypot(direction.x, direction.z);
    impactDirection_ = std::isfinite(length) && length > 0.00001f
        ? Vector3{direction.x / length, 0.0f, direction.z / length}
        : Vector3{std::sin(bodyYaw_), 0.0f, std::cos(bodyYaw_)};
    impactTimer_ = 0.0f;
    if (wasShielded) {
        impactState_ = PlayerImpactState::Knockback;
        impactInitialSpeed_ = config_.shieldedChargeSpeed;
        impactDuration_ = config_.shieldedChargeDuration;
        velocity_ = {impactDirection_.x * impactInitialSpeed_, 0.0f,
            impactDirection_.z * impactInitialSpeed_};
    } else {
        impactState_ = PlayerImpactState::Holding;
        impactInitialSpeed_ = config_.unshieldedChargeSpeed;
        impactDuration_ = config_.unshieldedChargeDuration;
        velocity_ = {};
    }
}

void PlayerMovement::StartBurstImpact(const Vector3& direction) {
    const float length = std::hypot(direction.x, direction.z);
    impactDirection_ = std::isfinite(length) && length > 0.00001f
        ? Vector3{direction.x / length, 0.0f, direction.z / length}
        : Vector3{std::sin(bodyYaw_), 0.0f, std::cos(bodyYaw_)};
    impactState_ = PlayerImpactState::Knockback;
    impactTimer_ = 0.0f;
    impactInitialSpeed_ = config_.burstKnockbackSpeed;
    impactDuration_ = config_.burstKnockbackDuration;
    velocity_ = {impactDirection_.x * impactInitialSpeed_, 0.0f,
        impactDirection_.z * impactInitialSpeed_};
}

void PlayerMovement::Update(double deltaSeconds, float moveRight, float moveForward, float cameraYaw) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0 ||
        !std::isfinite(moveRight) || !std::isfinite(moveForward) ||
        !std::isfinite(cameraYaw) || !std::isfinite(maxDeltaTime_) || maxDeltaTime_ <= 0.0f) {
        return;
    }
    const float deltaTime = static_cast<float>(std::min(deltaSeconds, static_cast<double>(maxDeltaTime_)));
    previousPosition_ = position_;

    const float sine = std::sin(cameraYaw);
    const float cosine = std::cos(cameraYaw);
    float directionX = cosine * moveRight + sine * moveForward;
    float directionZ = -sine * moveRight + cosine * moveForward;
    const float lengthSquared = directionX * directionX + directionZ * directionZ;
    if (lengthSquared > 1.0f) {
        const float inverseLength = 1.0f / std::sqrt(lengthSquared);
        directionX *= inverseLength;
        directionZ *= inverseLength;
    }

    if (impactState_ != PlayerImpactState::None) {
        impactTimer_ += deltaTime;
        if (impactState_ == PlayerImpactState::Holding) {
            velocity_ = {};
            if (impactTimer_ >= config_.unshieldedChargeHoldDuration) {
                impactState_ = PlayerImpactState::Knockback;
                impactTimer_ = 0.0f;
                velocity_ = {impactDirection_.x * impactInitialSpeed_, 0.0f,
                    impactDirection_.z * impactInitialSpeed_};
            }
        } else {
            const float progress = std::clamp(impactTimer_ / impactDuration_, 0.0f, 1.0f);
            const float speed = impactInitialSpeed_ * (1.0f - progress);
            velocity_ = {impactDirection_.x * speed, 0.0f, impactDirection_.z * speed};
            if (impactTimer_ >= impactDuration_) {
                velocity_ = {};
                ClearImpact();
            }
        }
    } else {
        const bool hasMovementInput = lengthSquared > 0.00001f;
        const float velocityChange = (hasMovementInput
            ? FirstPersonTuning::kMoveAcceleration : FirstPersonTuning::kMoveDeceleration) * deltaTime;
        velocity_.x = MoveTowards(velocity_.x,
            directionX * FirstPersonTuning::kMaximumMoveSpeed, velocityChange);
        velocity_.z = MoveTowards(velocity_.z,
            directionZ * FirstPersonTuning::kMaximumMoveSpeed, velocityChange);
    }
    position_.x += velocity_.x * deltaTime;
    position_.z += velocity_.z * deltaTime;

    const float minimumX = FirstPersonTuning::kBattlefieldMinimumX + FirstPersonTuning::kPlayerColliderRadius;
    const float maximumX = FirstPersonTuning::kBattlefieldMaximumX - FirstPersonTuning::kPlayerColliderRadius;
    const float minimumZ = FirstPersonTuning::kBattlefieldMinimumZ + FirstPersonTuning::kPlayerColliderRadius;
    const float maximumZ = FirstPersonTuning::kBattlefieldMaximumZ - FirstPersonTuning::kPlayerColliderRadius;
    if (position_.x < minimumX) {
        position_.x = minimumX;
        velocity_.x = std::max(0.0f, velocity_.x);
    } else if (position_.x > maximumX) {
        position_.x = maximumX;
        velocity_.x = std::min(0.0f, velocity_.x);
    }
    if (position_.z < minimumZ) {
        position_.z = minimumZ;
        velocity_.z = std::max(0.0f, velocity_.z);
    } else if (position_.z > maximumZ) {
        position_.z = maximumZ;
        velocity_.z = std::min(0.0f, velocity_.z);
    }
    if (researchInstituteCollisionEnabled_) {
        ResolveResearchInstituteCollision();
    }

    bodyYaw_ = MoveTowardsAngle(bodyYaw_, cameraYaw,
        FirstPersonTuning::kBodyTurnSpeed * deltaTime);
}

void PlayerMovement::ResolveResearchInstituteCollision() {
    const float minimumDistance = FirstPersonTuning::kResearchInstituteColliderRadius +
        FirstPersonTuning::kPlayerColliderRadius +
        FirstPersonTuning::kPlayerResearchInstituteCollisionMargin;
    const float centerZ = FirstPersonTuning::kBattlefieldMinimumZ;
    float outwardX = position_.x;
    float outwardZ = position_.z - centerZ;
    float distanceSquared = outwardX * outwardX + outwardZ * outwardZ;
    if (distanceSquared >= minimumDistance * minimumDistance) {
        return;
    }
    if (distanceSquared <= 0.00001f) {
        outwardX = previousPosition_.x;
        outwardZ = previousPosition_.z - centerZ;
        distanceSquared = outwardX * outwardX + outwardZ * outwardZ;
        if (distanceSquared <= 0.00001f) {
            outwardX = 0.0f;
            outwardZ = 1.0f;
            distanceSquared = 1.0f;
        }
    }
    const float inverseDistance = 1.0f / std::sqrt(distanceSquared);
    outwardX *= inverseDistance;
    outwardZ *= inverseDistance;
    position_.x = outwardX * minimumDistance;
    position_.z = centerZ + outwardZ * minimumDistance;
    const float inwardSpeed = velocity_.x * outwardX + velocity_.z * outwardZ;
    if (inwardSpeed < 0.0f) {
        velocity_.x -= outwardX * inwardSpeed;
        velocity_.z -= outwardZ * inwardSpeed;
    }
}
