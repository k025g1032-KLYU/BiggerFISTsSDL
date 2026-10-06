#include "Gameplay/Combat/EncounterConfig.h"

#include <cstdio>
#include <filesystem>
#include <string>

int main(int argc, char** argv) {
    if (argc != 2) {
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    EncounterConfig day1;
    EncounterConfig day2;
    std::string error;
    if (!LoadEncounterConfig(directory / "day1-encounter.cfg", day1, error) ||
        !LoadEncounterConfig(directory / "day2-encounter.cfg", day2, error)) {
        std::printf("FAIL: %s\n", error.c_str());
        return 1;
    }
    if (day1.waves.size() != 3 || day2.waves.size() != 3 ||
        day1.waves[0].spawns[0].total != 4 ||
        day1.waves[2].maximumActiveByType[1] != 1 ||
        day2.waves[2].maximumActive != 6 ||
        day2.waves[2].spawns[2].type != BattleEnemyType::ChargeUnit ||
        day2.waves[2].spawns[2].initial != 2) {
        std::puts("FAIL: encounter data differs from the original wave catalog");
        return 1;
    }
    std::puts("PASS: Day1 and Day2 original wave plans load");
    return 0;
}
