#pragma once

#include "Gameplay/Combat/EncounterConfig.h"

#include <array>
#include <cstddef>
#include <functional>
#include <vector>

class EncounterDirector {
public:
    using Spawn = std::function<bool(const EnemySpawnPlan&)>;

    explicit EncounterDirector(EncounterConfig config = {});
    void SetConfig(EncounterConfig config);
    void Start();
    void Reset();
    void Update(float seconds, std::array<int, 3> activeByType,
        std::size_t projectileCount, const Spawn& spawn);

    bool IsComplete() const { return complete_; }
    bool IsInterWave() const { return interWave_; }
    int GetWaveIndex() const { return waveIndex_; }
    int GetWaveCount() const { return static_cast<int>(config_.waves.size()); }
    int GetSpawnedCount(BattleEnemyType type) const {
        return spawned_[BattleEnemyTypeIndex(type)];
    }
    const BattleWave* GetCurrentWave() const;

private:
    bool CanSpawn(BattleEnemyType type, const std::array<int, 3>& active) const;
    bool TrySpawn(const EnemySpawnPlan& plan, std::array<int, 3>& active,
        const Spawn& spawn);
    bool InitialComplete() const;
    bool AllSpawned() const;
    void BeginWave(int index);

    EncounterConfig config_;
    int waveIndex_ = 0;
    std::array<int, 3> spawned_{};
    std::array<int, 3> initialSpawned_{};
    std::array<int, 3> previousActive_{};
    std::array<std::vector<float>, 3> respawnTimers_{};
    std::size_t initialPlanIndex_ = 0;
    std::size_t periodicPlanIndex_ = 0;
    float spawnTimer_ = 0.0f;
    float interWaveTimer_ = 0.0f;
    bool started_ = false;
    bool initialComplete_ = false;
    bool interWave_ = false;
    bool complete_ = false;
};
