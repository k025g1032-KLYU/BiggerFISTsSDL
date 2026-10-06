#include "Gameplay/Combat/EncounterDirector.h"

#include <array>
#include <cstdio>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        return 1;
    }
    EncounterConfig config;
    std::string error;
    if (!LoadEncounterConfig(std::filesystem::path(argv[1]) / "day1-encounter.cfg",
            config, error)) {
        std::printf("FAIL: %s\n", error.c_str());
        return 1;
    }
    EncounterDirector director(config);
    director.Start();
    std::array<int, 3> active{};
    int spawned = 0;
    auto spawn = [&](const EnemySpawnPlan& plan) {
        ++spawned;
        return plan.lane >= 0;
    };
    for (int step = 0; step < 14; ++step) {
        director.Update(1.0f / 60.0f, active, 0, spawn);
    }
    if (spawned != 0) {
        std::puts("FAIL: first spawn occurred before 0.25 seconds");
        return 1;
    }
    director.Update(1.0f / 60.0f, active, 0, spawn);
    if (spawned != 1 || director.GetWaveIndex() != 0) {
        std::puts("FAIL: first enemy did not spawn at 0.25 seconds");
        return 1;
    }
    active[0] = 1;
    director.Update(1.0f / 60.0f, active, 0, spawn);
    active[0] = 0;
    for (int step = 0; step < 180; ++step) {
        director.Update(1.0f / 60.0f, active, 0, spawn);
    }
    if (spawned != 2 || director.GetSpawnedCount(BattleEnemyType::MissileUnit) != 2) {
        std::puts("FAIL: defeated enemy did not respawn after 3 seconds");
        return 1;
    }
    active[0] = 2;
    for (int step = 0; step < 600; ++step) {
        director.Update(1.0f / 60.0f, active, 0, spawn);
    }
    if (spawned != 2) {
        std::puts("FAIL: periodic spawn exceeded active capacity");
        return 1;
    }
    active[0] = 1;
    for (int step = 0; step < 180; ++step) {
        director.Update(1.0f / 60.0f, active, 0, spawn);
    }
    if (spawned != 3) {
        std::puts("FAIL: replacement did not spawn after capacity opened");
        return 1;
    }
    active[0] = 2;
    for (int step = 0; step < 600; ++step) {
        director.Update(1.0f / 60.0f, active, 0, spawn);
    }
    if (spawned != 3) {
        std::puts("FAIL: active capacity exceeded");
        return 1;
    }
    director.Reset();
    if (director.IsComplete() || director.GetSpawnedCount(BattleEnemyType::MissileUnit) != 0) {
        std::puts("FAIL: reset retained battle progress");
        return 1;
    }
    std::puts("PASS: delayed spawn, periodic spawn, capacity and reset");
    return 0;
}
