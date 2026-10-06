#include "Gameplay/Combat/EncounterConfig.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <utility>

namespace {
std::string ToUtf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}

bool IsValidWave(const BattleWave& wave) {
    if (wave.id.empty() || wave.maximumActive <= 0 ||
        !std::isfinite(wave.firstSpawnDelay) || wave.firstSpawnDelay < 0.0f ||
        !std::isfinite(wave.periodicSpawnInterval) || wave.periodicSpawnInterval < 0.0f ||
        !std::isfinite(wave.defeatRespawnDelay) || wave.defeatRespawnDelay < 0.0f ||
        !std::isfinite(wave.interWaveDelay) || wave.interWaveDelay < 0.0f) {
        return false;
    }
    for (const int cap : wave.maximumActiveByType) {
        if (cap < 0 || cap > wave.maximumActive) {
            return false;
        }
    }
    return true;
}
}

bool LoadEncounterConfig(const std::filesystem::path& path,
    EncounterConfig& config, std::string& error) {
    error.clear();
    std::ifstream input(path);
    if (!input) {
        error = "Could not open encounter config '" + ToUtf8(path) + "'";
        return false;
    }
    EncounterConfig parsed;
    BattleWave current;
    bool insideWave = false;
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        std::istringstream fields(line);
        std::string command;
        if (!(fields >> command) || command.starts_with('#')) {
            continue;
        }
        std::string extra;
        bool valid = false;
        if (command == "wave" && !insideWave) {
            int wait = -1;
            if ((fields >> current.id >> current.maximumActive >>
                    current.maximumActiveByType[0] >> current.maximumActiveByType[1] >>
                    current.maximumActiveByType[2] >> current.firstSpawnDelay >>
                    current.periodicSpawnInterval >> current.defeatRespawnDelay >>
                    current.interWaveDelay >> wait) && !(fields >> extra) &&
                (wait == 0 || wait == 1) && IsValidWave(current) &&
                std::none_of(parsed.waves.begin(), parsed.waves.end(),
                    [&](const BattleWave& wave) { return wave.id == current.id; })) {
                current.waitForProjectiles = wait != 0;
                insideWave = true;
                valid = true;
            }
        } else if (command == "spawn" && insideWave) {
            std::string type;
            EnemySpawnPlan spawn;
            if ((fields >> type >> spawn.total >> spawn.initial >> spawn.lane) &&
                !(fields >> extra) && spawn.total > 0 && spawn.initial >= 0 &&
                spawn.initial <= spawn.total && spawn.lane >= 0 && spawn.lane < 5) {
                if (type == "MissileUnit") {
                    spawn.type = BattleEnemyType::MissileUnit;
                } else if (type == "Bomber") {
                    spawn.type = BattleEnemyType::Bomber;
                } else if (type == "ChargeUnit") {
                    spawn.type = BattleEnemyType::ChargeUnit;
                } else {
                    error = "Unknown enemy type in '" + ToUtf8(path) +
                        "' line " + std::to_string(lineNumber) + ": " + type;
                    return false;
                }
                const std::size_t index = BattleEnemyTypeIndex(spawn.type);
                bool duplicate = false;
                for (const EnemySpawnPlan& existing : current.spawns) {
                    duplicate |= existing.type == spawn.type;
                }
                if (!duplicate && current.maximumActiveByType[index] > 0) {
                    current.spawns.push_back(spawn);
                    valid = true;
                }
            }
        } else if (command == "endwave" && insideWave && !(fields >> extra)) {
            if (!current.spawns.empty()) {
                parsed.waves.push_back(std::move(current));
                current = {};
                insideWave = false;
                valid = true;
            }
        }
        if (!valid) {
            error = "Invalid encounter config '" + ToUtf8(path) +
                "' line " + std::to_string(lineNumber) + ": " + command;
            return false;
        }
    }
    if (input.bad() || insideWave || parsed.waves.empty()) {
        error = "Incomplete encounter config '" + ToUtf8(path) + "'";
        return false;
    }
    config = std::move(parsed);
    return true;
}
