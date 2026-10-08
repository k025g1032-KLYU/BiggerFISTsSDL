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
    ChargeImpact, BoxerStrike, NeedleStrike, NeedleImpact, SpreadBurst,
    CrystalSpike, CrystalCube
};

struct BossAttackEvent {
    BossAttackType type = BossAttackType::ChargeImpact;
    Vector3 position{};
    Vector3 previousPosition{};
    float radius = 0.0f;
    std::uint64_t attackId = 0;
};

struct BossCrystalSlot {
    Vector3 position{};
    float launchTime = 0.0f;
    float reloadTime = 0.0f;
    bool consumed = false;
};

struct BossPlayerThreat {
    std::array<bool, 2> isRocketCharged{};
    std::array<bool, 2> isChargedRocketAimedAtCore{};
    bool hasIncomingRocketPunch = false;
};

struct BossPartPose {
    Vector3 translation{};
    Vector3 rotation{};
    Vector3 rootTranslation{};
};

struct BossVisualPose {
    Vector3 rootRotation{};
    Vector3 coreRotation{};
    std::array<BossPartPose, 3> cones{};
    std::array<BossPartPose, 6> spikes{};
    std::array<BossPartPose, 4> cubes{};
    float spreadSpin = 0.0f;
    float crystalAngle = 0.0f;
};

class BattleBoss {
public:
    explicit BattleBoss(BossCombatConfig config = {});
    void Reset(const Vector3& position);
    std::vector<BossAttackEvent> Update(float seconds,
        const Vector3& playerPosition, const BossPlayerThreat& threat = {});
    bool ReceiveHit(int damage);
    bool RegisterNeedleStrikeHit(std::uint64_t attackId);
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
    const std::array<BossCrystalSlot, 6>& GetSpikeSlots() const { return spikeSlots_; }
    const std::array<BossCrystalSlot, 4>& GetCubeSlots() const { return cubeSlots_; }
    BossVisualPose GetVisualPose() const;
    BossVisualPose GetInterpolatedVisualPose(float alpha) const;

private:
    enum class CruisePhase { TakingOff, Flying, Landing };
    enum class BoxerPhase { Approaching, EntryWindup, EntryDashing,
        Orbiting, WindingUp, Holding, Striking, Recovering };
    float NextBehaviorRandom();
    void ChooseIdleAction(const Vector3& playerPosition, float playerDistance);
    void BeginCruise(const Vector3& playerPosition);
    void UpdateCruise(float seconds, const Vector3& playerPosition);
    int ResolvePlayerQuadrant(const Vector3& playerPosition);
    bool SelectCruiseTarget(const Vector3& playerPosition, int quadrant,
        int excludedCandidate = -1);
    void UpdateCrystals(float seconds, std::vector<BossAttackEvent>& attacks);
    bool BeginCrystalLaunch(bool spike);
    bool UpdateRocketDodge(float seconds, const Vector3& playerPosition,
        const BossPlayerThreat& threat);
    BossVisualPose CalculateTargetVisualPose() const;
    void UpdateVisualPose(float seconds);

    BossCombatConfig config_;
    BossMode mode_ = BossMode::Entrance;
    Vector3 position_{};
    Vector3 previousPosition_{};
    Vector3 chargeDirection_{0.0f, 0.0f, -1.0f};
    float modeTime_ = 0.0f;
    bool entranceRecovering_ = false;
    float entranceRecoveryTime_ = 0.0f;
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
    int cruiseTargetCandidate_ = -1;
    int cruiseTargetQuadrant_ = 0;
    int lastPlayerQuadrant_ = -1;
    int cruiseRetargetCount_ = 0;
    std::uint64_t nextAttackId_ = 1;
    Vector3 strikeStart_{};
    Vector3 strikeEnd_{};
    Vector3 strikePrevious_{};
    int strikeIndex_ = 0;
    float strikeTime_ = 0.0f;
    BoxerPhase boxerPhase_ = BoxerPhase::Approaching;
    float boxerPhaseTime_ = 0.0f;
    float boxerEntryDashDistance_ = 0.0f;
    bool usedBoxerEntryDash_ = false;
    Vector3 boxerEntryDirection_{};
    bool needleDashComplete_ = false;
    bool needleFirstHit_ = false;
    std::uint64_t needleFirstAttackId_ = 0;
    float needleImpactTime_ = 0.0f;
    std::uint64_t needleImpactAttackId_ = 0;
    std::uint64_t activeAttackId_ = 0;
    std::array<BossCrystalSlot, 6> spikeSlots_{};
    std::array<BossCrystalSlot, 4> cubeSlots_{};
    std::size_t nextSpikeSlot_ = 0;
    std::size_t nextCubeSlot_ = 0;
    float crystalAngle_ = 0.0f;
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
    float visualYaw_ = 0.0f;
    std::array<BossPartPose, 3> visualCones_{};
    std::array<BossPartPose, 3> spreadStartCones_{};
    Vector3 visualCoreRotation_{};
    BossVisualPose previousVisualPose_{};
};
