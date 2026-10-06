#include "Gameplay/Player/PlayerVitality.h"

#include <algorithm>
#include <cmath>

PlayerVitality::PlayerVitality(PlayerCombatConfig config) : config_(config) {
    Reset();
}

void PlayerVitality::Reset() {
    hitPoints_ = config_.maximumHitPoints;
    shieldLayers_ = config_.maximumShieldLayers;
    shieldRechargeTimer_ = 0.0f;
}

void PlayerVitality::Update(float deltaTime) {
    if (!std::isfinite(deltaTime) || deltaTime < 0.0f) {
        return;
    }
    if (shieldLayers_ >= config_.maximumShieldLayers) {
        shieldRechargeTimer_ = 0.0f;
        return;
    }
    shieldRechargeTimer_ += deltaTime;
    if (shieldRechargeTimer_ >= config_.shieldRechargeInterval) {
        ++shieldLayers_;
        shieldRechargeTimer_ = 0.0f;
    }
}

PlayerHitResult PlayerVitality::ReceiveHit(PlayerHitType type) {
    PlayerHitResult result{};
    result.wasApplied = true;
    result.shieldLayersBeforeHit = shieldLayers_;
    shieldRechargeTimer_ = 0.0f;
    if (shieldLayers_ > 0) {
        if (type == PlayerHitType::ChargeUnit || type == PlayerHitType::BossSpreadBurst) {
            shieldLayers_ = 0;
        } else {
            --shieldLayers_;
        }
        result.shieldLayersLost = result.shieldLayersBeforeHit - shieldLayers_;
    } else if (hitPoints_ > 0) {
        --hitPoints_;
        result.didDamageHitPoints = true;
    }
    result.hitPointsAfterHit = hitPoints_;
    return result;
}

float PlayerVitality::GetShieldRechargeRatio() const {
    if (shieldLayers_ >= config_.maximumShieldLayers) {
        return 1.0f;
    }
    return std::clamp(shieldRechargeTimer_ / config_.shieldRechargeInterval, 0.0f, 1.0f);
}
