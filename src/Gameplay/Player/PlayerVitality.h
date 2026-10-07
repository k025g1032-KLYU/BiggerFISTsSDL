#pragma once

#include "Data/PlayerCombatConfig.h"

enum class PlayerHitType {
    Projectile,
    ChargeUnit,
    BossSpreadBurst,
    NeedleFinisher
};

struct PlayerHitResult {
    bool wasApplied = false;
    int shieldLayersBeforeHit = 0;
    int shieldLayersLost = 0;
    bool didDamageHitPoints = false;
    int hitPointsAfterHit = 0;
};

class PlayerVitality {
public:
    explicit PlayerVitality(PlayerCombatConfig config = {});
    void Reset();
    void Update(float deltaTime);
    PlayerHitResult ReceiveHit(PlayerHitType type);

    int GetHitPoints() const { return hitPoints_; }
    int GetShieldLayers() const { return shieldLayers_; }
    float GetShieldRechargeRatio() const;
    bool IsDefeated() const { return hitPoints_ <= 0; }

private:
    PlayerCombatConfig config_;
    int hitPoints_ = 0;
    int shieldLayers_ = 0;
    float shieldRechargeTimer_ = 0.0f;
};
