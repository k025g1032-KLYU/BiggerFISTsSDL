#include "World/FirstPersonCamera.h"

#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <cmath>
#include <numbers>

void FirstPersonCamera::Reset(const Vector3& eye) {
    position_ = eye;
    previousPosition_ = eye;
    followedAnchor_ = eye;
    yaw_ = 0.0f;
    pitch_ = 0.0f;
    shakeStrength_ = 0.0f;
    shakeDuration_ = 0.0f;
    shakeTimer_ = 0.0f;
    shakePhase_ = 0.0f;
    stepBobPhase_ = 0.0f;
    stepBobWeight_ = 0.0f;
    stepImpactTimer_ = 0.0f;
    stepPitchOffset_ = 0.0f;
    didStepThisFrame_ = false;
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

void FirstPersonCamera::Follow(const Vector3& cameraAnchor, double deltaSeconds,
    float movementSpeed) {
    didStepThisFrame_ = false;
    if (!std::isfinite(deltaSeconds) || deltaSeconds < 0.0 ||
        !std::isfinite(cameraAnchor.x) || !std::isfinite(cameraAnchor.y) ||
        !std::isfinite(cameraAnchor.z) || !std::isfinite(movementSpeed)) {
        return;
    }
    previousPosition_ = position_;
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
    const float stepSeconds = static_cast<float>(deltaSeconds);
    const float speedRatio = std::clamp(movementSpeed /
        FirstPersonTuning::kMaximumMoveSpeed, 0.0f, 1.0f);
    const float targetWeight = movementSpeed > FirstPersonTuning::kStepBobMinimumSpeed
        ? speedRatio : 0.0f;
    const float weightChange = config_.stepBobFadeSpeed * stepSeconds;
    stepBobWeight_ += std::clamp(targetWeight - stepBobWeight_,
        -weightChange, weightChange);
    if (targetWeight > 0.0f) {
        const float previousStep = std::floor(stepBobPhase_ / std::numbers::pi_v<float>);
        stepBobPhase_ += config_.stepBobFrequency * speedRatio * stepSeconds;
        if (std::floor(stepBobPhase_ / std::numbers::pi_v<float>) != previousStep &&
            stepBobWeight_ > 0.2f) {
            stepImpactTimer_ = FirstPersonTuning::kStepImpactDuration;
            didStepThisFrame_ = true;
        }
    }
    stepImpactTimer_ = std::max(0.0f, stepImpactTimer_ - stepSeconds);
    const float impactRatio = stepImpactTimer_ / FirstPersonTuning::kStepImpactDuration;
    Vector3 bobOffset{};
    bobOffset.x = std::sin(stepBobPhase_) *
        config_.stepBobHorizontalAmplitude * stepBobWeight_;
    bobOffset.y = -0.5f * (1.0f - std::cos(stepBobPhase_ * 2.0f)) *
        config_.stepBobVerticalAmplitude * stepBobWeight_;
    bobOffset.y -= FirstPersonTuning::kStepImpactAmplitude * impactRatio * stepBobWeight_;
    stepPitchOffset_ = FirstPersonTuning::kStepImpactPitch * impactRatio * stepBobWeight_;
    Vector3 shakeOffset{};
    if (shakeTimer_ > 0.0f && shakeDuration_ > 0.00001f) {
        const float deltaTime = static_cast<float>(std::min(deltaSeconds,
            static_cast<double>(shakeTimer_)));
        shakeTimer_ = std::max(0.0f, shakeTimer_ - deltaTime);
        shakePhase_ += deltaTime * FirstPersonTuning::kShakeFrequency;
        const float attenuation = shakeTimer_ / shakeDuration_;
        shakeOffset.x = std::sin(shakePhase_) * shakeStrength_ * attenuation;
        shakeOffset.y = std::sin(shakePhase_ *
            FirstPersonTuning::kSecondaryShakeFrequencyScale) * shakeStrength_ * attenuation;
    }
    position_ = {followedAnchor_.x + bobOffset.x + shakeOffset.x,
        followedAnchor_.y + bobOffset.y + shakeOffset.y,
        followedAnchor_.z + shakeOffset.z};
}

void FirstPersonCamera::AddShake(float strength, float duration) {
    if (!std::isfinite(strength) || !std::isfinite(duration) ||
        strength <= 0.0f || duration <= 0.0f) {
        return;
    }
    if (shakeTimer_ <= 0.0f) {
        shakeStrength_ = strength;
        shakeDuration_ = duration;
        shakePhase_ = 0.0f;
    } else {
        shakeStrength_ = std::max(shakeStrength_, strength);
        shakeDuration_ = std::max(shakeDuration_, duration);
    }
    shakeTimer_ = std::max(shakeTimer_, duration);
}

Vector3 FirstPersonCamera::GetTarget() const {
    const float effectivePitch = pitch_ + stepPitchOffset_;
    const float cosine = std::cos(effectivePitch);
    return {
        position_.x + std::sin(yaw_) * cosine,
        position_.y - std::sin(effectivePitch),
        position_.z + std::cos(yaw_) * cosine
    };
}
