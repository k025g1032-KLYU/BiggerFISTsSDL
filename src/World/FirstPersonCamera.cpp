#include "World/FirstPersonCamera.h"

#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <cmath>
#include <numbers>

void FirstPersonCamera::Reset(const Vector3& eye) {
    position_ = eye;
    followedAnchor_ = eye;
    yaw_ = 0.0f;
    pitch_ = 0.0f;
}

void FirstPersonCamera::UpdateLook(float mouseDeltaX, float mouseDeltaY) {
    if (!std::isfinite(mouseDeltaX) || !std::isfinite(mouseDeltaY)) {
        return;
    }
    yaw_ = std::remainder(yaw_ + mouseDeltaX * FirstPersonTuning::kMouseLookSensitivity,
        2.0f * std::numbers::pi_v<float>);
    pitch_ = std::clamp(pitch_ + mouseDeltaY * FirstPersonTuning::kMouseLookSensitivity,
        -FirstPersonTuning::kMaximumPitch, FirstPersonTuning::kMaximumPitch);
}

void FirstPersonCamera::Follow(const Vector3& cameraAnchor, double deltaSeconds) {
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0 ||
        !std::isfinite(cameraAnchor.x) || !std::isfinite(cameraAnchor.y) ||
        !std::isfinite(cameraAnchor.z)) {
        return;
    }
    const Vector3 offset = SubtractVectors(cameraAnchor, followedAnchor_);
    const float distanceSquared = DotVectors(offset, offset);
    if (distanceSquared >= FirstPersonTuning::kCameraFollowSnapDistance *
            FirstPersonTuning::kCameraFollowSnapDistance) {
        followedAnchor_ = cameraAnchor;
    } else {
        const float followAmount = 1.0f - std::exp(-FirstPersonTuning::kCameraFollowResponsiveness *
            static_cast<float>(deltaSeconds));
        followedAnchor_.x += offset.x * followAmount;
        followedAnchor_.y += offset.y * followAmount;
        followedAnchor_.z += offset.z * followAmount;
    }
    position_ = followedAnchor_;
}

Vector3 FirstPersonCamera::GetTarget() const {
    const float cosine = std::cos(pitch_);
    return {
        position_.x + std::sin(yaw_) * cosine,
        position_.y - std::sin(pitch_),
        position_.z + std::cos(yaw_) * cosine
    };
}
