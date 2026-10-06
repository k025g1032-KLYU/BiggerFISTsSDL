#include "Gameplay/Combat/EncounterDirector.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace {
constexpr float kInitialSpawnInterval = 0.55f;
constexpr float kTimeEpsilon = 0.00001f;
}

EncounterDirector::EncounterDirector(EncounterConfig config) : config_(std::move(config)) {
}

void EncounterDirector::SetConfig(EncounterConfig config) {
    config_ = std::move(config);
    Reset();
}

void EncounterDirector::Reset() {
    waveIndex_ = 0;
    spawned_.fill(0);
    initialSpawned_.fill(0);
    previousActive_.fill(0);
    for (auto& timers : respawnTimers_) {
        timers.clear();
    }
    initialPlanIndex_ = 0;
    periodicPlanIndex_ = 0;
    spawnTimer_ = 0.0f;
    interWaveTimer_ = 0.0f;
    started_ = false;
    initialComplete_ = false;
    interWave_ = false;
    complete_ = false;
}

void EncounterDirector::Start() {
    Reset();
    started_ = true;
    if (config_.waves.empty()) {
        complete_ = true;
    } else {
        BeginWave(0);
    }
}

const BattleWave* EncounterDirector::GetCurrentWave() const {
    return waveIndex_ >= 0 && waveIndex_ < GetWaveCount()
        ? &config_.waves[waveIndex_] : nullptr;
}

void EncounterDirector::BeginWave(int index) {
    waveIndex_ = index;
    spawned_.fill(0);
    initialSpawned_.fill(0);
    previousActive_.fill(0);
    for (auto& timers : respawnTimers_) {
        timers.clear();
    }
    initialPlanIndex_ = 0;
    periodicPlanIndex_ = 0;
    initialComplete_ = false;
    interWave_ = false;
    interWaveTimer_ = 0.0f;
    spawnTimer_ = config_.waves[index].firstSpawnDelay;
}

bool EncounterDirector::CanSpawn(BattleEnemyType type,
    const std::array<int, 3>& active) const {
    const BattleWave* wave = GetCurrentWave();
    if (wave == nullptr) {
        return false;
    }
    const int total = active[0] + active[1] + active[2];
    const std::size_t index = BattleEnemyTypeIndex(type);
    return total < wave->maximumActive &&
        active[index] < wave->maximumActiveByType[index];
}

bool EncounterDirector::TrySpawn(const EnemySpawnPlan& plan,
    std::array<int, 3>& active, const Spawn& spawn) {
    if (!CanSpawn(plan.type, active) || !spawn || !spawn(plan)) {
        return false;
    }
    const std::size_t index = BattleEnemyTypeIndex(plan.type);
    ++spawned_[index];
    ++active[index];
    return true;
}

bool EncounterDirector::InitialComplete() const {
    const BattleWave* wave = GetCurrentWave();
    if (wave == nullptr) {
        return true;
    }
    for (const EnemySpawnPlan& plan : wave->spawns) {
        if (initialSpawned_[BattleEnemyTypeIndex(plan.type)] < plan.initial) {
            return false;
        }
    }
    return true;
}

bool EncounterDirector::AllSpawned() const {
    const BattleWave* wave = GetCurrentWave();
    if (wave == nullptr) {
        return true;
    }
    for (const EnemySpawnPlan& plan : wave->spawns) {
        if (spawned_[BattleEnemyTypeIndex(plan.type)] < plan.total) {
            return false;
        }
    }
    return true;
}

void EncounterDirector::Update(float seconds, std::array<int, 3> active,
    std::size_t projectileCount, const Spawn& spawn) {
    if (!started_ || complete_ || !std::isfinite(seconds) || seconds < 0.0f ||
        std::any_of(active.begin(), active.end(), [](int count) { return count < 0; })) {
        return;
    }
    if (interWave_) {
        interWaveTimer_ = std::max(0.0f, interWaveTimer_ - seconds);
        if (interWaveTimer_ <= kTimeEpsilon) {
            BeginWave(waveIndex_ + 1);
        }
        return;
    }
    const BattleWave* wave = GetCurrentWave();
    if (wave == nullptr) {
        complete_ = true;
        return;
    }
    for (const EnemySpawnPlan& plan : wave->spawns) {
        const std::size_t index = BattleEnemyTypeIndex(plan.type);
        auto& timers = respawnTimers_[index];
        const int defeated = std::max(0, previousActive_[index] - active[index]);
        const int available = std::max(0, plan.total - spawned_[index] -
            static_cast<int>(timers.size()));
        for (int count = 0; count < std::min(defeated, available); ++count) {
            timers.push_back(wave->defeatRespawnDelay);
        }
    }
    for (auto& timers : respawnTimers_) {
        for (float& timer : timers) {
            timer = std::max(0.0f, timer - seconds);
        }
    }

    bool completedInitialThisStep = false;
    if (!initialComplete_) {
        spawnTimer_ = std::max(0.0f, spawnTimer_ - seconds);
        if (spawnTimer_ <= kTimeEpsilon) {
            bool spawned = false;
            for (std::size_t offset = 0; offset < wave->spawns.size(); ++offset) {
                const std::size_t planIndex =
                    (initialPlanIndex_ + offset) % wave->spawns.size();
                const EnemySpawnPlan& plan = wave->spawns[planIndex];
                const std::size_t index = BattleEnemyTypeIndex(plan.type);
                if (initialSpawned_[index] >= plan.initial ||
                    spawned_[index] >= plan.total || !TrySpawn(plan, active, spawn)) {
                    continue;
                }
                ++initialSpawned_[index];
                initialPlanIndex_ = (planIndex + 1) % wave->spawns.size();
                spawned = true;
                break;
            }
            if (InitialComplete()) {
                initialComplete_ = true;
                completedInitialThisStep = true;
                spawnTimer_ = wave->periodicSpawnInterval;
            } else if (spawned) {
                spawnTimer_ = kInitialSpawnInterval;
            }
        }
    }

    if (initialComplete_) {
        for (const EnemySpawnPlan& plan : wave->spawns) {
            const std::size_t index = BattleEnemyTypeIndex(plan.type);
            auto& timers = respawnTimers_[index];
            for (auto timer = timers.begin(); timer != timers.end();) {
                if (*timer > kTimeEpsilon || !CanSpawn(plan.type, active)) {
                    ++timer;
                } else if (spawned_[index] >= plan.total ||
                    TrySpawn(plan, active, spawn)) {
                    timer = timers.erase(timer);
                } else {
                    ++timer;
                }
            }
        }
        if (wave->periodicSpawnInterval > kTimeEpsilon &&
            !completedInitialThisStep) {
            spawnTimer_ = std::max(0.0f, spawnTimer_ - seconds);
            if (spawnTimer_ <= kTimeEpsilon) {
                for (std::size_t offset = 0; offset < wave->spawns.size(); ++offset) {
                    const std::size_t planIndex =
                        (periodicPlanIndex_ + offset) % wave->spawns.size();
                    const EnemySpawnPlan& plan = wave->spawns[planIndex];
                    const std::size_t index = BattleEnemyTypeIndex(plan.type);
                    if (spawned_[index] + static_cast<int>(respawnTimers_[index].size()) >=
                            plan.total || !TrySpawn(plan, active, spawn)) {
                        continue;
                    }
                    periodicPlanIndex_ = (planIndex + 1) % wave->spawns.size();
                    break;
                }
                spawnTimer_ = wave->periodicSpawnInterval;
            }
        }
    }
    previousActive_ = active;
    if (!AllSpawned() || active[0] + active[1] + active[2] > 0 ||
        (wave->waitForProjectiles && projectileCount > 0)) {
        return;
    }
    if (waveIndex_ + 1 >= GetWaveCount()) {
        complete_ = true;
    } else {
        interWave_ = true;
        interWaveTimer_ = wave->interWaveDelay;
    }
}
