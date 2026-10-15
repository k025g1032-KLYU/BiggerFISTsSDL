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
enum class BattleAttackType { Punch, RocketPunch, Ultimate };

struct BattleAttackImpact {
    BattleAttackType type = BattleAttackType::Punch;
    Vector3 previousPosition{};
    Vector3 position{};
};

struct BattleEnemy {
    std::string id;
    BattleEnemyType type = BattleEnemyType::MissileUnit;
    Vector3 position{};
    Vector3 previousPosition{};
    Vector3 velocity{};
    float attackTimer = 0.0f;
    float stateTimer = 0.0f;
    float visualAge = 0.0f;
    float previousVisualAge = 0.0f;
    float visualYaw = 0.0f;
    float previousVisualYaw = 0.0f;
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
    Vector3 defeatVelocity{};
    Vector3 defeatRotation{};
    Vector3 previousDefeatRotation{};
    enum class ChargeState {
        Approaching, Repositioning, ReacquiringTarget, Preparing, Charging,
        Impact, HitBraking, MissBraking
    } chargeState = ChargeState::Approaching;
    Vector3 chargeDirection{0.0f, 0.0f, -1.0f};
    Vector3 chargeTarget{};
    float chargeTimer = 0.0f;
    bool chargeDamageActiveThisStep = false;
    bool chargeReachedObstacle = false;
    bool chargePassedTarget = false;
    float chargeBrakingSpeed = 0.0f;
};

struct BattleProjectile {
    std::string id;
    BattleProjectileType type = BattleProjectileType::Missile;
    Vector3 position{};
    Vector3 previousPosition{};
    Vector3 forwardPosition{};
    float age = 0.0f;
    float previousAge = 0.0f;
    float phase = 0.0f;
    Vector3 direction{0.0f, 0.0f, -1.0f};
    Vector3 previousDirection{0.0f, 0.0f, -1.0f};
    Vector3 turnStartDirection{0.0f, 0.0f, -1.0f};
    enum class MissileFlightState { Outbound, Turning, BaseRun }
        missileFlightState = MissileFlightState::Outbound;
    float turnTimer = 0.0f;
};

enum class BattleVisualEffect { ShieldHit, Explosion };

constexpr float BattleVisualEffectDuration(BattleVisualEffect effect) {
    return effect == BattleVisualEffect::ShieldHit ? 0.46f : 0.97f;
}

struct BattleVisualEvent {
    BattleVisualEffect effect = BattleVisualEffect::Explosion;
    Vector3 position{};
    float scale = 1.0f;
    ShieldImpactState shieldImpact = ShieldImpactState::None;
};

inline std::optional<BattleVisualEvent> MakeShieldHitVisualEvent(
    const PlayerHitFeedback& hit, const Vector3& center) {
    if (!hit.result.wasApplied || hit.result.shieldLayersLost <= 0 ||
        hit.result.shieldImpact == ShieldImpactState::None)
        return std::nullopt;
    return BattleVisualEvent{BattleVisualEffect::ShieldHit, center,
        1.0f, hit.result.shieldImpact};
}

enum class BattleHitboxKind {
    Enemy, Projectile, BossCore, BossAttack, PlayerAttack
};
struct BattleHitbox {
    std::string id;
    BattleHitboxKind kind = BattleHitboxKind::Enemy;
    Vector3 previousCenter{};
    Vector3 center{};
    Vector3 facing{0.0f, 0.0f, -1.0f};
    float radius = 0.0f;
};

class CombatSession {
public:
    CombatSession(BattlePreviewKind kind, FirstPersonGame& player,
        EncounterConfig encounter = {}, BattleCombatConfig config = {});
    void Start(bool skipCountdown = false);
    void Reset();
    void Update(float seconds, const GameInput& input, float mouseX, float mouseY);
    bool ApplyTargetDamage(std::string_view id, int damage,
        bool emitUltimateImpact = false);
    bool ApplyTargetDamage(std::string_view id, int damage,
        const BattleAttackImpact& impact, bool emitUltimateImpact = false);

    BattlePhase GetPhase() const { return phase_; }
    float GetCountdownRemaining() const { return countdown_; }
    int GetBaseHitPoints() const { return baseHitPoints_; }
    int GetBaseMaximumHitPoints() const { return baseMaximumHitPoints_; }
    int GetWaveIndex() const { return director_.GetWaveIndex(); }
    int GetWaveCount() const { return director_.GetWaveCount(); }
    const std::vector<BattleEnemy>& GetEnemies() const { return enemies_; }
    const std::vector<BattleProjectile>& GetProjectiles() const { return projectiles_; }
    const EncounterDirector& GetDirector() const { return director_; }
    const BattleCombatConfig& GetConfig() const { return config_; }
    const BattleBoss* GetBoss() const { return boss_ ? &*boss_ : nullptr; }
    const FirstPersonGame& GetPlayerGame() const { return player_; }
    const std::vector<BattleFeedbackEvent>& GetFeedbackEvents() const {
        return feedbackEvents_;
    }
    const std::vector<BattleVisualEvent>& GetVisualEvents() const {
        return visualEvents_;
    }
    bool HasActiveExplosionEffects() const {
        return explosionRemaining_ > 0.0f;
    }
    std::vector<BattleHitbox> GetHitboxes() const;
    bool IsPractice() const { return kind_ == BattlePreviewKind::Practice; }
    const char* GetPracticePatternName() const;
    const char* GetEnemyPracticePatternName() const;
    const char* GetProjectilePracticePatternName() const;

private:
    bool ApplyTargetDamageImpl(std::string_view id, int damage,
        const BattleAttackImpact* impact, bool emitUltimateImpact);
    void BeginPlaying();
    bool SpawnEnemy(const EnemySpawnPlan& plan, float attackDelayOffset = 0.0f,
        std::optional<float> initialBomberRotation = std::nullopt);
    void SpawnProjectile(BattleProjectileType type, const Vector3& position,
        const Vector3& direction = {0.0f, 0.0f, -1.0f}, float phase = 0.0f,
        bool playLaunchSound = false);
    void UpdateEnemies(float seconds);
    void UpdateChargeEnemy(BattleEnemy& enemy, float seconds);
    void UpdateBoss(float seconds);
    void UpdateProjectiles(float seconds);
    void ResolveProjectileImpacts();
    void ResolveChargeImpacts();
    void ProcessPlayerHits();
    void ProcessUltimateHits();
    PlayerHitResult ReceivePlayerHit(PlayerHitType type,
        const Vector3& pushDirection = {0.0f, 0.0f, 1.0f},
        bool bossNeedleHit = false);
    void EmitExplosion(const Vector3& position, float scale = 1.0f);
    void ProcessPlayerHitVisuals();
    std::array<int, 3> CountActiveEnemies() const;
    std::string NextTargetId();
    void ApplyPracticeCommand(PracticeCommand command);
    void UpdatePracticePattern(float seconds);
    void AdvancePracticePattern(PracticeCommand pattern, float seconds,
        float& timer, int& index);

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
    int baseMaximumHitPoints_ = 10;
    std::unordered_set<std::string> ultimateHitIds_;
    std::unordered_set<std::uint64_t> bossAttackHits_;
    std::vector<BattleFeedbackEvent> feedbackEvents_;
    std::vector<BattleVisualEvent> visualEvents_;
    float explosionRemaining_ = 0.0f;
    std::vector<BossAttackEvent> activeBossAttacks_;
    PracticeCommand practicePattern_ = PracticeCommand::None;
    PracticeCommand enemyPattern_ = PracticeCommand::None;
    PracticeCommand projectilePattern_ = PracticeCommand::None;
    float enemyPatternTimer_ = 0.0f;
    float projectilePatternTimer_ = 0.0f;
    int enemyPatternIndex_ = 0;
    int projectilePatternIndex_ = 0;
    int practiceLaneCursor_ = 2;
};
