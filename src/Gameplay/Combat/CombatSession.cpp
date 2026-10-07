#include "Gameplay/Combat/CombatSession.h"

#include "Collision/Collision.h"
#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <utility>

namespace {
constexpr float kBaseRadius = 7.0f;
constexpr Vector3 kBasePosition{0.0f, 0.0f, -16.0f};
constexpr std::string_view kBossTargetId = "target_boss_core";

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
    director_.Reset();
    random_.seed(0xB16F157u);
    baseHitPoints_ = kind_ == BattlePreviewKind::Day2
        ? config_.enemy.day2BaseHitPoints : config_.enemy.baseHitPoints;
    phase_ = BattlePhase::Ready;
    countdown_ = 0.0f;
    player_.ResetCombatState();
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
            1000000, {}, false, config_.boss.coreRadius});
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

bool CombatSession::SpawnEnemy(const EnemySpawnPlan& plan) {
    BattleEnemy enemy;
    enemy.id = NextTargetId();
    enemy.type = plan.type;
    enemy.lane = plan.lane;
    const float laneWidth = (config_.enemy.deploymentMaximumX -
        config_.enemy.deploymentMinimumX) / 5.0f;
    const float laneStart = config_.enemy.deploymentMinimumX +
        static_cast<float>(plan.lane) * laneWidth;
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
    enemy.previousPosition = enemy.position;
    enemy.attackTimer = plan.type == BattleEnemyType::MissileUnit
        ? config_.enemy.missileInitialFireDelay
        : config_.enemy.bomberInitialFireDelay;
    enemy.velocity.x = enemy.position.x <= 0.0f ? 1.0f : -1.0f;
    if (plan.type == BattleEnemyType::Bomber) {
        std::uniform_real_distribution<float> speedDistribution(
            config_.enemy.bomberMinimumMoveSpeed,
            config_.enemy.bomberMaximumMoveSpeed);
        std::uniform_int_distribution<int> directionDistribution(0, 1);
        enemy.velocity.x = speedDistribution(random_) *
            (directionDistribution(random_) == 0 ? -1.0f : 1.0f);
        std::uniform_real_distribution<float> phaseDistribution(0.0f, 6.2831853f);
        enemy.rotation = phaseDistribution(random_);
    }
    if (!player_.SpawnTarget({enemy.id, enemy.position, 1, {}, false,
            config_.enemy.radius, 1.35f})) {
        return false;
    }
    enemies_.push_back(std::move(enemy));
    return true;
}

void CombatSession::SpawnProjectile(BattleProjectileType type,
    const Vector3& position, const Vector3& direction, float phase) {
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
    projectile.turnStartDirection = projectile.direction;
    const float radius = type == BattleProjectileType::Missile
        ? config_.projectile.missileRadius : type == BattleProjectileType::FloatingBomb
        ? config_.projectile.bombRadius : type == BattleProjectileType::CrystalSpike
        ? config_.projectile.crystalSpikeRadius
        : config_.projectile.crystalCubeRadius;
    if (player_.SpawnTarget({projectile.id, projectile.position, 1, {}, false,
            radius})) {
        projectiles_.push_back(std::move(projectile));
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
    for (const BossAttackEvent& attack :
            boss_->Update(seconds, playerPosition, threat)) {
        if (attack.type == BossAttackType::CrystalSpike ||
            attack.type == BossAttackType::CrystalCube) {
            const Vector3 offset{playerPosition.x - attack.position.x,
                playerPosition.y + FirstPersonTuning::kPlayerColliderCenterHeight -
                    attack.position.y,
                playerPosition.z - attack.position.z};
            const float length = std::sqrt(offset.x * offset.x +
                offset.y * offset.y + offset.z * offset.z);
            const Vector3 direction = length > 0.00001f
                ? Vector3{offset.x / length, offset.y / length, offset.z / length}
                : Vector3{0.0f, 0.0f, -1.0f};
            SpawnProjectile(attack.type == BossAttackType::CrystalSpike
                ? BattleProjectileType::CrystalSpike
                : BattleProjectileType::CrystalCube,
                attack.position, direction);
        } else if (attack.type == BossAttackType::SpreadBurst) {
            player_.ReceivePlayerHit(PlayerHitType::BossSpreadBurst,
                {playerPosition.x - attack.position.x, 0.0f,
                    playerPosition.z - attack.position.z});
        } else {
            player_.ReceivePlayerHit(attack.type == BossAttackType::ChargeImpact
                ? PlayerHitType::ChargeUnit : PlayerHitType::Projectile,
                {playerPosition.x - attack.position.x, 0.0f,
                    playerPosition.z - attack.position.z});
        }
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
        enemy.chargeDamageActiveThisStep = false;
        if (enemy.defeated) {
            enemy.stateTimer += seconds;
            enemy.position.x += enemy.velocity.x * 14.0f * seconds;
            enemy.position.y += (11.0f - 14.0f * enemy.stateTimer) * seconds;
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
            UpdateChargeEnemy(enemy, seconds);
        } else {
            const bool isMissile = enemy.type == BattleEnemyType::MissileUnit;
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
                                enemy.position.z - 1.1f});
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
                    {enemy.position.x, enemy.position.y,
                        enemy.position.z - 1.1f}, {0.0f, 0.0f, -1.0f},
                    enemy.rotation);
                enemy.attackTimer = config_.enemy.bomberFireCooldown;
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
        return enemy.defeated &&
            enemy.stateTimer >= config_.enemy.defeatLaunchDuration;
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
        enemy.chargeDirection = toPlayer;
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
        }
        break;
    case BattleEnemy::ChargeState::ReacquiringTarget:
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
        const bool reachedObstacle = moveSafely(enemy.chargeDirection,
            config_.enemy.chargeDashSpeed * seconds);
        enemy.chargeDamageActiveThisStep = true;
        const float passed = (enemy.position.x - enemy.chargeTarget.x) *
            enemy.chargeDirection.x +
            (enemy.position.z - enemy.chargeTarget.z) * enemy.chargeDirection.z;
        if (reachedObstacle) {
            enemy.chargeState = BattleEnemy::ChargeState::ReacquiringTarget;
            enemy.chargeTimer = 0.0f;
        } else if (passed >= config_.enemy.chargeTargetPassDistance) {
            enemy.chargeState = BattleEnemy::ChargeState::MissBraking;
            enemy.chargeBrakingSpeed = config_.enemy.chargeDashSpeed;
            enemy.chargeTimer = 0.0f;
        }
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
        player_.MoveTarget(projectile.id, projectile.position);
        if (projectile.position.z < -18.0f ||
            projectile.position.z > 44.0f ||
            projectile.position.x < -25.1f ||
            projectile.position.x > 25.1f || projectile.age > 20.0f) {
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
            player_.ReceivePlayerHit(PlayerHitType::Projectile);
            player_.RemoveTarget(projectile.id);
            projectile.age = -1.0f;
        } else if (Collision::SweepSphereAgainstSphere(projectile.previousPosition,
                projectile.position, radius,
                {kBasePosition, kBaseRadius}).didHit) {
            baseHitPoints_ = std::max(0, baseHitPoints_ - 1);
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
            player_.ReceivePlayerHit(PlayerHitType::ChargeUnit,
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
        ApplyTargetDamage(hit.targetId, 1);
    }
    std::erase_if(projectiles_, [](const BattleProjectile& projectile) {
        return projectile.age < 0.0f;
    });
}

bool CombatSession::ApplyTargetDamage(std::string_view id, int damage) {
    if (phase_ != BattlePhase::Playing || damage <= 0) {
        return false;
    }
    if (id == kBossTargetId && boss_) {
        const bool applied = boss_->ReceiveHit(damage);
        if (applied && boss_->GetMode() == BossMode::Defeating) {
            player_.RemoveTarget(kBossTargetId);
        }
        return applied;
    }
    for (BattleEnemy& enemy : enemies_) {
        if (enemy.id == id && !enemy.defeated) {
            enemy.defeated = true;
            enemy.stateTimer = 0.0f;
            player_.RemoveTarget(id);
            return true;
        }
    }
    for (BattleProjectile& projectile : projectiles_) {
        if (projectile.id == id && projectile.age >= 0.0f) {
            projectile.age = -1.0f;
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
    for (BattleEnemy& enemy : enemies_) {
        if (enemy.defeated || ultimateHitIds_.contains(enemy.id) ||
            !Collision::SweepSphereAgainstMovingSphere(previous, current, radius,
                enemy.previousPosition, enemy.position,
                config_.enemy.radius).didHit) {
            continue;
        }
        if (ApplyTargetDamage(enemy.id, 100)) {
            ultimateHitIds_.insert(enemy.id);
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
        if (ApplyTargetDamage(kBossTargetId, config_.boss.ultimateDamage)) {
            ultimateHitIds_.insert(std::string(kBossTargetId));
        }
    }
    std::erase_if(projectiles_, [](const BattleProjectile& projectile) {
        return projectile.age < 0.0f;
    });
}

void CombatSession::Update(float seconds, const GameInput& input,
    float mouseX, float mouseY) {
    if (input.resetTarget.pressed) {
        Reset();
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
    if (kind_ == BattlePreviewKind::Day1 || kind_ == BattlePreviewKind::Day2) {
        director_.Update(seconds, CountActiveEnemies(), projectiles_.size(),
            [this](const EnemySpawnPlan& plan) { return SpawnEnemy(plan); });
    }
    if (player_.GetVitality().IsDefeated() || baseHitPoints_ <= 0) {
        phase_ = BattlePhase::GameOver;
    } else if ((kind_ == BattlePreviewKind::Day1 || kind_ == BattlePreviewKind::Day2)
            ? director_.IsComplete() && enemies_.empty() && projectiles_.empty() &&
                !player_.GetLeftArm().IsFistAway() && !player_.GetRightArm().IsFistAway()
            : kind_ == BattlePreviewKind::Boss
            ? boss_ && boss_->IsDefeated() && projectiles_.empty() &&
                !player_.GetLeftArm().IsFistAway() &&
                !player_.GetRightArm().IsFistAway()
            : enemies_.empty() && projectiles_.empty()) {
        phase_ = BattlePhase::Clear;
    }
}
