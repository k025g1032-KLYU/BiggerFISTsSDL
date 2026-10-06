#pragma once

#include <array>
#include <filesystem>
#include <string>
#include <vector>

enum class BattleEnemyType { MissileUnit, Bomber, ChargeUnit };

constexpr std::size_t BattleEnemyTypeIndex(BattleEnemyType type) {
    return static_cast<std::size_t>(type);
}

struct EnemySpawnPlan {
    BattleEnemyType type = BattleEnemyType::MissileUnit;
    int total = 0;
    int initial = 0;
    int lane = 0;
};

struct BattleWave {
    std::string id;
    int maximumActive = 0;
    std::array<int, 3> maximumActiveByType{};
    float firstSpawnDelay = 0.0f;
    float periodicSpawnInterval = 0.0f;
    float defeatRespawnDelay = 0.0f;
    float interWaveDelay = 0.0f;
    bool waitForProjectiles = true;
    std::vector<EnemySpawnPlan> spawns;
};

struct EncounterConfig {
    std::vector<BattleWave> waves;
};

bool LoadEncounterConfig(const std::filesystem::path& path,
    EncounterConfig& config, std::string& error);
