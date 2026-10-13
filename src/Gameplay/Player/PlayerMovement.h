#pragma once

#include "Math/MathTypes.h"
#include "Data/PlayerCombatConfig.h"

enum class PlayerImpactState { None, Holding, Knockback, NeedleStun };
enum class PlayerDodgeState { Ready, Boosting, Sliding, Recovery };

class PlayerMovement {
public:
    explicit PlayerMovement(float maxDeltaTime, PlayerCombatConfig config = {});

    void Reset(const Vector3& position);
    void Update(double deltaSeconds, float moveRight, float moveForward, float cameraYaw);
    void StartChargeImpact(const Vector3& direction, bool wasShielded);
    void StartBurstImpact(const Vector3& direction);
    void StartBossBodyPush(const Vector3& direction);
    void StartNeedleStun();
    void StartNeedleFinisher(const Vector3& direction);
    void ClearNeedleStun();
    void StartUltimateRecoil();
    void ClearImpact();
    bool TryStartDodge(float moveRight, float moveForward);
    void SetResearchInstituteCollisionEnabled(bool enabled) {
        researchInstituteCollisionEnabled_ = enabled;
    }

    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetPreviousPosition() const { return previousPosition_; }
    const Vector3& GetVelocity() const { return velocity_; }
    float GetBodyYaw() const { return bodyYaw_; }
    PlayerImpactState GetImpactState() const { return impactState_; }
    float GetImpactStateTimer() const { return impactTimer_; }
    PlayerDodgeState GetDodgeState() const { return dodgeState_; }
    const Vector3& GetDodgeDirection() const { return dodgeDirection_; }
    float GetDodgeStateTimer() const { return dodgeTimer_; }
    float GetDodgeCooldownRemaining() const { return dodgeCooldownRemaining_; }
    bool IsDodgeReady() const {
        return dodgeState_ == PlayerDodgeState::Ready &&
            dodgeCooldownRemaining_ <= 0.0f;
    }

private:
    void ResolveResearchInstituteCollision();
    float maxDeltaTime_;
    PlayerCombatConfig config_;
    Vector3 position_{};
    Vector3 previousPosition_{};
    Vector3 velocity_{};
    float bodyYaw_ = 0.0f;
    PlayerImpactState impactState_ = PlayerImpactState::None;
    Vector3 impactDirection_{0.0f, 0.0f, 1.0f};
    float impactTimer_ = 0.0f;
    float impactInitialSpeed_ = 0.0f;
    float impactDuration_ = 0.0f;
    bool researchInstituteCollisionEnabled_ = false;
    PlayerDodgeState dodgeState_ = PlayerDodgeState::Ready;
    Vector3 dodgeDirection_{0.0f, 0.0f, 1.0f};
    float dodgeTimer_ = 0.0f;
    float dodgeCooldownRemaining_ = 0.0f;
};
