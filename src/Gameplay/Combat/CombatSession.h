#pragma once

#include "Game/FirstPersonGame.h"
#include "Gameplay/Combat/BattleMode.h"
#include "Gameplay/Combat/EncounterDirector.h"
#include "Gameplay/Combat/BattleBoss.h"
#include "Gameplay/Combat/BattleFeedbackEvent.h"
#include "Data/BattleCombatConfig.h"

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

enum class BattlePhase { Ready, Countdown, Playing, Clear, GameOver };
enum class BattleProjectileType { Missile, FloatingBomb, CrystalSpike, CrystalCube };

struct BattleEnemy {
    std::string id;
    BattleEnemyType type = BattleEnemyType::MissileUnit;
    Vector3 position{};
    Vector3 previousPosition{};
    Vector3 velocity{};
    float attackTimer = 0.0f;
    float stateTimer = 0.0f;
    float visualAge = 0.0f;
    float visualYaw = 4.71238898f;
    float carriedBombScale = 1.0f;
    enum class MissileState { Patrolling, FireWindup, FireRecovery }
        missileState = MissileState::Patrolling;
    float travelSpeed = 0.0f;
    float combatHeight = 4.55f;
    float rotation = 0.0f;
    int lane = 0;
    bool deployed = false;
    enum class DeploymentState { Dropping, Landed, TakingOff, Deployed }
        deploymentState = DeploymentState::Dropping;
    bool defeated = false;
    enum class ChargeState {
        Approaching, Repositioning, ReacquiringTarget, Preparing, Charging,
        Impact, HitBraking, MissBraking
    } chargeState = ChargeState::Approaching;
    Vector3 chargeDirection{0.0f, 0.0f, -1.0f};
    Vector3 chargeTarget{};
    float chargeTimer = 0.0f;
    bool chargeDamageActiveThisStep = false;
    float chargeBrakingSpeed = 0.0f;
};

struct BattleProjectile {
    std::string id;
    BattleProjectileType type = BattleProjectileType::Missile;
    Vector3 position{};
    Vector3 previousPosition{};
    Vector3 forwardPosition{};
    float age = 0.0f;
    float phase = 0.0f;
    Vector3 direction{0.0f, 0.0f, -1.0f};
    Vector3 turnStartDirection{0.0f, 0.0f, -1.0f};
    enum class MissileFlightState { Outbound, Turning, BaseRun }
        missileFlightState = MissileFlightState::Outbound;
    float turnTimer = 0.0f;
};

enum class BattleVisualEffect { ShieldHit, Explosion };

struct BattleVisualEvent {
    BattleVisualEffect effect = BattleVisualEffect::Explosion;
    Vector3 position{};
    float scale = 1.0f;
    int shieldLayers = 0;
};

class CombatSession {
public:
    CombatSession(BattlePreviewKind kind, FirstPersonGame& player,
        EncounterConfig encounter = {}, BattleCombatConfig config = {});
    void Start(bool skipCountdown = false);
    void Reset();
    void Update(float seconds, const GameInput& input, float mouseX, float mouseY);
    bool ApplyTargetDamage(std::string_view id, int damage);

    BattlePhase GetPhase() const { return phase_; }
    int GetBaseHitPoints() const { return baseHitPoints_; }
    int GetWaveIndex() const { return director_.GetWaveIndex(); }
    int GetWaveCount() const { return director_.GetWaveCount(); }
    const std::vector<BattleEnemy>& GetEnemies() const { return enemies_; }
    const std::vector<BattleProjectile>& GetProjectiles() const { return projectiles_; }
    const EncounterDirector& GetDirector() const { return director_; }
    const BattleBoss* GetBoss() const { return boss_ ? &*boss_ : nullptr; }
    const FirstPersonGame& GetPlayerGame() const { return player_; }
    const std::vector<BattleFeedbackEvent>& GetFeedbackEvents() const {
        return feedbackEvents_;
    }
    const std::vector<BattleVisualEvent>& GetVisualEvents() const {
        return visualEvents_;
    }

private:
    void BeginPlaying();
    bool SpawnEnemy(const EnemySpawnPlan& plan);
    void SpawnProjectile(BattleProjectileType type, const Vector3& position,
        const Vector3& direction = {0.0f, 0.0f, -1.0f}, float phase = 0.0f);
    void UpdateEnemies(float seconds);
    void UpdateChargeEnemy(BattleEnemy& enemy, float seconds);
    void UpdateBoss(float seconds);
    void UpdateProjectiles(float seconds);
    void ResolveProjectileImpacts();
    void ResolveChargeImpacts();
    void ProcessPlayerHits();
    void ProcessUltimateHits();
    PlayerHitResult ReceivePlayerHit(PlayerHitType type,
        const Vector3& pushDirection = {0.0f, 0.0f, 1.0f});
    std::array<int, 3> CountActiveEnemies() const;
    std::string NextTargetId();

    BattlePreviewKind kind_ = BattlePreviewKind::None;
    FirstPersonGame& player_;
    EncounterDirector director_;
    BattleCombatConfig config_;
    BattlePhase phase_ = BattlePhase::Ready;
    std::vector<BattleEnemy> enemies_;
    std::vector<BattleProjectile> projectiles_;
    std::optional<BattleBoss> boss_;
    std::uint64_t nextId_ = 1;
    std::mt19937 random_{0xB16F157u};
    float countdown_ = 0.0f;
    int baseHitPoints_ = 10;
    std::unordered_set<std::string> ultimateHitIds_;
    std::unordered_set<std::uint64_t> bossAttackHits_;
    std::vector<BattleFeedbackEvent> feedbackEvents_;
    std::vector<BattleVisualEvent> visualEvents_;
};
