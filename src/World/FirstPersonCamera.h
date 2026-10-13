#pragma once

#include "Math/MathTypes.h"
#include "Data/PlayerCombatConfig.h"

enum class CameraControlMode { Driving, Observation, AligningToDriving };

class FirstPersonCamera {
public:
    explicit FirstPersonCamera(PlayerCombatConfig config = {}) : config_(config) {}
    void Reset(const Vector3& eye);
    void UpdateLook(float mouseDeltaX, float mouseDeltaY);
    void UpdateControl(float seconds, float bodyYaw);
    void ToggleObservationMode(float bodyYaw);
    void BeginTutorialVisualCheck() { controlMode_ = CameraControlMode::Observation; }
    void BeginTutorialCockpitAlignment(float bodyYaw);
    void AddShake(float strength, float duration);
    void Follow(const Vector3& cameraAnchor, double deltaSeconds,
        float movementSpeed = 0.0f);

    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetPreviousPosition() const { return previousPosition_; }
    Vector3 GetTarget() const;
    float GetYaw() const { return yaw_; }
    float GetPitch() const { return pitch_; }
    CameraControlMode GetControlMode() const { return controlMode_; }
    bool ShouldBodyFollowCamera() const {
        return controlMode_ == CameraControlMode::Driving;
    }
    float GetShakeTimeRemaining() const { return shakeTimer_; }
    bool DidStepThisFrame() const { return didStepThisFrame_; }

private:
    PlayerCombatConfig config_;
    Vector3 position_{};
    Vector3 previousPosition_{};
    Vector3 followedAnchor_{};
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    CameraControlMode controlMode_ = CameraControlMode::Driving;
    float shakeStrength_ = 0.0f;
    float shakeDuration_ = 0.0f;
    float shakeTimer_ = 0.0f;
    float shakePhase_ = 0.0f;
    float stepBobPhase_ = 0.0f;
    float stepBobWeight_ = 0.0f;
    float stepImpactTimer_ = 0.0f;
    float stepPitchOffset_ = 0.0f;
    bool didStepThisFrame_ = false;
};
