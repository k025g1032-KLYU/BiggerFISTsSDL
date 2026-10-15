#pragma once

#include "Data/BattleCombatConfig.h"
#include "Game/FirstPersonGame.h"
#include "Gameplay/Tutorial/TutorialSession.h"

#include <optional>

struct TutorialProjectileImpact {
    Vector3 position{};
    float scale = 1.0f;
    bool shieldAbsorbed = false;
};

class TutorialEncounter {
public:
    explicit TutorialEncounter(BattleCombatConfig config = {}) : config_(config) {}

    void Start(FirstPersonGame& player);
    void EnterStep(TutorialStep step, FirstPersonGame& player);
    void ResetTargets(FirstPersonGame& player);
    void UpdateBeforePlayer(float seconds, const TutorialSession& tutorial,
        FirstPersonGame& player);
    bool ResolveAfterPlayer(FirstPersonGame& player);
    std::optional<TutorialProjectileImpact> ConsumeProjectileImpact();

    bool HasTrainingMissile() const { return missileActive_; }
    static constexpr float kTargetRespawnDelay = 0.8f;
    static constexpr float kDefenseInitialDelay = 1.0f;
    static constexpr float kDefenseRespawnDelay = 1.2f;

private:
    void RemoveTrainingMissile(FirstPersonGame& player);
    void SpawnTrainingMissile(FirstPersonGame& player);
    void MoveTrainingMissile(float seconds, FirstPersonGame& player);
    int CountActiveTargets(const FirstPersonGame& player) const;

    BattleCombatConfig config_{};
    TutorialStep step_ = TutorialStep::VisualLink;
    float targetRespawnRemaining_ = kTargetRespawnDelay;
    float defenseSpawnRemaining_ = 0.0f;
    bool missileActive_ = false;
    std::optional<TutorialProjectileImpact> projectileImpact_;
    Vector3 missilePosition_{};
    Vector3 previousMissilePosition_{};
    Vector3 missileDirection_{0.0f, 0.0f, -1.0f};
    Vector3 missileTurnStartDirection_{0.0f, 0.0f, -1.0f};
    Vector3 missileTargetPosition_{};
    float missileTurnElapsed_ = 0.0f;
    enum class MissileFlightState { Outbound, Turning, BaseRun };
    MissileFlightState missileFlightState_ = MissileFlightState::Outbound;
};
