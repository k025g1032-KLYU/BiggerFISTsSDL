#pragma once

#include <filesystem>
#include <string>

struct PlayerCombatConfig {
    float chargeTime = 0.55f;
    float punchDuration = 0.22f;
    float punchDistance = 3.0f;
    float recoveryDuration = 0.30f;
    float rocketSpeed = 100.0f;
    float rocketMaximumRange = 80.0f;
    float rocketReturnFlightSpeed = 75.0f;
    float rocketBattlefieldExitMargin = 3.0f;
    float rocketBaseAvoidanceMargin = 0.5f;
    float rocketReturnTurnUpDuration = 0.16f;
    float rocketReturnTurnUpAscentDistance = 2.5f;
    float rocketReturnAscentSpeed = 32.0f;
    float rocketReturnAscentDistance = 16.0f;
    float rocketBaseEvacuationDistance = 16.0f;
    float rocketReturnTurnHomeDuration = 0.16f;
    float rocketReturnTrackingTurnSpeed = 10.0f;
    float rocketReturnAlignDuration = 0.18f;
    float rocketReturnDockDuration = 0.30f;
    float rocketReturnEntryHeightAboveShoulder = 9.0f;
    float punchShakeStrength = 0.12f;
    float punchShakeDuration = 0.16f;
    float rocketLaunchShakeStrength = 0.07f;
    float rocketLaunchShakeDuration = 0.12f;
    float chargedFistShakeAmplitude = 0.035f;
    float chargedFistShakeFrequency = 60.0f;
    int maximumHitPoints = 5;
    int maximumShieldLayers = 3;
    float shieldRechargeInterval = 3.0f;
    float shieldedChargeSpeed = 6.0f;
    float shieldedChargeDuration = 0.25f;
    float unshieldedChargeHoldDuration = 0.08f;
    float unshieldedChargeSpeed = 15.0f;
    float unshieldedChargeDuration = 1.0f;
    float burstKnockbackSpeed = 28.0f;
    float burstKnockbackDuration = 0.75f;
    float projectileShieldShakeStrength = 0.055f;
    float projectileShieldShakeDuration = 0.10f;
    float projectileHitPointShakeStrength = 0.10f;
    float projectileHitPointShakeDuration = 0.15f;
    float chargeHitShakeStrength = 0.18f;
    float chargeHitShakeDuration = 0.22f;
    float stepBobFrequency = 5.4f;
    float stepBobHorizontalAmplitude = 0.04f;
    float stepBobVerticalAmplitude = 0.08f;
    float stepBobFadeSpeed = 5.0f;
};

bool LoadPlayerCombatConfig(const std::filesystem::path& path,
    PlayerCombatConfig& config, std::string& error);
