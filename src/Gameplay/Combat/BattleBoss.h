#pragma once

#include "Data/BattleCombatConfig.h"
#include "Math/MathTypes.h"

#include <array>
#include <cstdint>
#include <vector>

enum class BossMode {
    Entrance, Cruise, Idle, Charge, Boxer, NeedleCombo, FullBarrage,
    SpreadCounter, Defeating, Defeated
};

enum class BossAttackType {
    ChargeImpact, BoxerStrike, NeedleStrike, SpreadBurst,
    CrystalSpike, CrystalCube
};

struct BossAttackEvent {
    BossAttackType type = BossAttackType::ChargeImpact;
    Vector3 position{};
};

struct BossPlayerThreat {
    std::array<bool, 2> isRocketCharged{};
    std::array<bool, 2> isChargedRocketAimedAtCore{};
    bool hasIncomingRocketPunch = false;
};

class BattleBoss {
public:
    explicit BattleBoss(BossCombatConfig config = {});
    void Reset(const Vector3& position);
    std::vector<BossAttackEvent> Update(float seconds,
        const Vector3& playerPosition, const BossPlayerThreat& threat = {});
    bool ReceiveHit(int damage);
    void BeginBehavior(BossMode mode);

    BossMode GetMode() const { return mode_; }
    const Vector3& GetPosition() const { return position_; }
    const Vector3& GetPreviousPosition() const { return previousPosition_; }
    int GetHitPoints() const { return hitPoints_; }
    int GetMaximumHitPoints() const { return config_.coreHitPoints; }
    bool IsDefeated() const { return mode_ == BossMode::Defeated; }
    float GetModeTime() const { return modeTime_; }
    bool IsRocketDodging() const { return rocketDodgeTimer_ > 0.0f; }
    bool DidBeginRocketDodgeThisStep() const { return beganRocketDodgeThisStep_; }

private:
    enum class CruisePhase { TakingOff, Flying, Landing };
    float NextBehaviorRandom();
    void ChooseIdleAction(const Vector3& playerPosition, float playerDistance);
    void BeginCruise(const Vector3& playerPosition);
    void UpdateCruise(float seconds);
    bool UpdateRocketDodge(float seconds, const Vector3& playerPosition,
        const BossPlayerThreat& threat);

    BossCombatConfig config_;
    BossMode mode_ = BossMode::Entrance;
    Vector3 position_{};
    Vector3 previousPosition_{};
    Vector3 chargeDirection_{0.0f, 0.0f, -1.0f};
    float modeTime_ = 0.0f;
    float attackTimer_ = 0.0f;
    int hitPoints_ = 0;
    std::uint32_t behaviorRandomState_ = 0x6d2b79f5u;
    BossMode lastIdleAction_ = BossMode::Entrance;
    float boxerCooldown_ = 0.0f;
    float chargeCooldown_ = 0.0f;
    float cruiseCooldown_ = 0.0f;
    CruisePhase cruisePhase_ = CruisePhase::TakingOff;
    Vector3 cruiseTarget_{};
    float cruiseFlightTime_ = 0.0f;
    int lastCruiseCandidate_ = -1;
    int attackCount_ = 0;
    bool attackRegistered_ = false;
    bool needleComboPending_ = false;
    bool needleComboUsed_ = false;
    bool fullBarragePending_ = false;
    bool fullBarrageUsed_ = false;
    bool fullBarrageDamageLocked_ = false;
    float idleStrafeTimer_ = 0.0f;
    float idleStrafeSide_ = 1.0f;
    float spreadPressureTimer_ = 0.0f;
    float spreadCooldown_ = 0.0f;
    bool spreadPressureArmed_ = true;
    float crystalSpikeTimer_ = 0.0f;
    float crystalCubeTimer_ = 0.0f;
    std::array<float, 2> chargedRocketAimTimers_{};
    std::array<bool, 2> evaluatedChargedRocketAim_{};
    float rocketDodgeTimer_ = 0.0f;
    float rocketDodgeCooldown_ = 0.0f;
    float boxerRocketDodgeCooldown_ = 0.0f;
    float rocketDodgeSpeed_ = 0.0f;
    bool beganRocketDodgeThisStep_ = false;
};
