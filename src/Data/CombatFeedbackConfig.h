#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

struct CombatFeedbackConfig {
    std::string impactWav;
    std::string rocketLaunchWav;
    std::string punchSwingWav;
    std::string rocketReconnectWav;
    std::string chargeHitWav;
    std::string dodgeWav;
    float punchImpactGain = 0.0f;
    float rocketImpactGain = 0.0f;
    float rocketLaunchGain = 0.0f;
    float punchSwingGain = 0.0f;
    float rocketReconnectGain = 0.0f;
    float chargeHitGain = 0.0f;
    float dodgeGain = 0.0f;
    float ultimateLaunchGain = 0.0f;
    std::uint16_t punchRumbleLow = 0;
    std::uint16_t punchRumbleHigh = 0;
    std::uint32_t punchRumbleMs = 0;
    std::uint16_t rocketLaunchRumbleLow = 0;
    std::uint16_t rocketLaunchRumbleHigh = 0;
    std::uint32_t rocketLaunchRumbleMs = 0;
    std::uint16_t projectileShieldRumbleLow = 0;
    std::uint16_t projectileShieldRumbleHigh = 0;
    std::uint16_t projectileHitPointRumbleLow = 0;
    std::uint16_t projectileHitPointRumbleHigh = 0;
    std::uint32_t projectileRumbleMs = 0;
    std::uint16_t chargeHitRumbleLow = 0;
    std::uint16_t chargeHitRumbleHigh = 0;
    std::uint32_t chargeHitRumbleMs = 0;
    std::uint16_t dodgeRumbleLow = 0;
    std::uint16_t dodgeRumbleHigh = 0;
    std::uint32_t dodgeRumbleMs = 0;
    std::uint16_t systemRestartRumbleLow = 0;
    std::uint16_t systemRestartRumbleHigh = 0;
    std::uint32_t systemRestartRumbleMs = 0;
};

bool LoadCombatFeedbackConfig(const std::filesystem::path& path,
    CombatFeedbackConfig& config, std::string& error);
