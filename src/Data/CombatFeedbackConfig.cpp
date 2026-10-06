#include "Data/CombatFeedbackConfig.h"

#include <array>
#include <cmath>
#include <fstream>
#include <locale>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace {
constexpr std::array requiredKeys{
    "impactWav", "rocketLaunchWav", "punchImpactGain", "rocketImpactGain",
    "rocketLaunchGain", "punchRumbleLow", "punchRumbleHigh", "punchRumbleMs",
    "rocketLaunchRumbleLow", "rocketLaunchRumbleHigh", "rocketLaunchRumbleMs",
    "punchSwingWav", "rocketReconnectWav", "chargeHitWav",
    "punchSwingGain", "rocketReconnectGain", "chargeHitGain",
    "projectileShieldRumbleLow", "projectileShieldRumbleHigh",
    "projectileHitPointRumbleLow", "projectileHitPointRumbleHigh",
    "projectileRumbleMs", "chargeHitRumbleLow", "chargeHitRumbleHigh",
    "chargeHitRumbleMs"};

std::string ToUtf8(const std::filesystem::path& path) {
    const std::u8string value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

template <typename Number>
bool ParseNumber(const std::string& text, Number& number) {
    std::istringstream input(text);
    input.imbue(std::locale::classic());
    if (!(input >> number)) {
        return false;
    }
    char extra = 0;
    return !(input >> extra);
}

bool ValidWavName(const std::string& value) {
    const std::filesystem::path path(value);
    return !value.empty() && path.filename() == path &&
        path.extension() == ".wav" && value != "." && value != "..";
}
}

bool LoadCombatFeedbackConfig(const std::filesystem::path& path,
    CombatFeedbackConfig& config, std::string& error) {
    error.clear();
    std::ifstream input(path);
    if (!input) {
        error = "Could not open combat feedback config '" + ToUtf8(path) + "'";
        return false;
    }
    CombatFeedbackConfig parsed{};
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
        float gain = 0.0f;
        unsigned int whole = 0;
        if (key == "impactWav" && ValidWavName(value)) {
            parsed.impactWav = value;
        } else if (key == "rocketLaunchWav" && ValidWavName(value)) {
            parsed.rocketLaunchWav = value;
        } else if (key == "punchSwingWav" && ValidWavName(value)) {
            parsed.punchSwingWav = value;
        } else if (key == "rocketReconnectWav" && ValidWavName(value)) {
            parsed.rocketReconnectWav = value;
        } else if (key == "chargeHitWav" && ValidWavName(value)) {
            parsed.chargeHitWav = value;
        } else if ((key == "punchImpactGain" || key == "rocketImpactGain" ||
                key == "rocketLaunchGain" || key == "punchSwingGain" ||
                key == "rocketReconnectGain" || key == "chargeHitGain") &&
            ParseNumber(value, gain) &&
            std::isfinite(gain) && gain >= 0.0f && gain <= 2.0f) {
            if (key == "punchImpactGain") {
                parsed.punchImpactGain = gain;
            } else if (key == "rocketImpactGain") {
                parsed.rocketImpactGain = gain;
            } else if (key == "rocketLaunchGain") {
                parsed.rocketLaunchGain = gain;
            } else if (key == "punchSwingGain") {
                parsed.punchSwingGain = gain;
            } else if (key == "rocketReconnectGain") {
                parsed.rocketReconnectGain = gain;
            } else {
                parsed.chargeHitGain = gain;
            }
        } else if ((key == "punchRumbleLow" || key == "punchRumbleHigh" ||
                key == "rocketLaunchRumbleLow" || key == "rocketLaunchRumbleHigh" ||
                key == "projectileShieldRumbleLow" || key == "projectileShieldRumbleHigh" ||
                key == "projectileHitPointRumbleLow" || key == "projectileHitPointRumbleHigh" ||
                key == "chargeHitRumbleLow" || key == "chargeHitRumbleHigh") &&
            ParseNumber(value, whole) && whole <= 65535) {
            if (key == "punchRumbleLow") {
                parsed.punchRumbleLow = static_cast<std::uint16_t>(whole);
            } else if (key == "punchRumbleHigh") {
                parsed.punchRumbleHigh = static_cast<std::uint16_t>(whole);
            } else if (key == "rocketLaunchRumbleLow") {
                parsed.rocketLaunchRumbleLow = static_cast<std::uint16_t>(whole);
            } else if (key == "rocketLaunchRumbleHigh") {
                parsed.rocketLaunchRumbleHigh = static_cast<std::uint16_t>(whole);
            } else if (key == "projectileShieldRumbleLow") {
                parsed.projectileShieldRumbleLow = static_cast<std::uint16_t>(whole);
            } else if (key == "projectileShieldRumbleHigh") {
                parsed.projectileShieldRumbleHigh = static_cast<std::uint16_t>(whole);
            } else if (key == "projectileHitPointRumbleLow") {
                parsed.projectileHitPointRumbleLow = static_cast<std::uint16_t>(whole);
            } else if (key == "projectileHitPointRumbleHigh") {
                parsed.projectileHitPointRumbleHigh = static_cast<std::uint16_t>(whole);
            } else if (key == "chargeHitRumbleLow") {
                parsed.chargeHitRumbleLow = static_cast<std::uint16_t>(whole);
            } else {
                parsed.chargeHitRumbleHigh = static_cast<std::uint16_t>(whole);
            }
        } else if ((key == "punchRumbleMs" || key == "rocketLaunchRumbleMs" ||
                key == "projectileRumbleMs" || key == "chargeHitRumbleMs") &&
            ParseNumber(value, whole) && whole >= 1 && whole <= 5000) {
            if (key == "punchRumbleMs") {
                parsed.punchRumbleMs = whole;
            } else if (key == "rocketLaunchRumbleMs") {
                parsed.rocketLaunchRumbleMs = whole;
            } else if (key == "projectileRumbleMs") {
                parsed.projectileRumbleMs = whole;
            } else {
                parsed.chargeHitRumbleMs = whole;
            }
        } else {
            error = "Unknown key or invalid value in '" + ToUtf8(path) +
                "' line " + std::to_string(lineNumber) + ": " + key;
            return false;
        }
    }
    for (const char* key : requiredKeys) {
        if (!seen.contains(key)) {
            error = "Missing key in '" + ToUtf8(path) + "': " + key;
            return false;
        }
    }
    if (input.bad()) {
        error = "Could not read combat feedback config '" + ToUtf8(path) + "'";
        return false;
    }
    config = std::move(parsed);
    return true;
}
