#pragma once

#include "Math/MathTypes.h"

class FirstPersonCamera {
public:
    void Reset(const Vector3& eye);
    void UpdateLook(float mouseDeltaX, float mouseDeltaY);
    void AddShake(float strength, float duration);
    void Follow(const Vector3& cameraAnchor, double deltaSeconds);

    const Vector3& GetPosition() const { return position_; }
    Vector3 GetTarget() const;
    float GetYaw() const { return yaw_; }
    float GetPitch() const { return pitch_; }
    float GetShakeTimeRemaining() const { return shakeTimer_; }

private:
    Vector3 position_{};
    Vector3 followedAnchor_{};
    float yaw_ = 0.0f;
    float pitch_ = 0.0f;
    float shakeStrength_ = 0.0f;
    float shakeDuration_ = 0.0f;
    float shakeTimer_ = 0.0f;
    float shakePhase_ = 0.0f;
};
