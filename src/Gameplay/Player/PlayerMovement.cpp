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

PlayerMovement::PlayerMovement(float maxDeltaTime)
    : maxDeltaTime_(maxDeltaTime) {
    Reset({ 0.0f, FirstPersonTuning::kPlayerInitialHeight, 0.0f });
}

void PlayerMovement::Reset(const Vector3& position) {
    position_ = position;
    velocity_ = {};
    bodyYaw_ = 0.0f;
}

void PlayerMovement::Update(double deltaSeconds, float moveRight, float moveForward, float cameraYaw) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0 ||
        !std::isfinite(moveRight) || !std::isfinite(moveForward) ||
        !std::isfinite(cameraYaw) || !std::isfinite(maxDeltaTime_) || maxDeltaTime_ <= 0.0f) {
        return;
    }
    const float deltaTime = static_cast<float>(std::min(deltaSeconds, static_cast<double>(maxDeltaTime_)));

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

    const bool hasMovementInput = lengthSquared > 0.00001f;
    const float velocityChange = (hasMovementInput
        ? FirstPersonTuning::kMoveAcceleration : FirstPersonTuning::kMoveDeceleration) * deltaTime;
    velocity_.x = MoveTowards(velocity_.x,
        directionX * FirstPersonTuning::kMaximumMoveSpeed, velocityChange);
    velocity_.z = MoveTowards(velocity_.z,
        directionZ * FirstPersonTuning::kMaximumMoveSpeed, velocityChange);
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

    bodyYaw_ = MoveTowardsAngle(bodyYaw_, cameraYaw,
        FirstPersonTuning::kBodyTurnSpeed * deltaTime);
}
