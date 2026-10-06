#pragma once

#include "Math/MathTypes.h"

class PlayerMovement {
public:
    explicit PlayerMovement(float maxDeltaTime);

    void Reset(const Vector3& position);
    void Update(double deltaSeconds, float moveRight, float moveForward, float cameraYaw);

    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetVelocity() const { return velocity_; }
    float GetBodyYaw() const { return bodyYaw_; }

private:
    float maxDeltaTime_;
    Vector3 position_{};
    Vector3 velocity_{};
    float bodyYaw_ = 0.0f;
};
