#pragma once

#include "Math/MathTypes.h"
#include "Data/PlayerCombatConfig.h"

enum class PunchArmSide {
    Left,
    Right
};

enum class PunchArmState {
    Ready,
    Charging,
    Charged,
    Punching,
    RocketFlying,
    RocketEvacuating,
    Returning,
    Recovery
};

enum class PunchArmAttackType {
    None,
    Punch,
    RocketPunch
};

struct PunchArmInput {
    bool punchHeld = false;
    bool rocketHeld = false;
};

class PunchArm {
public:
    explicit PunchArm(PunchArmSide side, PlayerCombatConfig config = {});

    void Reset(const Vector3& playerPosition, float bodyYaw);
    void Update(float deltaTime, const Vector3& playerPosition, float bodyYaw,
        const PunchArmInput& input, bool allowNewCharge = true);

    PunchArmState GetState() const { return state_; }
    PunchArmAttackType GetAttackType() const { return attackType_; }
    bool IsChargingOrCharged() const {
        return state_ == PunchArmState::Charging || state_ == PunchArmState::Charged;
    }
    bool IsFistAway() const {
        return state_ == PunchArmState::RocketFlying ||
            state_ == PunchArmState::RocketEvacuating ||
            state_ == PunchArmState::Returning;
    }
    float GetChargeRatio() const;
    const Vector3& GetFistPosition() const { return fistPosition_; }
    const Vector3& GetPreviousFistPosition() const { return previousFistPosition_; }
    Vector3 GetChargedVisualOffset() const;
    bool IsPunchDamageActiveThisFrame() const { return punchDamageActiveThisFrame_; }
    bool IsRocketDamageActiveThisFrame() const { return rocketDamageActiveThisFrame_; }
    bool DidLaunchRocketThisFrame() const { return didLaunchRocketThisFrame_; }
    bool DidBeginPunchThisFrame() const { return didBeginPunchThisFrame_; }
    bool DidReconnectThisFrame() const { return didReconnectThisFrame_; }
    float GetRocketPosePitch() const { return rocketPosePitch_; }
    float GetRocketPoseYaw() const { return rocketPoseYaw_; }
    float GetRocketTravelDistance() const { return rocketTravelDistance_; }
    float GetRocketMaximumRange() const { return rocketMaximumRange_; }

private:
    enum class RocketReturnPhase {
        TurnUp,
        Ascend,
        TurnHome,
        FlyHome,
        Align,
        Dock
    };

    Vector3 CalculateHomePosition(const Vector3& playerPosition, float bodyYaw) const;
    Vector3 CalculateRocketDockPosition(const Vector3& playerPosition, float bodyYaw) const;
    Vector3 CalculateChargePosition(const Vector3& playerPosition, float bodyYaw) const;
    Vector3 CalculateReturnEntryPosition(const Vector3& playerPosition, float bodyYaw) const;
    float CalculateRocketMaximumRange(const Vector3& start, const Vector3& direction) const;
    float CalculateRocketBaseAvoidanceDistance(const Vector3& start, const Vector3& direction) const;
    void BeginPunch(const Vector3& playerPosition, float bodyYaw);
    void BeginRocket(const Vector3& playerPosition, float bodyYaw);
    void BeginRocketReturn(bool avoidBase);
    void AdvanceRocketReturnPhase(RocketReturnPhase phase);
    void UpdateRocketReturn(float deltaTime, const Vector3& playerPosition, float bodyYaw);

    PunchArmSide side_;
    PlayerCombatConfig config_;
    PunchArmState state_ = PunchArmState::Ready;
    PunchArmAttackType attackType_ = PunchArmAttackType::None;
    Vector3 fistPosition_{};
    Vector3 previousFistPosition_{};
    Vector3 punchDirection_{0.0f, 0.0f, 1.0f};
    bool punchDamageActiveThisFrame_ = false;
    bool rocketDamageActiveThisFrame_ = false;
    bool didLaunchRocketThisFrame_ = false;
    bool didBeginPunchThisFrame_ = false;
    bool didReconnectThisFrame_ = false;
    float chargeTimer_ = 0.0f;
    float chargedVisualShakeTimer_ = 0.0f;
    float stateTimer_ = 0.0f;
    float rocketTravelDistance_ = 0.0f;
    float rocketMaximumRange_ = 0.0f;
    float rocketBaseAvoidanceDistance_ = 0.0f;
    float rocketPosePitch_ = 0.0f;
    float rocketPoseYaw_ = 0.0f;
    float returnPhaseStartPitch_ = 0.0f;
    float returnPhaseStartYaw_ = 0.0f;
    float returnAscentDistance_ = 0.0f;
    Vector3 returnPhaseStartPosition_{};
    RocketReturnPhase rocketReturnPhase_ = RocketReturnPhase::TurnUp;
};
