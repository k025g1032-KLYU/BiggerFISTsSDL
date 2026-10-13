#include "Data/BattleCombatConfig.h"

#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace {
std::string ToUtf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

template <typename T, std::size_t N, std::size_t M>
bool LoadFields(const std::filesystem::path& path, T& output,
    const std::array<std::pair<std::string_view, float T::*>, N>& fields,
    const std::array<std::pair<std::string_view, int T::*>, M>& integerFields,
    std::string& error) {
    std::ifstream input(path);
    if (!input) {
        error = "Could not open '" + ToUtf8(path) + "'";
        return false;
    }
    T parsed{};
    std::unordered_set<std::string> seen;
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        std::istringstream stream(line);
        std::string key;
        std::string valueText;
        std::string extra;
        if (!(stream >> key) || key.starts_with('#')) {
            continue;
        }
        if (!(stream >> valueText) || (stream >> extra) ||
            !seen.insert(key).second) {
            error = "Invalid value in '" + ToUtf8(path) + "' line " +
                std::to_string(lineNumber) + ": " + key;
            return false;
        }
        bool found = false;
        for (const auto& [name, member] : fields) {
            if (name == key) {
                std::istringstream number(valueText);
                float value = 0.0f;
                char trailing = 0;
                if (!(number >> value) || (number >> trailing) ||
                    !std::isfinite(value) || value > 10000.0f ||
                    (key == "deploymentMinimumX" ? value < -10000.0f :
                        value <= 0.0f)) {
                    break;
                }
                parsed.*member = value;
                found = true;
                break;
            }
        }
        for (const auto& [name, member] : integerFields) {
            if (name == key) {
                std::istringstream number(valueText);
                int value = 0;
                char trailing = 0;
                if (!(number >> value) || (number >> trailing) ||
                    value < 1 || value > 1000) {
                    break;
                }
                parsed.*member = value;
                found = true;
                break;
            }
        }
        if (!found) {
            error = "Unknown key or invalid value in '" + ToUtf8(path) + "' line " +
                std::to_string(lineNumber) + ": " + key;
            return false;
        }
    }
    if (input.bad()) {
        error = "Read error in '" + ToUtf8(path) + "'";
        return false;
    }
    for (const auto& [name, member] : fields) {
        if (!seen.contains(std::string(name))) {
            error = "Missing key '" + std::string(name) + "' in '" + ToUtf8(path) + "'";
            return false;
        }
    }
    for (const auto& [name, member] : integerFields) {
        if (!seen.contains(std::string(name))) {
            error = "Missing key '" + std::string(name) + "' in '" + ToUtf8(path) + "'";
            return false;
        }
    }
    output = parsed;
    return true;
}

constexpr std::array<std::pair<std::string_view, float EnemyCombatConfig::*>, 51>
    kEnemyFields{{
        {"radius", &EnemyCombatConfig::radius},
        {"dropStartHeight", &EnemyCombatConfig::dropStartHeight},
        {"deploymentMinimumX", &EnemyCombatConfig::deploymentMinimumX},
        {"deploymentMaximumX", &EnemyCombatConfig::deploymentMaximumX},
        {"deploymentMinimumZ", &EnemyCombatConfig::deploymentMinimumZ},
        {"deploymentMaximumZ", &EnemyCombatConfig::deploymentMaximumZ},
        {"combatHeightMinimum", &EnemyCombatConfig::combatHeightMinimum},
        {"combatHeightMaximum", &EnemyCombatConfig::combatHeightMaximum},
        {"chargeCombatHeight", &EnemyCombatConfig::chargeCombatHeight},
        {"dropInitialSpeed", &EnemyCombatConfig::dropInitialSpeed},
        {"dropAcceleration", &EnemyCombatConfig::dropAcceleration},
        {"dropMaximumSpeed", &EnemyCombatConfig::dropMaximumSpeed},
        {"landingPause", &EnemyCombatConfig::landingPause},
        {"takeoffSpeed", &EnemyCombatConfig::takeoffSpeed},
        {"missileMoveSpeed", &EnemyCombatConfig::missileMoveSpeed},
        {"missilePatrolTurnSpeed", &EnemyCombatConfig::missilePatrolTurnSpeed},
        {"missileFireTurnSpeed", &EnemyCombatConfig::missileFireTurnSpeed},
        {"missileInitialFireDelay", &EnemyCombatConfig::missileInitialFireDelay},
        {"missileFireCooldown", &EnemyCombatConfig::missileFireCooldown},
        {"missileFirePauseBeforeLaunch", &EnemyCombatConfig::missileFirePauseBeforeLaunch},
        {"missileFirePauseAfterLaunch", &EnemyCombatConfig::missileFirePauseAfterLaunch},
        {"bomberMoveSpeed", &EnemyCombatConfig::bomberMoveSpeed},
        {"bomberMinimumMoveSpeed", &EnemyCombatConfig::bomberMinimumMoveSpeed},
        {"bomberMaximumMoveSpeed", &EnemyCombatConfig::bomberMaximumMoveSpeed},
        {"bomberInitialFireDelay", &EnemyCombatConfig::bomberInitialFireDelay},
        {"bomberFireCooldown", &EnemyCombatConfig::bomberFireCooldown},
        {"chargeApproachSpeed", &EnemyCombatConfig::chargeApproachSpeed},
        {"chargeDashSpeed", &EnemyCombatConfig::chargeDashSpeed},
        {"chargeDetectionRange", &EnemyCombatConfig::chargeDetectionRange},
        {"chargeMinimumDistance", &EnemyCombatConfig::chargeMinimumDistance},
        {"chargeRepositionSpeed", &EnemyCombatConfig::chargeRepositionSpeed},
        {"chargePrepareDuration", &EnemyCombatConfig::chargePrepareDuration},
        {"chargeHitBrakingDuration", &EnemyCombatConfig::chargeHitBrakingDuration},
        {"chargeMissBrakingDuration", &EnemyCombatConfig::chargeMissBrakingDuration},
        {"chargeHitBrakingInitialSpeed", &EnemyCombatConfig::chargeHitBrakingInitialSpeed},
        {"chargeImpactDuration", &EnemyCombatConfig::chargeImpactDuration},
        {"chargeImpactWidthScale", &EnemyCombatConfig::chargeImpactWidthScale},
        {"chargeImpactHeightScale", &EnemyCombatConfig::chargeImpactHeightScale},
        {"chargeImpactDepthScale", &EnemyCombatConfig::chargeImpactDepthScale},
        {"chargeTargetPassDistance", &EnemyCombatConfig::chargeTargetPassDistance},
        {"chargeBaseSafetyMargin", &EnemyCombatConfig::chargeBaseSafetyMargin},
        {"defeatLaunchDuration", &EnemyCombatConfig::defeatLaunchDuration},
        {"defeatPunchSpeed", &EnemyCombatConfig::defeatPunchSpeed},
        {"defeatRocketSpeed", &EnemyCombatConfig::defeatRocketSpeed},
        {"defeatUltimateSpeed", &EnemyCombatConfig::defeatUltimateSpeed},
        {"defeatUpwardSpeed", &EnemyCombatConfig::defeatUpwardSpeed},
        {"defeatGravity", &EnemyCombatConfig::defeatGravity},
        {"defeatBoundsMargin", &EnemyCombatConfig::defeatBoundsMargin},
        {"defeatRotationXSpeed", &EnemyCombatConfig::defeatRotationXSpeed},
        {"defeatRotationYSpeed", &EnemyCombatConfig::defeatRotationYSpeed},
        {"defeatRotationZSpeed", &EnemyCombatConfig::defeatRotationZSpeed},
    }};
constexpr std::array<std::pair<std::string_view, float ProjectileCombatConfig::*>, 14>
    kProjectileFields{{
        {"missileSpeed", &ProjectileCombatConfig::missileSpeed},
        {"missileRadius", &ProjectileCombatConfig::missileRadius},
        {"missileTurnDuration", &ProjectileCombatConfig::missileTurnDuration},
        {"missileTurnBoundaryZ", &ProjectileCombatConfig::missileTurnBoundaryZ},
        {"bombSpeed", &ProjectileCombatConfig::bombSpeed},
        {"bombRadius", &ProjectileCombatConfig::bombRadius},
        {"bombOscillationAmplitude", &ProjectileCombatConfig::bombOscillationAmplitude},
        {"bombOscillationFrequency", &ProjectileCombatConfig::bombOscillationFrequency},
        {"crystalSpikeSpeed", &ProjectileCombatConfig::crystalSpikeSpeed},
        {"crystalSpikeRadius", &ProjectileCombatConfig::crystalSpikeRadius},
        {"crystalCubeSpeed", &ProjectileCombatConfig::crystalCubeSpeed},
        {"crystalCubeRadius", &ProjectileCombatConfig::crystalCubeRadius},
        {"crystalCubeOscillationAmplitude", &ProjectileCombatConfig::crystalCubeOscillationAmplitude},
        {"crystalCubeOscillationFrequency", &ProjectileCombatConfig::crystalCubeOscillationFrequency},
    }};
constexpr std::array<std::pair<std::string_view, float BossCombatConfig::*>, 140>
    kBossFields{{
        {"poseBlendSpeed", &BossCombatConfig::poseBlendSpeed},
        {"cruisePoseBlendSpeed", &BossCombatConfig::cruisePoseBlendSpeed},
        {"coreRadius", &BossCombatConfig::coreRadius},
        {"hoverHeight", &BossCombatConfig::hoverHeight},
        {"entranceSpawnHeight", &BossCombatConfig::entranceSpawnHeight},
        {"entranceDropSpeed", &BossCombatConfig::entranceDropSpeed},
        {"entranceFlipStartHeight", &BossCombatConfig::entranceFlipStartHeight},
        {"entranceFlipDuration", &BossCombatConfig::entranceFlipDuration},
        {"idleDecisionInterval", &BossCombatConfig::idleDecisionInterval},
        {"idleMoveSpeed", &BossCombatConfig::idleMoveSpeed},
        {"idleMinimumDistance", &BossCombatConfig::idleMinimumDistance},
        {"idleMaximumDistance", &BossCombatConfig::idleMaximumDistance},
        {"chargeMinimumStartDistance", &BossCombatConfig::chargeMinimumStartDistance},
        {"idleStrafePeriod", &BossCombatConfig::idleStrafePeriod},
        {"idleNoActionRetryInterval", &BossCombatConfig::idleNoActionRetryInterval},
        {"boxerCooldown", &BossCombatConfig::boxerCooldown},
        {"chargeCooldown", &BossCombatConfig::chargeCooldown},
        {"cruiseCooldown", &BossCombatConfig::cruiseCooldown},
        {"nearBoxerWeight", &BossCombatConfig::nearBoxerWeight},
        {"nearChargeWeight", &BossCombatConfig::nearChargeWeight},
        {"nearRangedWeight", &BossCombatConfig::nearRangedWeight},
        {"nearCruiseWeight", &BossCombatConfig::nearCruiseWeight},
        {"middleBoxerWeight", &BossCombatConfig::middleBoxerWeight},
        {"middleChargeWeight", &BossCombatConfig::middleChargeWeight},
        {"middleRangedWeight", &BossCombatConfig::middleRangedWeight},
        {"middleCruiseWeight", &BossCombatConfig::middleCruiseWeight},
        {"farBoxerWeight", &BossCombatConfig::farBoxerWeight},
        {"farChargeWeight", &BossCombatConfig::farChargeWeight},
        {"farRangedWeight", &BossCombatConfig::farRangedWeight},
        {"farCruiseWeight", &BossCombatConfig::farCruiseWeight},
        {"cruiseTakeoffHeight", &BossCombatConfig::cruiseTakeoffHeight},
        {"cruiseTakeoffSpeed", &BossCombatConfig::cruiseTakeoffSpeed},
        {"cruiseFlightSpeed", &BossCombatConfig::cruiseFlightSpeed},
        {"cruiseLandingSpeed", &BossCombatConfig::cruiseLandingSpeed},
        {"cruiseMinimumFlightDuration", &BossCombatConfig::cruiseMinimumFlightDuration},
        {"cruiseHoverOrbitRadius", &BossCombatConfig::cruiseHoverOrbitRadius},
        {"cruiseHoverOrbitAngularSpeed", &BossCombatConfig::cruiseHoverOrbitAngularSpeed},
        {"cruiseMinimumLandingPlayerDistance", &BossCombatConfig::cruiseMinimumLandingPlayerDistance},
        {"cruiseQuadrantDeadZoneX", &BossCombatConfig::cruiseQuadrantDeadZoneX},
        {"cruiseQuadrantDeadZoneZ", &BossCombatConfig::cruiseQuadrantDeadZoneZ},
        {"cruiseCandidateWallMargin", &BossCombatConfig::cruiseCandidateWallMargin},
        {"cruiseCandidateBaseMargin", &BossCombatConfig::cruiseCandidateBaseMargin},
        {"cruiseMinimumRelocationDistance", &BossCombatConfig::cruiseMinimumRelocationDistance},
        {"chargedRocketAimDodgeDelay", &BossCombatConfig::chargedRocketAimDodgeDelay},
        {"rocketDodgeWarningDistance", &BossCombatConfig::rocketDodgeWarningDistance},
        {"rocketDodgeDirectionDotThreshold", &BossCombatConfig::rocketDodgeDirectionDotThreshold},
        {"idleRocketDodgeDuration", &BossCombatConfig::idleRocketDodgeDuration},
        {"idleRocketDodgeSpeed", &BossCombatConfig::idleRocketDodgeSpeed},
        {"idleRocketDodgeCooldown", &BossCombatConfig::idleRocketDodgeCooldown},
        {"boxerRocketDodgeDuration", &BossCombatConfig::boxerRocketDodgeDuration},
        {"boxerRocketDodgeSpeed", &BossCombatConfig::boxerRocketDodgeSpeed},
        {"boxerRocketDodgeCooldown", &BossCombatConfig::boxerRocketDodgeCooldown},
        {"spreadPressureDistance", &BossCombatConfig::spreadPressureDistance},
        {"spreadPressureRearmDistance", &BossCombatConfig::spreadPressureRearmDistance},
        {"spreadPressureDuration", &BossCombatConfig::spreadPressureDuration},
        {"spreadPressureDecaySpeed", &BossCombatConfig::spreadPressureDecaySpeed},
        {"spreadPressureRetreatSpeed", &BossCombatConfig::spreadPressureRetreatSpeed},
        {"spreadBurstCooldown", &BossCombatConfig::spreadBurstCooldown},
        {"crystalSpikeInitialDelay", &BossCombatConfig::crystalSpikeInitialDelay},
        {"crystalSpikeInterval", &BossCombatConfig::crystalSpikeInterval},
        {"crystalCubeInitialDelay", &BossCombatConfig::crystalCubeInitialDelay},
        {"crystalCubeInterval", &BossCombatConfig::crystalCubeInterval},
        {"crystalLaunchDuration", &BossCombatConfig::crystalLaunchDuration},
        {"crystalReloadDuration", &BossCombatConfig::crystalReloadDuration},
        {"crystalHaloRadius", &BossCombatConfig::crystalHaloRadius},
        {"crystalHaloBackOffset", &BossCombatConfig::crystalHaloBackOffset},
        {"crystalHaloRotationSpeed", &BossCombatConfig::crystalHaloRotationSpeed},
        {"crystalLaunchHeight", &BossCombatConfig::crystalLaunchHeight},
        {"crystalRemoteSpawnMargin", &BossCombatConfig::crystalRemoteSpawnMargin},
        {"chargePrepareDuration", &BossCombatConfig::chargePrepareDuration},
        {"chargeSpeed", &BossCombatConfig::chargeSpeed},
        {"chargeTurnDuration", &BossCombatConfig::chargeTurnDuration},
        {"chargePullbackDuration", &BossCombatConfig::chargePullbackDuration},
        {"chargePullbackDistance", &BossCombatConfig::chargePullbackDistance},
        {"chargeAimTurnSpeed", &BossCombatConfig::chargeAimTurnSpeed},
        {"chargeTipOffset", &BossCombatConfig::chargeTipOffset},
        {"chargeTipRadius", &BossCombatConfig::chargeTipRadius},
        {"chargeImpactDuration", &BossCombatConfig::chargeImpactDuration},
        {"chargeWallImpactDuration", &BossCombatConfig::chargeWallImpactDuration},
        {"boxerApproachSpeed", &BossCombatConfig::boxerApproachSpeed},
        {"boxerOrbitSpeed", &BossCombatConfig::boxerOrbitSpeed},
        {"boxerDesiredDistance", &BossCombatConfig::boxerDesiredDistance},
        {"boxerStrikeStartDistance", &BossCombatConfig::boxerStrikeStartDistance},
        {"boxerDistanceTolerance", &BossCombatConfig::boxerDistanceTolerance},
        {"boxerEntryMaximumDistance", &BossCombatConfig::boxerEntryMaximumDistance},
        {"boxerEntryWindupDuration", &BossCombatConfig::boxerEntryWindupDuration},
        {"boxerEntryPullbackDistance", &BossCombatConfig::boxerEntryPullbackDistance},
        {"boxerEntryDashSpeed", &BossCombatConfig::boxerEntryDashSpeed},
        {"boxerEntryDashMaximumDistance", &BossCombatConfig::boxerEntryDashMaximumDistance},
        {"boxerEntryDashMaximumDuration", &BossCombatConfig::boxerEntryDashMaximumDuration},
        {"boxerEntryTimeout", &BossCombatConfig::boxerEntryTimeout},
        {"boxerBehaviorDuration", &BossCombatConfig::boxerBehaviorDuration},
        {"boxerClosingStepMaximumDistance", &BossCombatConfig::boxerClosingStepMaximumDistance},
        {"boxerClosingStepSpeed", &BossCombatConfig::boxerClosingStepSpeed},
        {"boxerClosingStepDuration", &BossCombatConfig::boxerClosingStepDuration},
        {"boxerClosingStepCooldown", &BossCombatConfig::boxerClosingStepCooldown},
        {"boxerClosingStepLateralWeight", &BossCombatConfig::boxerClosingStepLateralWeight},
        {"boxerClosingStepEndMargin", &BossCombatConfig::boxerClosingStepEndMargin},
        {"boxerStrikeInterval", &BossCombatConfig::boxerStrikeInterval},
        {"boxerWindupDuration", &BossCombatConfig::boxerWindupDuration},
        {"boxerWindupHoldDuration", &BossCombatConfig::boxerWindupHoldDuration},
        {"boxerRecoveryDuration", &BossCombatConfig::boxerRecoveryDuration},
        {"boxerStrikeDuration", &BossCombatConfig::boxerStrikeDuration},
        {"boxerStrikeRadius", &BossCombatConfig::boxerStrikeRadius},
        {"boxerStrikeOffset", &BossCombatConfig::boxerStrikeOffset},
        {"boxerStrikePullbackDistance", &BossCombatConfig::boxerStrikePullbackDistance},
        {"needleTipOffset", &BossCombatConfig::needleTipOffset},
        {"needleStrikeRadius", &BossCombatConfig::needleStrikeRadius},
        {"needleWindupDuration", &BossCombatConfig::needleWindupDuration},
        {"needlePullbackDistance", &BossCombatConfig::needlePullbackDistance},
        {"needleDashSpeed", &BossCombatConfig::needleDashSpeed},
        {"needleFrontDistance", &BossCombatConfig::needleFrontDistance},
        {"needleStrikeDuration", &BossCombatConfig::needleStrikeDuration},
        {"needleStrikeOffset", &BossCombatConfig::needleStrikeOffset},
        {"needleStrikePauseDuration", &BossCombatConfig::needleStrikePauseDuration},
        {"barrageFlightHeightOffset", &BossCombatConfig::barrageFlightHeightOffset},
        {"barrageDeploymentZ", &BossCombatConfig::barrageDeploymentZ},
        {"barrageTurnUpDuration", &BossCombatConfig::barrageTurnUpDuration},
        {"barrageAscendSpeed", &BossCombatConfig::barrageAscendSpeed},
        {"barrageLevelingDuration", &BossCombatConfig::barrageLevelingDuration},
        {"barrageFlightSpeed", &BossCombatConfig::barrageFlightSpeed},
        {"barrageDiveTurnDuration", &BossCombatConfig::barrageDiveTurnDuration},
        {"barrageDiveSpeed", &BossCombatConfig::barrageDiveSpeed},
        {"barrageLandingTurnDuration", &BossCombatConfig::barrageLandingTurnDuration},
        {"barrageShotInterval", &BossCombatConfig::barrageShotInterval},
        {"barrageChargeTransitionDuration", &BossCombatConfig::barrageChargeTransitionDuration},
        {"spreadWindupDuration", &BossCombatConfig::spreadWindupDuration},
        {"spreadSpinDuration", &BossCombatConfig::spreadSpinDuration},
        {"spreadBurstRadius", &BossCombatConfig::spreadBurstRadius},
        {"defeatLaunchHorizontalSpeed", &BossCombatConfig::defeatLaunchHorizontalSpeed},
        {"defeatLaunchUpwardSpeed", &BossCombatConfig::defeatLaunchUpwardSpeed},
        {"defeatGravity", &BossCombatConfig::defeatGravity},
        {"defeatWallBounceRetention", &BossCombatConfig::defeatWallBounceRetention},
        {"defeatGroundBounceSpeed", &BossCombatConfig::defeatGroundBounceSpeed},
        {"defeatGroundHorizontalRetention", &BossCombatConfig::defeatGroundHorizontalRetention},
        {"defeatPauseDuration", &BossCombatConfig::defeatPauseDuration},
        {"defeatSmallExplosionInterval", &BossCombatConfig::defeatSmallExplosionInterval},
        {"defeatRotationXSpeed", &BossCombatConfig::defeatRotationXSpeed},
        {"defeatRotationYSpeed", &BossCombatConfig::defeatRotationYSpeed},
        {"defeatRotationZSpeed", &BossCombatConfig::defeatRotationZSpeed},
    }};
constexpr std::array<std::pair<std::string_view, int EnemyCombatConfig::*>, 2>
    kEnemyIntegers{{
        {"baseHitPoints", &EnemyCombatConfig::baseHitPoints},
        {"day2BaseHitPoints", &EnemyCombatConfig::day2BaseHitPoints},
    }};
constexpr std::array<std::pair<std::string_view, int BossCombatConfig::*>, 4>
    kBossIntegers{{
        {"coreHitPoints", &BossCombatConfig::coreHitPoints},
        {"ultimateDamage", &BossCombatConfig::ultimateDamage},
        {"cruiseMaximumRetargetCount", &BossCombatConfig::cruiseMaximumRetargetCount},
        {"chargeSequenceCount", &BossCombatConfig::chargeSequenceCount},
    }};
}

bool LoadBattleCombatConfig(const std::filesystem::path& directory,
    BattleCombatConfig& config, std::string& error) {
    BattleCombatConfig parsed;
    if (!LoadFields(directory / "enemy-combat.cfg", parsed.enemy,
            kEnemyFields, kEnemyIntegers, error) ||
        !LoadFields(directory / "projectile-combat.cfg", parsed.projectile,
            kProjectileFields,
            std::array<std::pair<std::string_view, int ProjectileCombatConfig::*>, 0>{},
            error) ||
        !LoadFields(directory / "boss-combat.cfg", parsed.boss,
            kBossFields, kBossIntegers, error)) {
        return false;
    }
    if (parsed.enemy.deploymentMaximumX <= parsed.enemy.deploymentMinimumX ||
        parsed.enemy.deploymentMaximumZ <= parsed.enemy.deploymentMinimumZ ||
        parsed.enemy.combatHeightMaximum < parsed.enemy.combatHeightMinimum ||
        parsed.enemy.chargeCombatHeight <= parsed.enemy.radius) {
        error = "Invalid deployment bounds in '" +
            ToUtf8(directory / "enemy-combat.cfg") + "'";
        return false;
    }
    if (parsed.enemy.bomberMaximumMoveSpeed <
        parsed.enemy.bomberMinimumMoveSpeed) {
        error = "Invalid Bomber speed bounds in '" +
            ToUtf8(directory / "enemy-combat.cfg") + "'";
        return false;
    }
    if (parsed.boss.idleMinimumDistance >=
            parsed.boss.idleMaximumDistance ||
        parsed.boss.spreadPressureDistance >=
            parsed.boss.spreadPressureRearmDistance ||
        parsed.boss.chargeMinimumStartDistance <=
            parsed.boss.spreadPressureDistance ||
        parsed.boss.boxerDesiredDistance <=
            parsed.boss.boxerStrikeStartDistance ||
        parsed.boss.boxerClosingStepMaximumDistance <=
            parsed.boss.boxerStrikeStartDistance) {
        error = "Invalid Boss distance thresholds in '" +
            ToUtf8(directory / "boss-combat.cfg") + "'";
        return false;
    }
    if (parsed.boss.idleNoActionRetryInterval >
            parsed.boss.idleDecisionInterval ||
        parsed.boss.cruiseMinimumLandingPlayerDistance <= 0.0f ||
        parsed.boss.rocketDodgeDirectionDotThreshold > 1.0f) {
        error = "Invalid Boss action or cruise timing in '" +
            ToUtf8(directory / "boss-combat.cfg") + "'";
        return false;
    }
    if (parsed.boss.chargeTurnDuration +
            parsed.boss.chargePullbackDuration >=
            parsed.boss.chargePrepareDuration ||
        parsed.boss.chargeSequenceCount < 1 ||
        parsed.boss.chargeTipOffset <= 0.0f ||
        parsed.boss.chargeTipRadius <= 0.0f) {
        error = "Invalid Boss charge settings in '" +
            ToUtf8(directory / "boss-combat.cfg") + "'";
        return false;
    }
    config = parsed;
    return true;
}
