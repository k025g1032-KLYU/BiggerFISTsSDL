#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

struct CombatFeedbackConfig {
    std::string impactWav;
    std::string rocketLaunchWav;
    float punchImpactGain = 0.0f;
    float rocketImpactGain = 0.0f;
    float rocketLaunchGain = 0.0f;
    std::uint16_t punchRumbleLow = 0;
    std::uint16_t punchRumbleHigh = 0;
    std::uint32_t punchRumbleMs = 0;
    std::uint16_t rocketLaunchRumbleLow = 0;
    std::uint16_t rocketLaunchRumbleHigh = 0;
    std::uint32_t rocketLaunchRumbleMs = 0;
};

bool LoadCombatFeedbackConfig(const std::filesystem::path& path,
    CombatFeedbackConfig& config, std::string& error);
