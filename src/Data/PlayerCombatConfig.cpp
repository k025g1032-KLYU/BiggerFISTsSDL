#include "Data/PlayerCombatConfig.h"

#include <array>
#include <cmath>
#include <fstream>
#include <locale>
#include <sstream>
#include <string_view>
#include <unordered_set>
#include <utility>

namespace {
using FloatField = float PlayerCombatConfig::*;
using IntField = int PlayerCombatConfig::*;

constexpr std::array<std::pair<std::string_view, FloatField>, 57> floatFields{{
    std::pair<std::string_view, FloatField>{"chargeTime", &PlayerCombatConfig::chargeTime},
    {"punchDuration", &PlayerCombatConfig::punchDuration},
    {"punchDistance", &PlayerCombatConfig::punchDistance},
    {"recoveryDuration", &PlayerCombatConfig::recoveryDuration},
    {"rocketSpeed", &PlayerCombatConfig::rocketSpeed},
    {"rocketMaximumRange", &PlayerCombatConfig::rocketMaximumRange},
    {"rocketReturnFlightSpeed", &PlayerCombatConfig::rocketReturnFlightSpeed},
    {"rocketBattlefieldExitMargin", &PlayerCombatConfig::rocketBattlefieldExitMargin},
    {"rocketBaseAvoidanceMargin", &PlayerCombatConfig::rocketBaseAvoidanceMargin},
    {"rocketReturnTurnUpDuration", &PlayerCombatConfig::rocketReturnTurnUpDuration},
    {"rocketReturnTurnUpAscentDistance", &PlayerCombatConfig::rocketReturnTurnUpAscentDistance},
    {"rocketReturnAscentSpeed", &PlayerCombatConfig::rocketReturnAscentSpeed},
    {"rocketReturnAscentDistance", &PlayerCombatConfig::rocketReturnAscentDistance},
    {"rocketBaseEvacuationDistance", &PlayerCombatConfig::rocketBaseEvacuationDistance},
    {"rocketReturnTurnHomeDuration", &PlayerCombatConfig::rocketReturnTurnHomeDuration},
    {"rocketReturnTrackingTurnSpeed", &PlayerCombatConfig::rocketReturnTrackingTurnSpeed},
    {"rocketReturnAlignDuration", &PlayerCombatConfig::rocketReturnAlignDuration},
    {"rocketReturnDockDuration", &PlayerCombatConfig::rocketReturnDockDuration},
    {"rocketReturnEntryHeightAboveShoulder", &PlayerCombatConfig::rocketReturnEntryHeightAboveShoulder},
    {"punchShakeStrength", &PlayerCombatConfig::punchShakeStrength},
    {"punchShakeDuration", &PlayerCombatConfig::punchShakeDuration},
    {"rocketLaunchShakeStrength", &PlayerCombatConfig::rocketLaunchShakeStrength},
    {"rocketLaunchShakeDuration", &PlayerCombatConfig::rocketLaunchShakeDuration},
    {"chargedFistShakeAmplitude", &PlayerCombatConfig::chargedFistShakeAmplitude},
    {"chargedFistShakeFrequency", &PlayerCombatConfig::chargedFistShakeFrequency},
    {"shieldRechargeInterval", &PlayerCombatConfig::shieldRechargeInterval},
    {"shieldedChargeSpeed", &PlayerCombatConfig::shieldedChargeSpeed},
    {"shieldedChargeDuration", &PlayerCombatConfig::shieldedChargeDuration},
    {"unshieldedChargeHoldDuration", &PlayerCombatConfig::unshieldedChargeHoldDuration},
    {"unshieldedChargeSpeed", &PlayerCombatConfig::unshieldedChargeSpeed},
    {"unshieldedChargeDuration", &PlayerCombatConfig::unshieldedChargeDuration},
    {"burstKnockbackSpeed", &PlayerCombatConfig::burstKnockbackSpeed},
    {"burstKnockbackDuration", &PlayerCombatConfig::burstKnockbackDuration},
    {"projectileShieldShakeStrength", &PlayerCombatConfig::projectileShieldShakeStrength},
    {"projectileShieldShakeDuration", &PlayerCombatConfig::projectileShieldShakeDuration},
    {"projectileHitPointShakeStrength", &PlayerCombatConfig::projectileHitPointShakeStrength},
    {"projectileHitPointShakeDuration", &PlayerCombatConfig::projectileHitPointShakeDuration},
    {"chargeHitShakeStrength", &PlayerCombatConfig::chargeHitShakeStrength},
    {"chargeHitShakeDuration", &PlayerCombatConfig::chargeHitShakeDuration},
    {"stepBobFrequency", &PlayerCombatConfig::stepBobFrequency},
    {"stepBobHorizontalAmplitude", &PlayerCombatConfig::stepBobHorizontalAmplitude},
    {"stepBobVerticalAmplitude", &PlayerCombatConfig::stepBobVerticalAmplitude},
    {"stepBobFadeSpeed", &PlayerCombatConfig::stepBobFadeSpeed},
    {"dodgeInitialSpeed", &PlayerCombatConfig::dodgeInitialSpeed},
    {"dodgeBoostDuration", &PlayerCombatConfig::dodgeBoostDuration},
    {"dodgeSlideDuration", &PlayerCombatConfig::dodgeSlideDuration},
    {"dodgeSlideDeceleration", &PlayerCombatConfig::dodgeSlideDeceleration},
    {"dodgeRecoveryDuration", &PlayerCombatConfig::dodgeRecoveryDuration},
    {"dodgeCooldown", &PlayerCombatConfig::dodgeCooldown},
    {"ultimateChargeTime", &PlayerCombatConfig::ultimateChargeTime},
    {"ultimateSpeed", &PlayerCombatConfig::ultimateSpeed},
    {"ultimateMaximumRange", &PlayerCombatConfig::ultimateMaximumRange},
    {"ultimateRadius", &PlayerCombatConfig::ultimateRadius},
    {"ultimateReturnDuration", &PlayerCombatConfig::ultimateReturnDuration},
    {"ultimateSystemOfflineDuration", &PlayerCombatConfig::ultimateSystemOfflineDuration},
    {"ultimateRecoilSpeed", &PlayerCombatConfig::ultimateRecoilSpeed},
    {"ultimateRecoilDuration", &PlayerCombatConfig::ultimateRecoilDuration},
}};
constexpr std::array<std::pair<std::string_view, IntField>, 2> intFields{{
    std::pair<std::string_view, IntField>{"maximumHitPoints", &PlayerCombatConfig::maximumHitPoints},
    {"maximumShieldLayers", &PlayerCombatConfig::maximumShieldLayers},
}};

std::string ToUtf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

template <typename T>
bool ParseNumber(const std::string& text, T& value) {
    std::istringstream stream(text);
    stream.imbue(std::locale::classic());
    char extra = 0;
    return (stream >> value) && !(stream >> extra);
}
}

bool LoadPlayerCombatConfig(const std::filesystem::path& path,
    PlayerCombatConfig& config, std::string& error) {
    error.clear();
    std::ifstream input(path);
    if (!input) {
        error = "Could not open player combat config '" + ToUtf8(path) + "'";
        return false;
    }
    PlayerCombatConfig parsed{};
    std::unordered_set<std::string> seen;
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        std::istringstream fields(line);
        std::string key;
        std::string value;
        std::string extra;
        if (!(fields >> key) || key.starts_with('#')) {
            continue;
        }
        if (!(fields >> value) || (fields >> extra) || !seen.insert(key).second) {
            error = "Invalid or duplicate value in '" + ToUtf8(path) +
                "' line " + std::to_string(lineNumber);
            return false;
        }
        bool recognized = false;
        for (const auto& [name, member] : floatFields) {
            if (key == name) {
                float number = 0.0f;
                if (ParseNumber(value, number) && std::isfinite(number) &&
                    number >= 0.0f && number <= 10000.0f) {
                    parsed.*member = number;
                    recognized = true;
                }
                break;
            }
        }
        for (const auto& [name, member] : intFields) {
            if (key == name) {
                int number = 0;
                if (ParseNumber(value, number) && number >= 1 && number <= 1000) {
                    parsed.*member = number;
                    recognized = true;
                }
                break;
            }
        }
        if (!recognized) {
            error = "Unknown key or invalid value in '" + ToUtf8(path) +
                "' line " + std::to_string(lineNumber) + ": " + key;
            return false;
        }
    }
    if (input.bad()) {
        error = "Could not read player combat config '" + ToUtf8(path) + "'";
        return false;
    }
    for (const auto& [name, member] : floatFields) {
        if (!seen.contains(std::string(name)) ||
            (name != "stepBobHorizontalAmplitude" && name != "stepBobVerticalAmplitude" &&
             (parsed.*member <= 0.0f))) {
            error = "Missing or non-positive key in '" + ToUtf8(path) + "': " + std::string(name);
            return false;
        }
    }
    for (const auto& [name, member] : intFields) {
        if (!seen.contains(std::string(name))) {
            error = "Missing key in '" + ToUtf8(path) + "': " + std::string(name);
            return false;
        }
    }
    config = parsed;
    return true;
}
