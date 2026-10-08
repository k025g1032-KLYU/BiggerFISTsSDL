#include "Data/CombatFeedbackConfig.h"

#include <algorithm>
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
    "chargeHitRumbleMs", "dodgeWav", "dodgeGain",
    "ultimateLaunchGain", "dodgeRumbleLow", "dodgeRumbleHigh",
    "dodgeRumbleMs", "bossChangeModeWav", "bossSharpHitWav",
    "bossRiserWav", "battleExplosionWav", "battleCueGain",
    "systemRestartRumbleLow",
    "systemRestartRumbleHigh", "systemRestartRumbleMs"};
constexpr std::array cueNames{
    "landing", "chargePrepare", "chargeDash", "missileLaunch",
    "bossDash", "bossStrike",
    "bossNeedle", "bossBarrage", "playerShield", "playerHealth",
    "cockpitStart", "systemOffline", "engineLoop", "footstep",
    "projectileExplosion"};

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
        if (key == "cue") {
            std::string name;
            std::string wav;
            std::string gainText;
            float gain = 0.0f;
            if (!(fields >> name >> wav >> gainText) || (fields >> extra) ||
                std::find(cueNames.begin(), cueNames.end(), name) ==
                    cueNames.end() ||
                !ValidWavName(wav) || !ParseNumber(gainText, gain) ||
                !std::isfinite(gain) || gain < 0.0f || gain > 2.0f ||
                !parsed.cues.emplace(name, SoundCueConfig{wav, gain}).second) {
                error = "Invalid or duplicate cue in '" + ToUtf8(path) +
                    "' line " + std::to_string(lineNumber);
                return false;
            }
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
        } else if (key == "dodgeWav" && ValidWavName(value)) {
            parsed.dodgeWav = value;
        } else if (key == "bossChangeModeWav" && ValidWavName(value)) {
            parsed.bossChangeModeWav = value;
        } else if (key == "bossSharpHitWav" && ValidWavName(value)) {
            parsed.bossSharpHitWav = value;
        } else if (key == "bossRiserWav" && ValidWavName(value)) {
            parsed.bossRiserWav = value;
        } else if (key == "battleExplosionWav" && ValidWavName(value)) {
            parsed.battleExplosionWav = value;
        } else if ((key == "punchImpactGain" || key == "rocketImpactGain" ||
                key == "rocketLaunchGain" || key == "punchSwingGain" ||
                key == "rocketReconnectGain" || key == "chargeHitGain" ||
                key == "dodgeGain" || key == "ultimateLaunchGain" ||
                key == "battleCueGain") &&
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
            } else if (key == "dodgeGain") {
                parsed.dodgeGain = gain;
            } else if (key == "ultimateLaunchGain") {
                parsed.ultimateLaunchGain = gain;
            } else if (key == "battleCueGain") {
                parsed.battleCueGain = gain;
            } else {
                parsed.chargeHitGain = gain;
            }
        } else if ((key == "punchRumbleLow" || key == "punchRumbleHigh" ||
                key == "rocketLaunchRumbleLow" || key == "rocketLaunchRumbleHigh" ||
                key == "projectileShieldRumbleLow" || key == "projectileShieldRumbleHigh" ||
                key == "projectileHitPointRumbleLow" || key == "projectileHitPointRumbleHigh" ||
                key == "chargeHitRumbleLow" || key == "chargeHitRumbleHigh" ||
                key == "dodgeRumbleLow" || key == "dodgeRumbleHigh" ||
                key == "systemRestartRumbleLow" ||
                key == "systemRestartRumbleHigh") &&
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
            } else if (key == "dodgeRumbleLow") {
                parsed.dodgeRumbleLow = static_cast<std::uint16_t>(whole);
            } else if (key == "dodgeRumbleHigh") {
                parsed.dodgeRumbleHigh = static_cast<std::uint16_t>(whole);
            } else if (key == "systemRestartRumbleLow") {
                parsed.systemRestartRumbleLow = static_cast<std::uint16_t>(whole);
            } else if (key == "systemRestartRumbleHigh") {
                parsed.systemRestartRumbleHigh = static_cast<std::uint16_t>(whole);
            } else {
                parsed.chargeHitRumbleHigh = static_cast<std::uint16_t>(whole);
            }
        } else if ((key == "punchRumbleMs" || key == "rocketLaunchRumbleMs" ||
                key == "projectileRumbleMs" || key == "chargeHitRumbleMs" ||
                key == "dodgeRumbleMs" || key == "systemRestartRumbleMs") &&
            ParseNumber(value, whole) && whole >= 1 && whole <= 5000) {
            if (key == "punchRumbleMs") {
                parsed.punchRumbleMs = whole;
            } else if (key == "rocketLaunchRumbleMs") {
                parsed.rocketLaunchRumbleMs = whole;
            } else if (key == "projectileRumbleMs") {
                parsed.projectileRumbleMs = whole;
            } else if (key == "dodgeRumbleMs") {
                parsed.dodgeRumbleMs = whole;
            } else if (key == "systemRestartRumbleMs") {
                parsed.systemRestartRumbleMs = whole;
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
    for (const char* name : cueNames) {
        if (!parsed.cues.contains(name)) {
            error = "Missing cue in '" + ToUtf8(path) + "': " + name;
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
