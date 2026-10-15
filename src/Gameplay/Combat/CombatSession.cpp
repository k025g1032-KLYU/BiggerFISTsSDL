#include "Gameplay/Combat/CombatSession.h"

#include "Collision/Collision.h"
#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>
#include <utility>

namespace {
constexpr float kBaseRadius = 7.0f;
constexpr Vector3 kBasePosition{0.0f, 0.0f, -16.0f};
constexpr std::string_view kBossTargetId = "target_boss_core";
constexpr float kEnemyPatternSpawnInterval = 0.35f;
constexpr float kProjectilePatternSpawnInterval = 0.45f;
constexpr float kEnemyAttackDelayOffset = 0.45f;

const char* PracticePatternName(PracticeCommand command) {
    switch (command) {
    case PracticeCommand::MissileUnit: return "SINGLE MISSILE UNIT";
    case PracticeCommand::Bomber: return "SINGLE BOMBER";
    case PracticeCommand::ChargeUnit: return "SINGLE CHARGE UNIT";
    case PracticeCommand::MissileUnits: return "MISSILE UNIT TEST";
    case PracticeCommand::Bombers: return "BOMBER TEST";
    case PracticeCommand::ChargeUnits: return "CHARGE UNIT TEST";
    case PracticeCommand::MixedEnemies: return "MIXED ENEMY TEST";
    case PracticeCommand::SingleMissile: return "SINGLE MISSILE";
    case PracticeCommand::MissileLine: return "MISSILE LINE";
    case PracticeCommand::FloatingBombs: return "FLOATING BOMBS";
    case PracticeCommand::MixedProjectiles: return "MIXED PROJECTILES";
    default: return "NONE";
    }
}

const char* EnemyPatternName(PracticeCommand command) {
    switch (command) {
    case PracticeCommand::MissileUnit: return "Single MissileUnit";
    case PracticeCommand::Bomber: return "Single Bomber";
    case PracticeCommand::ChargeUnit: return "Single ChargeUnit";
    case PracticeCommand::MissileUnits: return "MissileUnit Test";
    case PracticeCommand::Bombers: return "Bomber Test";
    case PracticeCommand::ChargeUnits: return "ChargeUnit Test";
    case PracticeCommand::MixedEnemies: return "Mixed Enemy Test";
    default: return "None";
    }
}

const char* ProjectilePatternName(PracticeCommand command) {
    switch (command) {
    case PracticeCommand::SingleMissile: return "Single Missile";
    case PracticeCommand::MissileLine: return "Missile Line";
    case PracticeCommand::FloatingBombs: return "Floating Bombs";
    case PracticeCommand::MixedProjectiles: return "Mixed";
    default: return "None";
    }
}

}

CombatSession::CombatSession(BattlePreviewKind kind, FirstPersonGame& player,
    EncounterConfig encounter, BattleCombatConfig config)
    : kind_(kind), player_(player), director_(std::move(encounter)),
      config_(config) {
    player_.EnableUltimate(kind_ != BattlePreviewKind::None);
    player_.EnableBaseCollision(kind_ != BattlePreviewKind::None);
    Reset();
}

void CombatSession::Reset() {
    if (boss_) {
        player_.RemoveTarget(kBossTargetId);
    }
    for (const BattleEnemy& enemy : enemies_) {
        player_.RemoveTarget(enemy.id);
    }
    for (const BattleProjectile& projectile : projectiles_) {
        player_.RemoveTarget(projectile.id);
    }
    enemies_.clear();
    projectiles_.clear();
    boss_.reset();
    ultimateHitIds_.clear();
    bossAttackHits_.clear();
    feedbackEvents_.clear();
    visualEvents_.clear();
    explosionRemaining_ = 0.0f;
    activeBossAttacks_.clear();
    director_.Reset();
    random_.seed(0xB16F157u);
    baseHitPoints_ = kind_ == BattlePreviewKind::Day2
        ? config_.enemy.day2BaseHitPoints : config_.enemy.baseHitPoints;
    baseMaximumHitPoints_ = baseHitPoints_;
    phase_ = BattlePhase::Ready;
    countdown_ = 0.0f;
    practicePattern_ = PracticeCommand::None;
    enemyPattern_ = projectilePattern_ = PracticeCommand::None;
    enemyPatternTimer_ = projectilePatternTimer_ = 0.0f;
    enemyPatternIndex_ = projectilePatternIndex_ = 0;
    practiceLaneCursor_ = 2;
    if (!IsPractice()) player_.ResetCombatState();
}

std::vector<BattleHitbox> CombatSession::GetHitboxes() const {
    std::vector<BattleHitbox> result;
    result.reserve(enemies_.size() + projectiles_.size() +
        activeBossAttacks_.size() + (boss_ ? 1u : 0u));
    for (const BattleEnemy& enemy : enemies_) {
        if (enemy.defeated) continue;
        const Vector3 facing = enemy.type == BattleEnemyType::ChargeUnit
            ? enemy.chargeDirection
            : enemy.type == BattleEnemyType::MissileUnit
            ? Vector3{-std::sin(enemy.visualYaw), 0.0f,
                -std::cos(enemy.visualYaw)}
            : Vector3{0.0f, 0.0f, -1.0f};
        result.push_back({enemy.id, BattleHitboxKind::Enemy,
            enemy.previousPosition, enemy.position, facing,
            config_.enemy.radius});
    }
    for (const BattleProjectile& projectile : projectiles_) {
        if (projectile.age < 0.0f) continue;
        const float radius = projectile.type == BattleProjectileType::Missile
            ? config_.projectile.missileRadius
            : projectile.type == BattleProjectileType::FloatingBomb
            ? config_.projectile.bombRadius
            : projectile.type == BattleProjectileType::CrystalSpike
            ? config_.projectile.crystalSpikeRadius
            : config_.projectile.crystalCubeRadius;
        result.push_back({projectile.id, BattleHitboxKind::Projectile,
            projectile.previousPosition, projectile.position,
            projectile.direction, radius});
    }
    if (boss_ && boss_->GetMode() != BossMode::Defeating &&
        boss_->GetMode() != BossMode::Defeated) {
        const float yaw = boss_->GetVisualPose().rootRotation.y;
        result.push_back({std::string(kBossTargetId),
            BattleHitboxKind::BossCore, boss_->GetPreviousPosition(),
            boss_->GetPosition(), {-std::sin(yaw), 0.0f,
                -std::cos(yaw)},
            config_.boss.coreRadius});
    }
    for (const BossAttackEvent& attack : activeBossAttacks_) {
        if (attack.type == BossAttackType::CrystalSpike ||
            attack.type == BossAttackType::CrystalCube ||
            attack.radius <= 0.0f) continue;
        Vector3 direction{attack.position.x - attack.previousPosition.x,
            0.0f, attack.position.z - attack.previousPosition.z};
        const float length = std::hypot(direction.x, direction.z);
        if (length > 0.00001f) {
            direction.x /= length;
            direction.z /= length;
        } else direction = {0.0f, 0.0f, -1.0f};
        result.push_back({"boss_attack_" + std::to_string(attack.attackId),
            BattleHitboxKind::BossAttack, attack.previousPosition,
            attack.position, direction, attack.radius});
    }
    const std::array<const PunchArm*, 2> arms{
        &player_.GetLeftArm(), &player_.GetRightArm()};
    for (std::size_t index = 0; index < arms.size(); ++index) {
        const PunchArm& arm = *arms[index];
        if (!arm.IsPunchDamageActiveThisFrame() &&
            !arm.IsRocketDamageActiveThisFrame()) continue;
        Vector3 direction{
            arm.GetFistPosition().x - arm.GetPreviousFistPosition().x,
            arm.GetFistPosition().y - arm.GetPreviousFistPosition().y,
            arm.GetFistPosition().z - arm.GetPreviousFistPosition().z};
        const float length = std::sqrt(direction.x * direction.x +
            direction.y * direction.y + direction.z * direction.z);
        if (length > 0.00001f) {
            direction = {direction.x / length,
                direction.y / length, direction.z / length};
        } else direction = {0.0f, 0.0f, -1.0f};
        result.push_back({index == 0 ? "left_fist" : "right_fist",
            BattleHitboxKind::PlayerAttack, arm.GetPreviousFistPosition(),
            arm.GetFistPosition(), direction, FirstPersonTuning::kFistRadius});
    }
    if (player_.IsUltimateDamageActiveThisFrame()) {
        result.push_back({"ultimate", BattleHitboxKind::PlayerAttack,
            player_.GetPreviousUltimatePosition(),
            player_.GetUltimatePosition(), player_.GetUltimateDirection(),
            player_.GetUltimateRadius()});
    }
    return result;
}

const char* CombatSession::GetPracticePatternName() const {
    return PracticePatternName(practicePattern_);
}

const char* CombatSession::GetEnemyPracticePatternName() const {
    return EnemyPatternName(enemyPattern_ == PracticeCommand::None
        ? practicePattern_ : enemyPattern_);
}

const char* CombatSession::GetProjectilePracticePatternName() const {
    return ProjectilePatternName(projectilePattern_);
}

void CombatSession::ApplyPracticeCommand(PracticeCommand command) {
    if (!IsPractice()) return;
    switch (command) {
    case PracticeCommand::MissileUnit:
    case PracticeCommand::Bomber:
    case PracticeCommand::ChargeUnit: {
        practicePattern_ = command;
        const BattleEnemyType type = command == PracticeCommand::MissileUnit
            ? BattleEnemyType::MissileUnit : command == PracticeCommand::Bomber
            ? BattleEnemyType::Bomber : BattleEnemyType::ChargeUnit;
        const int lane = practiceLaneCursor_++;
        SpawnEnemy({type, 1, 1, lane}, 0.0f,
            type == BattleEnemyType::Bomber
            ? std::optional<float>{lane * std::numbers::pi_v<float>}
            : std::nullopt);
        break;
    }
    case PracticeCommand::MissileUnits:
    case PracticeCommand::Bombers:
    case PracticeCommand::ChargeUnits:
    case PracticeCommand::MixedEnemies:
    case PracticeCommand::SingleMissile:
    case PracticeCommand::MissileLine:
    case PracticeCommand::FloatingBombs:
    case PracticeCommand::MixedProjectiles:
        if (command == PracticeCommand::MissileUnits ||
            command == PracticeCommand::Bombers ||
            command == PracticeCommand::ChargeUnits ||
            command == PracticeCommand::MixedEnemies) {
            for (const BattleEnemy& enemy : enemies_) player_.RemoveTarget(enemy.id);
            enemies_.clear();
            for (const BattleProjectile& projectile : projectiles_)
                player_.RemoveTarget(projectile.id);
            projectiles_.clear();
            projectilePattern_ = PracticeCommand::None;
            projectilePatternIndex_ = 0;
            projectilePatternTimer_ = 0.0f;
            enemyPattern_ = command;
            enemyPatternIndex_ = 0;
            enemyPatternTimer_ = 0.0f;
        } else {
            for (const BattleProjectile& projectile : projectiles_)
                player_.RemoveTarget(projectile.id);
            projectiles_.clear();
            projectilePattern_ = command;
            projectilePatternIndex_ = 0;
            projectilePatternTimer_ = 0.0f;
        }
        practicePattern_ = command;
        break;
    case PracticeCommand::BossSpawn:
        if (boss_) player_.RemoveTarget(kBossTargetId);
        boss_.emplace(config_.boss);
        player_.SpawnTarget({std::string(kBossTargetId), boss_->GetPosition(),
            1000000, {}, false, config_.boss.coreRadius, 1.0f,
            TargetDisplayKind::Boss});
        break;
    case PracticeCommand::BossConeCharge:
        if (boss_ && boss_->GetMode() != BossMode::Entrance)
            boss_->BeginBehavior(BossMode::Charge);
        break;
    case PracticeCommand::BossBoxer:
        if (boss_ && boss_->GetMode() != BossMode::Entrance)
            boss_->BeginBehavior(boss_->GetMode() == BossMode::Boxer
                ? BossMode::Idle : BossMode::Boxer);
        break;
    case PracticeCommand::BossNeedleCombo:
        if (boss_ && boss_->GetMode() != BossMode::Entrance)
            boss_->BeginBehavior(BossMode::NeedleCombo);
        break;
    case PracticeCommand::BossFullBarrage:
        if (boss_ && boss_->GetMode() != BossMode::Entrance)
            boss_->BeginBehavior(BossMode::FullBarrage);
        break;
    case PracticeCommand::BossSpreadBurst:
        if (boss_ && boss_->GetMode() != BossMode::Entrance)
            boss_->BeginBehavior(BossMode::SpreadCounter);
        break;
    case PracticeCommand::BossCycleNeedles:
        if (boss_) boss_->CyclePracticeNeedlePose();
        break;
    default: break;
    }
}

void CombatSession::UpdatePracticePattern(float seconds) {
    AdvancePracticePattern(enemyPattern_, seconds,
        enemyPatternTimer_, enemyPatternIndex_);
    AdvancePracticePattern(projectilePattern_, seconds,
        projectilePatternTimer_, projectilePatternIndex_);
}

void CombatSession::AdvancePracticePattern(PracticeCommand pattern, float seconds,
    float& timer, int& nextIndex) {
    if (pattern == PracticeCommand::None) return;
    const bool projectile = pattern == PracticeCommand::SingleMissile ||
        pattern == PracticeCommand::MissileLine ||
        pattern == PracticeCommand::FloatingBombs ||
        pattern == PracticeCommand::MixedProjectiles;
    const int count = pattern == PracticeCommand::MixedEnemies ? 4 :
        pattern == PracticeCommand::SingleMissile ? 1 :
        pattern == PracticeCommand::MissileLine ||
        pattern == PracticeCommand::FloatingBombs ? 5 :
        pattern == PracticeCommand::MixedProjectiles ? 8 : 2;
    if (nextIndex >= count) return;
    timer -= seconds;
    if (timer > 0.0f) return;
    const int index = nextIndex++;
    if (projectile) {
        const float lane = pattern == PracticeCommand::MissileLine
            ? 1.55f : index % 2 == 0 ? -1.55f : 1.55f;
        const float height = config_.enemy.combatHeightMinimum +
            (config_.enemy.combatHeightMaximum - config_.enemy.combatHeightMinimum) *
            (index % 3) * 0.5f;
        const bool bomb = pattern == PracticeCommand::FloatingBombs ||
            (pattern == PracticeCommand::MixedProjectiles && index % 2 != 0);
        const float projectileHeight = pattern == PracticeCommand::SingleMissile ||
            pattern == PracticeCommand::MissileLine
            ? (config_.enemy.combatHeightMinimum +
                config_.enemy.combatHeightMaximum) * 0.5f : height;
        SpawnProjectile(bomb ? BattleProjectileType::FloatingBomb :
            BattleProjectileType::Missile, {lane, projectileHeight, 19.0f});
    } else {
        BattleEnemyType type = BattleEnemyType::MissileUnit;
        if (pattern == PracticeCommand::Bombers ||
            (pattern == PracticeCommand::MixedEnemies && index == 2))
            type = BattleEnemyType::Bomber;
        if (pattern == PracticeCommand::ChargeUnits ||
            (pattern == PracticeCommand::MixedEnemies && index == 3))
            type = BattleEnemyType::ChargeUnit;
        const int lane = pattern == PracticeCommand::MixedEnemies
            ? (index == 2 ? 2 : index == 3 ? 4 : index == 0 ? 1 : 3)
            : index == 0 ? 1 : 3;
        const float attackDelay = type == BattleEnemyType::ChargeUnit
            ? 0.0f : index * kEnemyAttackDelayOffset;
        const std::optional<float> bomberRotation = type == BattleEnemyType::Bomber
            ? std::optional<float>{(pattern == PracticeCommand::MixedEnemies
                ? 1.0f : static_cast<float>(index)) * std::numbers::pi_v<float>}
            : std::nullopt;
        SpawnEnemy({type, 1, 1, lane}, attackDelay, bomberRotation);
    }
    timer = projectile ? kProjectilePatternSpawnInterval :
        kEnemyPatternSpawnInterval;
}

void CombatSession::Start(bool skipCountdown) {
    Reset();
    phase_ = skipCountdown ? BattlePhase::Playing : BattlePhase::Countdown;
    countdown_ = skipCountdown ? 0.0f : 3.0f;
    if (skipCountdown) {
        BeginPlaying();
    }
}

void CombatSession::BeginPlaying() {
    if (kind_ == BattlePreviewKind::Day1 || kind_ == BattlePreviewKind::Day2) {
        director_.Start();
    } else if (kind_ == BattlePreviewKind::Boss) {
        boss_.emplace(config_.boss);
        player_.SpawnTarget({std::string(kBossTargetId), boss_->GetPosition(),
            1000000, {}, false, config_.boss.coreRadius, 1.0f,
            TargetDisplayKind::Boss});
    } else {
        const BattleEnemyType type = kind_ == BattlePreviewKind::Bomber
            ? BattleEnemyType::Bomber : kind_ == BattlePreviewKind::ChargeUnit
            ? BattleEnemyType::ChargeUnit : BattleEnemyType::MissileUnit;
        SpawnEnemy({type, 1, 1, 2});
    }
}

std::string CombatSession::NextTargetId() {
    return "target_battle_" + std::to_string(nextId_++);
}

bool CombatSession::SpawnEnemy(const EnemySpawnPlan& plan,
    float attackDelayOffset, std::optional<float> initialBomberRotation) {
    BattleEnemy enemy;
    enemy.id = NextTargetId();
    enemy.type = plan.type;
    enemy.lane = std::clamp(plan.lane, 0, 4);
    const float laneWidth = (config_.enemy.deploymentMaximumX -
        config_.enemy.deploymentMinimumX) / 5.0f;
    const float laneStart = config_.enemy.deploymentMinimumX +
        static_cast<float>(enemy.lane) * laneWidth;
    std::uniform_real_distribution<float> xDistribution(laneStart,
        laneStart + laneWidth);
    std::uniform_real_distribution<float> zDistribution(
        config_.enemy.deploymentMinimumZ,
        config_.enemy.deploymentMaximumZ);
    std::uniform_real_distribution<float> heightDistribution(
        config_.enemy.combatHeightMinimum,
        config_.enemy.combatHeightMaximum);
    Vector3 candidate{};
    float bestSeparation = -1.0f;
    bool foundSeparated = false;
    for (int attempt = 0; attempt < 12; ++attempt) {
        candidate = {xDistribution(random_), config_.enemy.dropStartHeight,
            zDistribution(random_)};
        float nearest = 1000000.0f;
        for (const BattleEnemy& existing : enemies_) {
            if (!existing.defeated) {
                const float dx = candidate.x - existing.position.x;
                const float dz = candidate.z - existing.position.z;
                nearest = std::min(nearest, dx * dx + dz * dz);
            }
        }
        if (nearest >= 2.5f * 2.5f) {
            enemy.position = candidate;
            foundSeparated = true;
            break;
        }
        if (nearest > bestSeparation) {
            bestSeparation = nearest;
            enemy.position = candidate;
        }
    }
    if (!foundSeparated && bestSeparation < 0.0f) {
        enemy.position = candidate;
    }
    enemy.combatHeight = plan.type == BattleEnemyType::ChargeUnit
        ? config_.enemy.chargeCombatHeight : heightDistribution(random_);
    enemy.visualYaw = plan.type == BattleEnemyType::ChargeUnit
        ? 4.71238898f : 0.0f;
    enemy.previousVisualYaw = enemy.visualYaw;
    enemy.previousPosition = enemy.position;
    enemy.attackTimer = plan.type == BattleEnemyType::MissileUnit
        ? config_.enemy.missileInitialFireDelay
        : config_.enemy.bomberInitialFireDelay;
    if (plan.type != BattleEnemyType::ChargeUnit)
        enemy.attackTimer += attackDelayOffset;
    enemy.velocity.x = enemy.position.x <= 0.0f ? 1.0f : -1.0f;
    if (plan.type == BattleEnemyType::Bomber) {
        std::uniform_real_distribution<float> speedDistribution(
            config_.enemy.bomberMinimumMoveSpeed,
            config_.enemy.bomberMaximumMoveSpeed);
        std::uniform_int_distribution<int> directionDistribution(0, 1);
        enemy.velocity.x = speedDistribution(random_) *
            (directionDistribution(random_) == 0 ? -1.0f : 1.0f);
        std::uniform_real_distribution<float> phaseDistribution(0.0f, 6.2831853f);
        enemy.rotation = initialBomberRotation
            ? *initialBomberRotation : phaseDistribution(random_);
    }
    if (!player_.SpawnTarget({enemy.id, enemy.position, 1, {}, false,
            config_.enemy.radius, 1.35f, TargetDisplayKind::Enemy})) {
        return false;
    }
    enemies_.push_back(std::move(enemy));
    return true;
}

void CombatSession::SpawnProjectile(BattleProjectileType type,
    const Vector3& position, const Vector3& direction, float phase,
    bool playLaunchSound) {
    BattleProjectile projectile;
    projectile.id = NextTargetId();
    projectile.type = type;
    projectile.position = position;
    projectile.position.y = std::clamp(projectile.position.y,
        config_.enemy.combatHeightMinimum,
        config_.enemy.combatHeightMaximum);
    projectile.previousPosition = projectile.position;
    projectile.forwardPosition = projectile.position;
    projectile.phase = phase;
    const float length = std::hypot(direction.x, direction.z);
    projectile.direction = length > 0.00001f
        ? Vector3{direction.x / length, 0.0f, direction.z / length}
        : Vector3{0.0f, 0.0f, -1.0f};
    projectile.previousDirection = projectile.direction;
    projectile.turnStartDirection = projectile.direction;
    const float radius = type == BattleProjectileType::Missile
        ? config_.projectile.missileRadius : type == BattleProjectileType::FloatingBomb
        ? config_.projectile.bombRadius : type == BattleProjectileType::CrystalSpike
        ? config_.projectile.crystalSpikeRadius
        : config_.projectile.crystalCubeRadius;
    if (player_.SpawnTarget({projectile.id, projectile.position, 1, {}, false,
            radius, 1.0f, TargetDisplayKind::Projectile})) {
        projectiles_.push_back(std::move(projectile));
        if (playLaunchSound) {
            feedbackEvents_.push_back(BattleFeedbackEvent::ProjectileLaunched);
        }
    }
}

void CombatSession::UpdateBoss(float seconds) {
    if (!boss_ || boss_->IsDefeated()) {
        return;
    }
    const Vector3 playerPosition = player_.GetPlayer().GetPosition();
    BossPlayerThreat threat;
    const bool ultimateForming = player_.GetUltimateState() ==
            UltimateState::Forming ||
        player_.GetUltimateState() == UltimateState::Ready;
    const std::array<const PunchArm*, 2> arms{
        &player_.GetLeftArm(), &player_.GetRightArm()};
    for (std::size_t index = 0; index < arms.size(); ++index) {
        const PunchArm& arm = *arms[index];
        threat.isRocketCharged[index] = !ultimateForming &&
            arm.GetState() == PunchArmState::Charged &&
            arm.GetAttackType() == PunchArmAttackType::RocketPunch;
        if (threat.isRocketCharged[index]) {
            const RocketPreviewTrajectory trajectory =
                arm.GetRocketPreviewTrajectory(playerPosition,
                    player_.GetPlayer().GetBodyYaw());
            const Vector3 end{
                trajectory.start.x + trajectory.direction.x *
                    trajectory.flightDistance,
                trajectory.start.y + trajectory.direction.y *
                    trajectory.flightDistance,
                trajectory.start.z + trajectory.direction.z *
                    trajectory.flightDistance};
            threat.isChargedRocketAimedAtCore[index] =
                trajectory.flightDistance > 0.0f &&
                Collision::SweepSphereAgainstSphere(trajectory.start, end,
                    FirstPersonTuning::kFistRadius,
                    {boss_->GetPosition(), config_.boss.coreRadius}).didHit;
        }
        if (arm.GetState() != PunchArmState::RocketFlying) {
            continue;
        }
        const Vector3 offset{
            boss_->GetPosition().x - arm.GetFistPosition().x, 0.0f,
            boss_->GetPosition().z - arm.GetFistPosition().z};
        const float distance = std::hypot(offset.x, offset.z);
        const float yaw = arm.GetRocketPoseYaw();
        const float alignment = distance > 0.00001f
            ? (std::sin(yaw) * offset.x + std::cos(yaw) * offset.z) /
                distance : -1.0f;
        threat.hasIncomingRocketPunch |= distance <=
                config_.boss.rocketDodgeWarningDistance &&
            alignment >= config_.boss.rocketDodgeDirectionDotThreshold;
    }
    const BossMode previousMode = boss_->GetMode();
    const BossNeedlePose previousNeedlePose = boss_->GetNeedlePose();
    const std::vector<BossAttackEvent> attacks =
        boss_->Update(seconds, playerPosition, threat);
    activeBossAttacks_ = attacks;
    if (boss_->DidEmitDefeatSmallExplosionThisStep()) {
        EmitExplosion(boss_->GetDefeatSmallExplosionPosition(), 0.72f);
        feedbackEvents_.push_back(
            BattleFeedbackEvent::BossDefeatSmallExplosion);
    }
    if (previousMode == BossMode::Defeating) {
        if (boss_->GetMode() == BossMode::Defeated) {
            EmitExplosion(boss_->GetPosition(), 3.8f);
            feedbackEvents_.push_back(BattleFeedbackEvent::BossDefeated);
        }
    }
    if (previousNeedlePose != boss_->GetNeedlePose() &&
        boss_->GetMode() != BossMode::Defeating &&
        boss_->GetMode() != BossMode::Defeated) {
        feedbackEvents_.push_back(BattleFeedbackEvent::BossNeedlePoseChanged);
    }
    if (previousMode != boss_->GetMode() &&
        boss_->GetMode() != BossMode::Defeating &&
        boss_->GetMode() != BossMode::Defeated) {
        if (boss_->GetMode() == BossMode::NeedleCombo) {
            feedbackEvents_.push_back(BattleFeedbackEvent::BossNeedleStarted);
        } else if (boss_->GetMode() == BossMode::FullBarrage) {
            feedbackEvents_.push_back(BattleFeedbackEvent::BossBarrageStarted);
        }
    }
    if (boss_->DidBeginRocketDodgeThisStep()) {
        feedbackEvents_.push_back(BattleFeedbackEvent::BossDodged);
    }
    if (boss_->DidBeginCrystalLaunchThisStep()) {
        feedbackEvents_.push_back(BattleFeedbackEvent::BossCrystalLaunched);
    }
    if (boss_->DidBeginChargeThisStep()) {
        feedbackEvents_.push_back(BattleFeedbackEvent::BossChargeLaunched);
    }
    if (boss_->DidBeginStrikeThisStep()) {
        feedbackEvents_.push_back(BattleFeedbackEvent::BossStriking);
    }
    for (const BossAttackEvent& attack : attacks) {
        if (attack.type == BossAttackType::ChargeImpact ||
            attack.type == BossAttackType::ChargeBodyContact) {
            Vector3 playerCenter = playerPosition;
            playerCenter.y += FirstPersonTuning::kPlayerColliderCenterHeight;
            Vector3 previousPlayerCenter =
                player_.GetPlayer().GetPreviousPosition();
            previousPlayerCenter.y +=
                FirstPersonTuning::kPlayerColliderCenterHeight;
            const Collision::SweepHit hit =
                Collision::SweepSphereAgainstMovingSphere(
                    attack.previousPosition, attack.position, attack.radius,
                    previousPlayerCenter, playerCenter,
                    FirstPersonTuning::kPlayerColliderRadius);
            if (!hit.didHit) {
                continue;
            }
            if (attack.type == BossAttackType::ChargeImpact &&
                boss_->RegisterChargeTipHit(attack.attackId)) {
                ReceivePlayerHit(PlayerHitType::ChargeUnit,
                    boss_->GetChargeDirection());
            } else if (attack.type == BossAttackType::ChargeBodyContact &&
                boss_->RegisterChargeBodyContact(attack.attackId)) {
                const Vector3 bossAtContact{
                    attack.previousPosition.x +
                        (attack.position.x - attack.previousPosition.x) *
                            hit.time,
                    attack.previousPosition.y +
                        (attack.position.y - attack.previousPosition.y) *
                            hit.time,
                    attack.previousPosition.z +
                        (attack.position.z - attack.previousPosition.z) *
                            hit.time};
                const Vector3 playerAtContact{
                    previousPlayerCenter.x +
                        (playerCenter.x - previousPlayerCenter.x) * hit.time,
                    previousPlayerCenter.y +
                        (playerCenter.y - previousPlayerCenter.y) * hit.time,
                    previousPlayerCenter.z +
                        (playerCenter.z - previousPlayerCenter.z) * hit.time};
                player_.StartBossBodyPush({
                    playerAtContact.x - bossAtContact.x, 0.0f,
                    playerAtContact.z - bossAtContact.z});
            }
        } else if (attack.type == BossAttackType::CrystalSpike ||
            attack.type == BossAttackType::CrystalCube) {
            SpawnProjectile(attack.type == BossAttackType::CrystalSpike
                ? BattleProjectileType::CrystalSpike
                : BattleProjectileType::CrystalCube,
                attack.position, attack.direction, attack.oscillationPhase);
        } else if (attack.type == BossAttackType::BoxerStrike ||
            attack.type == BossAttackType::NeedleStrike ||
            attack.type == BossAttackType::NeedleImpact) {
            Vector3 playerCenter = playerPosition;
            playerCenter.y += FirstPersonTuning::kPlayerColliderCenterHeight;
            Vector3 previousPlayerCenter =
                player_.GetPlayer().GetPreviousPosition();
            previousPlayerCenter.y +=
                FirstPersonTuning::kPlayerColliderCenterHeight;
            if (!bossAttackHits_.contains(attack.attackId) &&
                Collision::SweepSphereAgainstMovingSphere(
                    attack.previousPosition, attack.position, attack.radius,
                    previousPlayerCenter, playerCenter,
                    FirstPersonTuning::kPlayerColliderRadius).didHit) {
                ReceivePlayerHit(
                    attack.type == BossAttackType::NeedleImpact
                        ? PlayerHitType::NeedleFinisher
                        : PlayerHitType::Projectile,
                    {playerPosition.x - attack.position.x, 0.0f,
                        playerPosition.z - attack.position.z},
                    attack.type != BossAttackType::NeedleImpact);
                if (attack.type == BossAttackType::NeedleStrike &&
                    boss_->RegisterNeedleStrikeHit(attack.attackId)) {
                    player_.StartNeedleStun();
                }
                bossAttackHits_.insert(attack.attackId);
            }
        } else if (attack.type == BossAttackType::SpreadBurst) {
            EmitExplosion(attack.position,
                (config_.boss.spreadBurstRadius +
                    FirstPersonTuning::kPlayerColliderRadius) /
                    (1.1f * 1.35f));
            feedbackEvents_.push_back(BattleFeedbackEvent::BossSpreadBurstExploded);
            if (std::hypot(playerPosition.x - attack.position.x,
                    playerPosition.z - attack.position.z) <=
                    config_.boss.spreadBurstRadius +
                    FirstPersonTuning::kPlayerColliderRadius) {
                ReceivePlayerHit(PlayerHitType::BossSpreadBurst,
                    {playerPosition.x - attack.position.x, 0.0f,
                        playerPosition.z - attack.position.z});
            }
        } else {
            ReceivePlayerHit(PlayerHitType::Projectile,
                {playerPosition.x - attack.position.x, 0.0f,
                    playerPosition.z - attack.position.z});
        }
    }
    if (boss_->GetMode() != BossMode::NeedleCombo) {
        player_.EndNeedleStun();
    }
    if (boss_->GetMode() != BossMode::Defeating) {
        player_.MoveTarget(kBossTargetId, boss_->GetPosition());
    }
}

std::array<int, 3> CombatSession::CountActiveEnemies() const {
    std::array<int, 3> counts{};
    for (const BattleEnemy& enemy : enemies_) {
        if (!enemy.defeated) {
            ++counts[BattleEnemyTypeIndex(enemy.type)];
        }
    }
    return counts;
}

void CombatSession::UpdateEnemies(float seconds) {
    for (BattleEnemy& enemy : enemies_) {
        enemy.previousPosition = enemy.position;
        enemy.previousVisualYaw = enemy.visualYaw;
        enemy.previousVisualAge = enemy.visualAge;
        enemy.previousDefeatRotation = enemy.defeatRotation;
        enemy.chargeDamageActiveThisStep = false;
        if (enemy.defeated) {
            enemy.stateTimer += seconds;
            enemy.defeatVelocity.y -= config_.enemy.defeatGravity * seconds;
            enemy.position.x += enemy.defeatVelocity.x * seconds;
            enemy.position.y += enemy.defeatVelocity.y * seconds;
            enemy.position.z += enemy.defeatVelocity.z * seconds;
            enemy.defeatRotation.x += config_.enemy.defeatRotationXSpeed * seconds;
            enemy.defeatRotation.y += config_.enemy.defeatRotationYSpeed * seconds;
            enemy.defeatRotation.z += config_.enemy.defeatRotationZSpeed * seconds;
            continue;
        }
        if (!enemy.deployed) {
            switch (enemy.deploymentState) {
            case BattleEnemy::DeploymentState::Dropping:
                enemy.travelSpeed = std::min(config_.enemy.dropMaximumSpeed,
                    std::max(config_.enemy.dropInitialSpeed,
                        enemy.travelSpeed + config_.enemy.dropAcceleration * seconds));
                enemy.position.y = std::max(config_.enemy.radius,
                    enemy.position.y - enemy.travelSpeed * seconds);
                if (enemy.position.y <= config_.enemy.radius) {
                    enemy.deploymentState = BattleEnemy::DeploymentState::Landed;
                    enemy.stateTimer = 0.0f;
                    feedbackEvents_.push_back(BattleFeedbackEvent::EnemyLanded);
                }
                break;
            case BattleEnemy::DeploymentState::Landed:
                enemy.stateTimer += seconds;
                if (enemy.stateTimer >= config_.enemy.landingPause) {
                    enemy.deploymentState = BattleEnemy::DeploymentState::TakingOff;
                }
                break;
            case BattleEnemy::DeploymentState::TakingOff:
                enemy.position.y = std::min(enemy.combatHeight,
                    enemy.position.y + config_.enemy.takeoffSpeed * seconds);
                if (enemy.position.y >= enemy.combatHeight) {
                    enemy.deploymentState = BattleEnemy::DeploymentState::Deployed;
                    enemy.deployed = true;
                }
                break;
            case BattleEnemy::DeploymentState::Deployed:
                enemy.deployed = true;
                break;
            }
        } else if (enemy.type == BattleEnemyType::ChargeUnit) {
            enemy.visualAge += seconds;
            const BattleEnemy::ChargeState previousState = enemy.chargeState;
            UpdateChargeEnemy(enemy, seconds);
            if (previousState != enemy.chargeState) {
                if (enemy.chargeState == BattleEnemy::ChargeState::Preparing) {
                    feedbackEvents_.push_back(BattleFeedbackEvent::ChargePreparing);
                } else if (enemy.chargeState == BattleEnemy::ChargeState::Charging) {
                    feedbackEvents_.push_back(BattleFeedbackEvent::ChargeDashing);
                }
            }
        } else {
            enemy.visualAge += seconds;
            const bool isMissile = enemy.type == BattleEnemyType::MissileUnit;
            if (!isMissile) {
                enemy.carriedBombScale = std::min(1.0f,
                    enemy.carriedBombScale + seconds /
                        std::max(0.001f, config_.enemy.bomberFireCooldown));
            }
            const bool patrol = !isMissile ||
                enemy.missileState == BattleEnemy::MissileState::Patrolling;
            if (patrol) {
                const float speed = isMissile
                    ? config_.enemy.missileMoveSpeed : 1.0f;
                enemy.position.x += enemy.velocity.x * speed * seconds;
                const float minimumX = config_.enemy.deploymentMinimumX +
                    config_.enemy.radius;
                const float maximumX = config_.enemy.deploymentMaximumX -
                    config_.enemy.radius;
                if (enemy.position.x <= minimumX ||
                    enemy.position.x >= maximumX) {
                    enemy.position.x = std::clamp(enemy.position.x,
                        minimumX, maximumX);
                    enemy.velocity.x = -enemy.velocity.x;
                }
                enemy.attackTimer = std::max(0.0f,
                    enemy.attackTimer - seconds);
            }
            if (isMissile) {
                const float patrolYaw = enemy.velocity.x > 0.0f
                    ? -1.570796327f : 1.570796327f;
                const float targetYaw = enemy.missileState ==
                    BattleEnemy::MissileState::FireWindup ? 0.0f : patrolYaw;
                const float turnSpeed = enemy.missileState ==
                    BattleEnemy::MissileState::Patrolling
                    ? config_.enemy.missilePatrolTurnSpeed
                    : config_.enemy.missileFireTurnSpeed;
                enemy.visualYaw += std::clamp(std::remainder(
                    targetYaw - enemy.visualYaw, 6.283185307f),
                    -turnSpeed * seconds, turnSpeed * seconds);
                switch (enemy.missileState) {
                case BattleEnemy::MissileState::Patrolling:
                    if (enemy.attackTimer <= 0.0f) {
                        enemy.missileState = BattleEnemy::MissileState::FireWindup;
                        enemy.stateTimer = 0.0f;
                    }
                    break;
                case BattleEnemy::MissileState::FireWindup:
                    enemy.stateTimer += seconds;
                    if (enemy.stateTimer >=
                        config_.enemy.missileFirePauseBeforeLaunch) {
                        SpawnProjectile(BattleProjectileType::Missile,
                            {enemy.position.x, enemy.position.y,
                                enemy.position.z - 1.1f},
                            {0.0f, 0.0f, -1.0f}, 0.0f, true);
                        enemy.missileState = BattleEnemy::MissileState::FireRecovery;
                        enemy.stateTimer = 0.0f;
                    }
                    break;
                case BattleEnemy::MissileState::FireRecovery:
                    enemy.stateTimer += seconds;
                    if (enemy.stateTimer >=
                        config_.enemy.missileFirePauseAfterLaunch) {
                        enemy.missileState = BattleEnemy::MissileState::Patrolling;
                        enemy.stateTimer = 0.0f;
                        enemy.attackTimer = config_.enemy.missileFireCooldown;
                    }
                    break;
                }
            } else if (enemy.attackTimer <= 0.0f) {
                SpawnProjectile(BattleProjectileType::FloatingBomb,
                    enemy.position, {0.0f, 0.0f, -1.0f},
                    enemy.rotation);
                enemy.attackTimer = config_.enemy.bomberFireCooldown;
                enemy.carriedBombScale = 0.0f;
                enemy.rotation += 3.14159265f;
                std::uniform_real_distribution<float> speedDistribution(
                    config_.enemy.bomberMinimumMoveSpeed,
                    config_.enemy.bomberMaximumMoveSpeed);
                enemy.velocity.x = std::copysign(speedDistribution(random_),
                    enemy.velocity.x);
            }
        }
        player_.MoveTarget(enemy.id, enemy.position);
    }
    std::erase_if(enemies_, [this](const BattleEnemy& enemy) {
        if (!enemy.defeated) return false;
        const float margin = config_.enemy.defeatBoundsMargin;
        const bool finished = enemy.stateTimer >= config_.enemy.defeatLaunchDuration ||
            enemy.position.x < -24.0f - margin ||
            enemy.position.x > 24.0f + margin ||
            enemy.position.z < -16.0f - margin ||
            enemy.position.z > 42.0f + margin ||
            enemy.position.y < -margin ||
            enemy.position.y > config_.enemy.dropStartHeight + margin;
        if (finished) {
            EmitExplosion(enemy.position);
            feedbackEvents_.push_back(BattleFeedbackEvent::EnemyExploded);
        }
        return finished;
    });
}

void CombatSession::UpdateChargeEnemy(BattleEnemy& enemy, float seconds) {
    const Vector3& playerPosition = player_.GetPlayer().GetPosition();
    const float dx = playerPosition.x - enemy.position.x;
    const float dz = playerPosition.z - enemy.position.z;
    const float distance = std::hypot(dx, dz);
    const Vector3 toPlayer = distance > 0.00001f
        ? Vector3{dx / distance, 0.0f, dz / distance}
        : Vector3{0.0f, 0.0f, -1.0f};
    const auto safeTravelDistance = [&](const Vector3& direction) {
        float safe = std::numeric_limits<float>::infinity();
        const float minimumX = -24.0f + config_.enemy.radius;
        const float maximumX = 24.0f - config_.enemy.radius;
        const float minimumZ = -16.0f + config_.enemy.radius;
        const float maximumZ = 42.0f - config_.enemy.radius;
        if (direction.x > 0.00001f) {
            safe = std::min(safe, (maximumX - enemy.position.x) / direction.x);
        } else if (direction.x < -0.00001f) {
            safe = std::min(safe, (minimumX - enemy.position.x) / direction.x);
        }
        if (direction.z > 0.00001f) {
            safe = std::min(safe, (maximumZ - enemy.position.z) / direction.z);
        } else if (direction.z < -0.00001f) {
            safe = std::min(safe, (minimumZ - enemy.position.z) / direction.z);
        }
        const float exclusion = kBaseRadius + config_.enemy.radius +
            config_.enemy.chargeBaseSafetyMargin;
        const float fromBaseX = enemy.position.x - kBasePosition.x;
        const float fromBaseZ = enemy.position.z - kBasePosition.z;
        const float projection = fromBaseX * direction.x +
            fromBaseZ * direction.z;
        const float discriminant = projection * projection -
            (fromBaseX * fromBaseX + fromBaseZ * fromBaseZ -
                exclusion * exclusion);
        if (discriminant >= 0.0f) {
            const float entry = -projection - std::sqrt(discriminant);
            if (entry >= 0.0f) {
                safe = std::min(safe, entry);
            }
        }
        return std::max(0.0f, safe);
    };
    const auto moveSafely = [&](const Vector3& direction, float requested) {
        const float safe = safeTravelDistance(direction);
        const float travel = std::min(std::max(0.0f, requested), safe);
        enemy.position.x = std::clamp(enemy.position.x + direction.x * travel,
            -24.0f + config_.enemy.radius,
            24.0f - config_.enemy.radius);
        enemy.position.z = std::clamp(enemy.position.z + direction.z * travel,
            -16.0f + config_.enemy.radius,
            42.0f - config_.enemy.radius);
        return safe <= requested + 0.00001f;
    };
    const bool safeToCharge = safeTravelDistance(toPlayer) + 0.00001f >=
        std::max(0.0f, distance - config_.enemy.radius -
            FirstPersonTuning::kPlayerColliderRadius);
    const auto prepare = [&] {
        enemy.chargeState = BattleEnemy::ChargeState::Preparing;
        enemy.chargeTimer = 0.0f;
        enemy.chargeReachedObstacle = false;
        enemy.chargePassedTarget = false;
        enemy.chargeDirection = toPlayer;
        enemy.visualYaw = std::atan2(toPlayer.x, toPlayer.z) +
            1.570796327f;
        enemy.previousVisualYaw = enemy.visualYaw;
        enemy.chargeTarget = playerPosition;
    };
    switch (enemy.chargeState) {
    case BattleEnemy::ChargeState::Approaching:
        if (distance < config_.enemy.chargeMinimumDistance) {
            enemy.chargeState = BattleEnemy::ChargeState::Repositioning;
        } else if (distance <= config_.enemy.chargeDetectionRange) {
            if (safeToCharge) {
                prepare();
            } else {
                enemy.chargeState = BattleEnemy::ChargeState::Repositioning;
            }
        } else {
            moveSafely({0.0f, 0.0f, -1.0f},
                config_.enemy.chargeApproachSpeed * seconds);
            enemy.visualYaw = 4.71238898f;
            enemy.previousVisualYaw = enemy.visualYaw;
        }
        break;
    case BattleEnemy::ChargeState::Repositioning:
        if (distance >= config_.enemy.chargeMinimumDistance &&
            distance <= config_.enemy.chargeDetectionRange && safeToCharge) {
            prepare();
        } else if (distance > config_.enemy.chargeDetectionRange) {
            enemy.chargeState = BattleEnemy::ChargeState::Approaching;
        } else {
            moveSafely({-toPlayer.x, 0.0f, -toPlayer.z},
                config_.enemy.chargeRepositionSpeed * seconds);
            enemy.visualYaw = std::atan2(toPlayer.x, toPlayer.z) +
                1.570796327f;
            enemy.previousVisualYaw = enemy.visualYaw;
        }
        break;
    case BattleEnemy::ChargeState::ReacquiringTarget:
        enemy.visualYaw += std::clamp(std::remainder(
            std::atan2(toPlayer.x, toPlayer.z) + 1.570796327f -
                enemy.visualYaw, 6.283185307f),
            -4.5f * seconds, 4.5f * seconds);
        enemy.chargeTimer += seconds;
        if (enemy.chargeTimer >= 0.5f) {
            enemy.chargeTimer = 0.0f;
            enemy.chargeState = BattleEnemy::ChargeState::Approaching;
        }
        break;
    case BattleEnemy::ChargeState::Preparing:
        enemy.chargeTimer += seconds;
        if (enemy.chargeTimer >= config_.enemy.chargePrepareDuration) {
            enemy.chargeState = BattleEnemy::ChargeState::Charging;
            enemy.chargeTimer = 0.0f;
        }
        break;
    case BattleEnemy::ChargeState::Charging: {
        if (enemy.chargeReachedObstacle) {
            enemy.chargeState = BattleEnemy::ChargeState::ReacquiringTarget;
            enemy.chargeTimer = 0.0f;
            break;
        }
        if (enemy.chargePassedTarget) {
            enemy.chargeState = BattleEnemy::ChargeState::MissBraking;
            enemy.chargeBrakingSpeed = config_.enemy.chargeDashSpeed;
            enemy.chargeTimer = 0.0f;
            break;
        }
        const bool reachedObstacle = moveSafely(enemy.chargeDirection,
            config_.enemy.chargeDashSpeed * seconds);
        enemy.chargeDamageActiveThisStep = true;
        const float passed = (enemy.position.x - enemy.chargeTarget.x) *
            enemy.chargeDirection.x +
            (enemy.position.z - enemy.chargeTarget.z) * enemy.chargeDirection.z;
        if (reachedObstacle) {
            constexpr float kWallEpsilon = 0.00001f;
            const float minimumX = -24.0f + config_.enemy.radius;
            const float maximumX = 24.0f - config_.enemy.radius;
            const float minimumZ = -16.0f + config_.enemy.radius;
            const float maximumZ = 42.0f - config_.enemy.radius;
            const bool hitXWall = (enemy.chargeDirection.x > kWallEpsilon &&
                    enemy.position.x >= maximumX - kWallEpsilon) ||
                (enemy.chargeDirection.x < -kWallEpsilon &&
                    enemy.position.x <= minimumX + kWallEpsilon);
            const bool hitZWall = (enemy.chargeDirection.z > kWallEpsilon &&
                    enemy.position.z >= maximumZ - kWallEpsilon) ||
                (enemy.chargeDirection.z < -kWallEpsilon &&
                    enemy.position.z <= minimumZ + kWallEpsilon);
            if (hitXWall || hitZWall) {
                feedbackEvents_.push_back(BattleFeedbackEvent::ChargeWallImpact);
            }
        }
        enemy.chargeReachedObstacle = reachedObstacle;
        enemy.chargePassedTarget = passed >=
            config_.enemy.chargeTargetPassDistance;
        break;
    }
    case BattleEnemy::ChargeState::Impact:
        enemy.chargeTimer += seconds;
        if (enemy.chargeTimer >= config_.enemy.chargeImpactDuration) {
            enemy.chargeState = BattleEnemy::ChargeState::HitBraking;
            enemy.chargeBrakingSpeed =
                config_.enemy.chargeHitBrakingInitialSpeed;
            enemy.chargeTimer = 0.0f;
        }
        break;
    case BattleEnemy::ChargeState::HitBraking:
    case BattleEnemy::ChargeState::MissBraking: {
        const float duration = enemy.chargeState ==
            BattleEnemy::ChargeState::HitBraking
            ? config_.enemy.chargeHitBrakingDuration
            : config_.enemy.chargeMissBrakingDuration;
        const float initialSpeed = enemy.chargeState ==
            BattleEnemy::ChargeState::HitBraking
            ? config_.enemy.chargeHitBrakingInitialSpeed
            : config_.enemy.chargeDashSpeed;
        enemy.chargeTimer += seconds;
        enemy.chargeBrakingSpeed = std::max(0.0f,
            enemy.chargeBrakingSpeed - initialSpeed / duration * seconds);
        moveSafely(enemy.chargeDirection,
            enemy.chargeBrakingSpeed * seconds);
        if (enemy.chargeTimer >= duration) {
            enemy.chargeState = BattleEnemy::ChargeState::ReacquiringTarget;
            enemy.chargeTimer = 0.0f;
        }
        break;
    }
    }
}

void CombatSession::UpdateProjectiles(float seconds) {
    for (BattleProjectile& projectile : projectiles_) {
        projectile.previousPosition = projectile.position;
        projectile.previousDirection = projectile.direction;
        projectile.previousAge = projectile.age;
        projectile.age += seconds;
        const float speed = projectile.type == BattleProjectileType::Missile
            ? config_.projectile.missileSpeed
            : projectile.type == BattleProjectileType::FloatingBomb
            ? config_.projectile.bombSpeed
            : projectile.type == BattleProjectileType::CrystalSpike
            ? config_.projectile.crystalSpikeSpeed
            : config_.projectile.crystalCubeSpeed;
        if (projectile.type == BattleProjectileType::Missile &&
            projectile.missileFlightState ==
                BattleProjectile::MissileFlightState::Outbound &&
            projectile.forwardPosition.z <=
                config_.projectile.missileTurnBoundaryZ) {
            projectile.missileFlightState =
                BattleProjectile::MissileFlightState::Turning;
            projectile.turnStartDirection = projectile.direction;
            projectile.turnTimer = 0.0f;
        }
        if (projectile.missileFlightState ==
                BattleProjectile::MissileFlightState::Turning &&
            projectile.type == BattleProjectileType::Missile) {
            projectile.turnTimer += seconds;
            const float ratio = std::clamp(projectile.turnTimer /
                config_.projectile.missileTurnDuration, 0.0f, 1.0f);
            const float targetX = kBasePosition.x - projectile.forwardPosition.x;
            const float targetZ = kBasePosition.z - projectile.forwardPosition.z;
            const float targetLength = std::hypot(targetX, targetZ);
            const Vector3 targetDirection = targetLength > 0.00001f
                ? Vector3{targetX / targetLength, 0.0f,
                    targetZ / targetLength}
                : projectile.turnStartDirection;
            const float blendX = projectile.turnStartDirection.x *
                (1.0f - ratio) + targetDirection.x * ratio;
            const float blendZ = projectile.turnStartDirection.z *
                (1.0f - ratio) + targetDirection.z * ratio;
            const float blendLength = std::hypot(blendX, blendZ);
            if (blendLength > 0.00001f) {
                projectile.direction = {blendX / blendLength, 0.0f,
                    blendZ / blendLength};
            }
            if (ratio >= 1.0f) {
                projectile.missileFlightState =
                    BattleProjectile::MissileFlightState::BaseRun;
            }
        }
        projectile.forwardPosition.x += projectile.direction.x * speed * seconds;
        projectile.forwardPosition.y += projectile.direction.y * speed * seconds;
        projectile.forwardPosition.z += projectile.direction.z * speed * seconds;
        projectile.position = projectile.forwardPosition;
        if (projectile.type == BattleProjectileType::FloatingBomb ||
            projectile.type == BattleProjectileType::CrystalCube) {
            const float frequency = projectile.type ==
                BattleProjectileType::FloatingBomb
                ? config_.projectile.bombOscillationFrequency
                : config_.projectile.crystalCubeOscillationFrequency;
            const float amplitude = projectile.type ==
                BattleProjectileType::FloatingBomb
                ? config_.projectile.bombOscillationAmplitude
                : config_.projectile.crystalCubeOscillationAmplitude;
            const float offset = std::sin(projectile.age * frequency +
                projectile.phase) * amplitude;
            projectile.position.x += -projectile.direction.z * offset;
            projectile.position.z += projectile.direction.x * offset;
        }
        projectile.position.y = std::clamp(projectile.position.y,
            config_.enemy.combatHeightMinimum,
            config_.enemy.combatHeightMaximum);
        player_.MoveTarget(projectile.id, projectile.position);
        if (projectile.position.z < -18.0f ||
            projectile.position.z > 44.0f ||
            projectile.position.x < -26.0f ||
            projectile.position.x > 26.0f) {
            player_.RemoveTarget(projectile.id);
            projectile.age = -1.0f;
        }
    }
    std::erase_if(projectiles_, [](const BattleProjectile& projectile) {
        return projectile.age < 0.0f;
    });
}

void CombatSession::ResolveProjectileImpacts() {
    Vector3 playerPosition = player_.GetPlayer().GetPosition();
    playerPosition.y += FirstPersonTuning::kPlayerColliderCenterHeight;
    Vector3 previousPlayerPosition = player_.GetPlayer().GetPreviousPosition();
    previousPlayerPosition.y += FirstPersonTuning::kPlayerColliderCenterHeight;
    for (BattleProjectile& projectile : projectiles_) {
        const float radius = projectile.type == BattleProjectileType::Missile
            ? config_.projectile.missileRadius
            : projectile.type == BattleProjectileType::FloatingBomb
            ? config_.projectile.bombRadius
            : projectile.type == BattleProjectileType::CrystalSpike
            ? config_.projectile.crystalSpikeRadius
            : config_.projectile.crystalCubeRadius;
        if (Collision::SweepSphereAgainstMovingSphere(projectile.previousPosition,
                projectile.position, radius, previousPlayerPosition,
                playerPosition, FirstPersonTuning::kPlayerColliderRadius).didHit) {
            const PlayerHitResult hit = ReceivePlayerHit(PlayerHitType::Projectile);
            EmitExplosion(projectile.position,
                hit.shieldLayersLost > 0 ? 0.55f : 1.0f);
            feedbackEvents_.push_back(hit.shieldLayersLost > 0
                ? BattleFeedbackEvent::ShieldedProjectileExploded
                : BattleFeedbackEvent::ProjectileExploded);
            player_.RemoveTarget(projectile.id);
            projectile.age = -1.0f;
        } else if (Collision::SweepSphereAgainstSphere(projectile.previousPosition,
                projectile.position, radius,
                {kBasePosition, kBaseRadius}).didHit) {
            baseHitPoints_ = std::max(0, baseHitPoints_ - 1);
            feedbackEvents_.push_back(BattleFeedbackEvent::BaseDamaged);
            EmitExplosion(projectile.position);
            player_.RemoveTarget(projectile.id);
            projectile.age = -1.0f;
        }
    }
    std::erase_if(projectiles_, [](const BattleProjectile& projectile) {
        return projectile.age < 0.0f;
    });
}

void CombatSession::ResolveChargeImpacts() {
    Vector3 playerPosition = player_.GetPlayer().GetPosition();
    playerPosition.y += FirstPersonTuning::kPlayerColliderCenterHeight;
    Vector3 previousPlayerPosition = player_.GetPlayer().GetPreviousPosition();
    previousPlayerPosition.y += FirstPersonTuning::kPlayerColliderCenterHeight;
    for (BattleEnemy& enemy : enemies_) {
        if (enemy.defeated || enemy.type != BattleEnemyType::ChargeUnit ||
            !enemy.chargeDamageActiveThisStep) {
            continue;
        }
        if (Collision::SweepSphereAgainstMovingSphere(enemy.previousPosition,
                enemy.position, config_.enemy.radius,
                previousPlayerPosition, playerPosition,
                FirstPersonTuning::kPlayerColliderRadius).didHit) {
            ReceivePlayerHit(PlayerHitType::ChargeUnit,
                {playerPosition.x - enemy.position.x, 0.0f,
                    playerPosition.z - enemy.position.z});
            enemy.chargeState = BattleEnemy::ChargeState::Impact;
            enemy.chargeTimer = 0.0f;
            enemy.chargeDamageActiveThisStep = false;
        }
    }
}

void CombatSession::ProcessPlayerHits() {
    for (const CombatHitEvent& hit : player_.GetEvents().targetHits) {
        const PunchArm& arm = hit.armSide == PunchArmSide::Left
            ? player_.GetLeftArm() : player_.GetRightArm();
        const BattleAttackImpact impact{
            hit.attackType == PunchArmAttackType::Punch
                ? BattleAttackType::Punch : BattleAttackType::RocketPunch,
            arm.GetPreviousFistPosition(), arm.GetFistPosition()};
        ApplyTargetDamage(hit.targetId, 1, impact);
    }
    std::erase_if(projectiles_, [](const BattleProjectile& projectile) {
        return projectile.age < 0.0f;
    });
}

PlayerHitResult CombatSession::ReceivePlayerHit(PlayerHitType type,
    const Vector3& pushDirection, bool bossNeedleHit) {
    return player_.ReceivePlayerHit(type, pushDirection, bossNeedleHit);
}

void CombatSession::EmitExplosion(const Vector3& position, float scale) {
    visualEvents_.push_back({BattleVisualEffect::Explosion, position, scale});
    explosionRemaining_ = BattleVisualEffectDuration(
        BattleVisualEffect::Explosion);
}

void CombatSession::ProcessPlayerHitVisuals() {
    for (const PlayerHitFeedback& hit : player_.GetEvents().playerHits) {
        Vector3 center = player_.GetPlayer().GetPosition();
        center.y += FirstPersonTuning::kPlayerColliderCenterHeight;
        if (const auto event = MakeShieldHitVisualEvent(hit, center))
            visualEvents_.push_back(*event);
    }
}

bool CombatSession::ApplyTargetDamage(std::string_view id, int damage,
    bool emitUltimateImpact) {
    return ApplyTargetDamageImpl(id, damage, nullptr, emitUltimateImpact);
}

bool CombatSession::ApplyTargetDamage(std::string_view id, int damage,
    const BattleAttackImpact& impact, bool emitUltimateImpact) {
    return ApplyTargetDamageImpl(id, damage, &impact, emitUltimateImpact);
}

bool CombatSession::ApplyTargetDamageImpl(std::string_view id, int damage,
    const BattleAttackImpact* impact, bool emitUltimateImpact) {
    if ((phase_ != BattlePhase::Playing && !IsPractice()) || damage <= 0) {
        return false;
    }
    if (id == kBossTargetId && boss_) {
        const bool applied = boss_->ReceiveHit(damage);
        if (applied) {
            if (emitUltimateImpact)
                feedbackEvents_.push_back(BattleFeedbackEvent::BossDamaged);
            else if (impact && impact->type != BattleAttackType::Ultimate)
                feedbackEvents_.push_back(impact->type == BattleAttackType::Punch
                    ? BattleFeedbackEvent::PunchImpact
                    : BattleFeedbackEvent::RocketImpact);
            if (impact) {
                const float attackRadius = impact->type == BattleAttackType::Ultimate
                    ? player_.GetUltimateRadius()
                    : FirstPersonTuning::kFistRadius;
                const Collision::SweepHit sweep =
                    Collision::SweepSphereAgainstMovingSphere(
                        impact->previousPosition, impact->position,
                        attackRadius, boss_->GetPreviousPosition(),
                        boss_->GetPosition(), config_.boss.coreRadius);
                const float time = sweep.didHit ? sweep.time : 1.0f;
                const Vector3 center{
                    impact->previousPosition.x +
                        (impact->position.x - impact->previousPosition.x) * time,
                    impact->previousPosition.y +
                        (impact->position.y - impact->previousPosition.y) * time,
                    impact->previousPosition.z +
                        (impact->position.z - impact->previousPosition.z) * time};
                Vector3 normal{center.x - boss_->GetPosition().x,
                    center.y - boss_->GetPosition().y,
                    center.z - boss_->GetPosition().z};
                const float length = std::sqrt(normal.x * normal.x +
                    normal.y * normal.y + normal.z * normal.z);
                if (length > 0.00001f) {
                    normal = {normal.x / length, normal.y / length,
                        normal.z / length};
                } else {
                    normal = {0.0f, 0.0f, 1.0f};
                }
                EmitExplosion({
                    boss_->GetPosition().x + normal.x *
                        (config_.boss.coreRadius + 0.08f),
                    boss_->GetPosition().y + normal.y *
                        (config_.boss.coreRadius + 0.08f),
                    boss_->GetPosition().z + normal.z *
                        (config_.boss.coreRadius + 0.08f)}, 0.45f);
                if (impact->type == BattleAttackType::Ultimate) {
                    boss_->BeginUltimateWallCarry({
                        impact->position.x - impact->previousPosition.x,
                        0.0f,
                        impact->position.z - impact->previousPosition.z},
                        player_.GetUltimateSpeed());
                } else {
                    const float knockbackDistance =
                        impact->type == BattleAttackType::Punch
                        ? config_.boss.punchKnockbackDistance
                        : config_.boss.rocketKnockbackDistance;
                    boss_->ApplyKnockback(impact->position,
                        knockbackDistance);
                }
            }
        }
        if (applied && boss_->GetMode() == BossMode::Defeating) {
            activeBossAttacks_.clear();
            if (impact) {
                boss_->SetDefeatLaunchDirection({
                    impact->position.x - impact->previousPosition.x,
                    impact->position.y - impact->previousPosition.y,
                    impact->position.z - impact->previousPosition.z});
            }
            player_.RemoveTarget(kBossTargetId);
        }
        return applied;
    }
    for (BattleEnemy& enemy : enemies_) {
        if (enemy.id == id && !enemy.defeated) {
            enemy.defeated = true;
            Vector3 direction{0.0f, 0.0f, -1.0f};
            float speed = config_.enemy.defeatPunchSpeed;
            if (impact) {
                direction = {impact->position.x - impact->previousPosition.x,
                    0.0f, impact->position.z - impact->previousPosition.z};
                if (impact->type == BattleAttackType::RocketPunch)
                    speed = config_.enemy.defeatRocketSpeed;
                else if (impact->type == BattleAttackType::Ultimate)
                    speed = config_.enemy.defeatUltimateSpeed;
            }
            const float length = std::hypot(direction.x, direction.z);
            if (length <= 0.00001f) direction = {0.0f, 0.0f, -1.0f};
            else direction = {direction.x / length, 0.0f, direction.z / length};
            enemy.defeatVelocity = {direction.x * speed,
                config_.enemy.defeatUpwardSpeed, direction.z * speed};
            enemy.defeatRotation = {0.0f, enemy.visualYaw, 0.0f};
            enemy.previousDefeatRotation = enemy.defeatRotation;
            if (!impact || impact->type != BattleAttackType::Ultimate)
                feedbackEvents_.push_back(BattleFeedbackEvent::EnemyDefeated);
            enemy.stateTimer = 0.0f;
            player_.RemoveTarget(id);
            return true;
        }
    }
    for (BattleProjectile& projectile : projectiles_) {
        if (projectile.id == id && projectile.age >= 0.0f) {
            projectile.age = -1.0f;
            feedbackEvents_.push_back(BattleFeedbackEvent::ProjectileExploded);
            if (impact && impact->type != BattleAttackType::Ultimate)
                feedbackEvents_.push_back(impact->type == BattleAttackType::Punch
                    ? BattleFeedbackEvent::PunchImpact
                    : BattleFeedbackEvent::RocketImpact);
            EmitExplosion(projectile.position);
            player_.RemoveTarget(id);
            return true;
        }
    }
    return false;
}

void CombatSession::ProcessUltimateHits() {
    if (player_.GetEvents().ultimateLaunched) {
        ultimateHitIds_.clear();
    }
    if (!player_.IsUltimateDamageActiveThisFrame()) {
        return;
    }
    const Vector3& previous = player_.GetPreviousUltimatePosition();
    const Vector3& current = player_.GetUltimatePosition();
    const float radius = player_.GetUltimateRadius();
    bool defeatedEnemy = false;
    for (BattleEnemy& enemy : enemies_) {
        if (enemy.defeated || ultimateHitIds_.contains(enemy.id) ||
            !Collision::SweepSphereAgainstMovingSphere(previous, current, radius,
                enemy.previousPosition, enemy.position,
                config_.enemy.radius).didHit) {
            continue;
        }
        if (ApplyTargetDamage(enemy.id, 100,
                {BattleAttackType::Ultimate, previous, current})) {
            ultimateHitIds_.insert(enemy.id);
            defeatedEnemy = true;
        }
    }
    for (BattleProjectile& projectile : projectiles_) {
        const float projectileRadius = projectile.type == BattleProjectileType::Missile
            ? config_.projectile.missileRadius
            : projectile.type == BattleProjectileType::FloatingBomb
            ? config_.projectile.bombRadius
            : projectile.type == BattleProjectileType::CrystalSpike
            ? config_.projectile.crystalSpikeRadius
            : config_.projectile.crystalCubeRadius;
        if (ultimateHitIds_.contains(projectile.id) ||
            !Collision::SweepSphereAgainstMovingSphere(previous, current, radius,
                projectile.previousPosition, projectile.position,
                projectileRadius).didHit) {
            continue;
        }
        if (ApplyTargetDamage(projectile.id, 100)) {
            ultimateHitIds_.insert(projectile.id);
        }
    }
    if (boss_ && boss_->GetMode() != BossMode::Defeating &&
        !ultimateHitIds_.contains(std::string(kBossTargetId)) &&
        Collision::SweepSphereAgainstMovingSphere(previous, current, radius,
            boss_->GetPreviousPosition(), boss_->GetPosition(),
            config_.boss.coreRadius).didHit) {
        if (ApplyTargetDamage(kBossTargetId, config_.boss.ultimateDamage,
                {BattleAttackType::Ultimate, previous, current}, true)) {
            ultimateHitIds_.insert(std::string(kBossTargetId));
        }
    }
    if (defeatedEnemy)
        feedbackEvents_.push_back(BattleFeedbackEvent::EnemyDefeated);
    std::erase_if(projectiles_, [](const BattleProjectile& projectile) {
        return projectile.age < 0.0f;
    });
}

void CombatSession::Update(float seconds, const GameInput& input,
    float mouseX, float mouseY) {
    feedbackEvents_.clear();
    visualEvents_.clear();
    activeBossAttacks_.clear();
    explosionRemaining_ = std::max(0.0f, explosionRemaining_ - seconds);
    if (input.resetTarget.pressed) {
        Reset();
        return;
    }
    if (IsPractice()) {
        const std::optional<BossNeedlePose> previousNeedlePose = boss_
            ? std::optional{boss_->GetNeedlePose()} : std::nullopt;
        ApplyPracticeCommand(input.practiceCommand);
        if (boss_ && previousNeedlePose &&
            boss_->GetNeedlePose() != *previousNeedlePose) {
            feedbackEvents_.push_back(
                BattleFeedbackEvent::BossNeedlePoseChanged);
        }
        player_.Update(seconds, input, mouseX, mouseY, true);
        UpdatePracticePattern(seconds);
        UpdateEnemies(seconds);
        UpdateBoss(seconds);
        UpdateProjectiles(seconds);
        ProcessUltimateHits();
        player_.ResolveTargetHits();
        ProcessPlayerHits();
        ResolveProjectileImpacts();
        ResolveChargeImpacts();
        ProcessPlayerHitVisuals();
        return;
    }
    if (phase_ == BattlePhase::GameOver && input.battleStart.pressed) {
        Start();
        return;
    }
    if (phase_ == BattlePhase::Ready && input.battleStart.pressed) {
        Start();
    }
    if (phase_ == BattlePhase::Countdown) {
        countdown_ = std::max(0.0f, countdown_ - seconds);
        if (countdown_ <= 0.0f) {
            phase_ = BattlePhase::Playing;
            BeginPlaying();
        }
    }
    if (phase_ == BattlePhase::Countdown || phase_ == BattlePhase::Clear ||
        phase_ == BattlePhase::GameOver) {
        player_.UpdateLookOnly(mouseX, mouseY);
        return;
    }
    player_.Update(seconds, input, mouseX, mouseY, true);
    if (phase_ != BattlePhase::Playing) {
        return;
    }
    UpdateEnemies(seconds);
    UpdateBoss(seconds);
    UpdateProjectiles(seconds);
    ProcessUltimateHits();
    player_.ResolveTargetHits();
    ProcessPlayerHits();
    ResolveProjectileImpacts();
    ResolveChargeImpacts();
    ProcessPlayerHitVisuals();
    if (kind_ == BattlePreviewKind::Day1 || kind_ == BattlePreviewKind::Day2) {
        director_.Update(seconds, CountActiveEnemies(), projectiles_.size(),
            [this](const EnemySpawnPlan& plan) { return SpawnEnemy(plan); });
    }
    if (player_.GetVitality().IsDefeated() || baseHitPoints_ <= 0) {
        phase_ = BattlePhase::GameOver;
    } else if ((kind_ == BattlePreviewKind::Day1 || kind_ == BattlePreviewKind::Day2)
            ? director_.IsComplete() && enemies_.empty() && projectiles_.empty() &&
                !HasActiveExplosionEffects() &&
                !player_.GetLeftArm().IsFistAway() && !player_.GetRightArm().IsFistAway()
            : kind_ == BattlePreviewKind::Boss
            ? boss_ && boss_->IsDefeated() && projectiles_.empty() &&
                !HasActiveExplosionEffects() &&
                !player_.GetLeftArm().IsFistAway() &&
                !player_.GetRightArm().IsFistAway()
            : enemies_.empty() && projectiles_.empty()) {
        phase_ = BattlePhase::Clear;
    }
}
