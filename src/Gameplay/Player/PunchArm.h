#pragma once

#include "Math/MathTypes.h"

enum class PunchArmSide {
    Left,
    Right
};

enum class PunchArmState {
    Ready,
    Charging,
    Charged,
    Punching,
    Recovery
};

class PunchArm {
public:
    explicit PunchArm(PunchArmSide side);

    void Reset(const Vector3& playerPosition, float bodyYaw);
    void Update(float deltaTime, const Vector3& playerPosition, float bodyYaw,
        bool punchHeld);

    PunchArmState GetState() const { return state_; }
    bool IsChargingOrCharged() const {
        return state_ == PunchArmState::Charging || state_ == PunchArmState::Charged;
    }
    float GetChargeRatio() const;
    const Vector3& GetFistPosition() const { return fistPosition_; }
    const Vector3& GetPreviousFistPosition() const { return previousFistPosition_; }
    bool IsPunchDamageActiveThisFrame() const { return punchDamageActiveThisFrame_; }

private:
    Vector3 CalculateHomePosition(const Vector3& playerPosition, float bodyYaw) const;
    void BeginPunch(const Vector3& playerPosition, float bodyYaw);

    PunchArmSide side_;
    PunchArmState state_ = PunchArmState::Ready;
    Vector3 fistPosition_{};
    Vector3 previousFistPosition_{};
    Vector3 punchDirection_{0.0f, 0.0f, 1.0f};
    bool punchDamageActiveThisFrame_ = false;
    float chargeTimer_ = 0.0f;
    float stateTimer_ = 0.0f;
};
