#include "Gameplay/Tutorial/TutorialEncounter.h"

#include "Collision/Collision.h"
#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr std::string_view kTrainingMissileId = "target_tutorial_missile";
constexpr float kTargetHeight = 4.55f;
constexpr float kTargetRadius = 1.25f;
constexpr float kTrainingMissileDistance = 18.0f;
constexpr float kBattlefieldMinimumZ = -16.0f;
constexpr float kBattlefieldMaximumZ = 42.0f;

constexpr std::array<Vector3, 6> kTargetPositions{{
    {-4.0f, kTargetHeight, 8.0f},
    {4.0f, kTargetHeight, 8.0f},
    {-8.0f, kTargetHeight, 16.0f},
    {8.0f, kTargetHeight, 16.0f},
    {-12.0f, kTargetHeight, 24.0f},
    {12.0f, kTargetHeight, 24.0f},
}};
}

void TutorialEncounter::Start(FirstPersonGame& player) {
    step_ = TutorialStep::VisualLink;
    targetRespawnRemaining_ = kTargetRespawnDelay;
    defenseSpawnRemaining_ = 0.0f;
    ResetTargets(player);
}

void TutorialEncounter::ResetTargets(FirstPersonGame& player) {
    projectileImpact_.reset();
    std::vector<std::string> oldIds;
    for (const FirstPersonTarget& target : player.GetTargets()) {
        if (target.id.starts_with("target_tutorial_")) oldIds.push_back(target.id);
    }
    for (const std::string& id : oldIds) player.RemoveTarget(id);
    missileActive_ = false;
    for (std::size_t index = 0; index < kTargetPositions.size(); ++index) {
        player.SpawnTarget({"target_tutorial_" + std::to_string(index),
            kTargetPositions[index], 1, {}, false, kTargetRadius});
    }
}

void TutorialEncounter::EnterStep(TutorialStep step, FirstPersonGame& player) {
    if (step == step_) return;
    step_ = step;
    if (step == TutorialStep::Punch || step == TutorialStep::RocketPunch ||
        step == TutorialStep::Complete) {
        ResetTargets(player);
        targetRespawnRemaining_ = kTargetRespawnDelay;
    } else if (step == TutorialStep::Defense) {
        projectileImpact_.reset();
        RemoveTrainingMissile(player);
        defenseSpawnRemaining_ = kDefenseInitialDelay;
    } else if (step == TutorialStep::Dodge) {
        projectileImpact_.reset();
        RemoveTrainingMissile(player);
        defenseSpawnRemaining_ = 0.0f;
    }
}

int TutorialEncounter::CountActiveTargets(const FirstPersonGame& player) const {
    int active = 0;
    for (const FirstPersonTarget& target : player.GetTargets()) {
        if (target.id.starts_with("target_tutorial_") &&
            target.id != kTrainingMissileId && target.hitPoints > 0) ++active;
    }
    return active;
}

void TutorialEncounter::UpdateBeforePlayer(float seconds,
    const TutorialSession& tutorial, FirstPersonGame& player) {
    if (tutorial.GetStep() != step_) EnterStep(tutorial.GetStep(), player);
    if (step_ == TutorialStep::Punch || step_ == TutorialStep::RocketPunch) {
        const bool needsTargets = step_ == TutorialStep::RocketPunch ||
            !tutorial.HasLeftHit() || !tutorial.HasRightHit();
        const bool fistsConnected = !player.GetLeftArm().IsFistAway() &&
            !player.GetRightArm().IsFistAway();
        if (tutorial.IsAwaitingInput() && needsTargets && fistsConnected &&
            CountActiveTargets(player) == 0) {
            targetRespawnRemaining_ = std::max(0.0f,
                targetRespawnRemaining_ - seconds);
            if (targetRespawnRemaining_ <= 0.0f) {
                ResetTargets(player);
                targetRespawnRemaining_ = kTargetRespawnDelay;
            }
        } else {
            targetRespawnRemaining_ = kTargetRespawnDelay;
        }
    }
    if (step_ != TutorialStep::Defense) return;
    if (missileActive_) {
        MoveTrainingMissile(seconds, player);
    } else if (tutorial.IsAwaitingInput()) {
        defenseSpawnRemaining_ = std::max(0.0f,
            defenseSpawnRemaining_ - seconds);
        if (defenseSpawnRemaining_ <= 0.0f) {
            SpawnTrainingMissile(player);
            defenseSpawnRemaining_ = kDefenseRespawnDelay;
        }
    }
}

void TutorialEncounter::SpawnTrainingMissile(FirstPersonGame& player) {
    missileTargetPosition_ = player.GetPlayer().GetPosition();
    missileTargetPosition_.y += FirstPersonTuning::kPlayerColliderCenterHeight;
    missilePosition_ = missileTargetPosition_;
    missilePosition_.z += kTrainingMissileDistance;
    missilePosition_.y = std::clamp(missilePosition_.y,
        config_.enemy.combatHeightMinimum,
        config_.enemy.combatHeightMaximum);
    previousMissilePosition_ = missilePosition_;
    missileDirection_ = {0.0f, 0.0f, -1.0f};
    missileTurnStartDirection_ = missileDirection_;
    missileTurnElapsed_ = 0.0f;
    missileFlightState_ = MissileFlightState::Outbound;
    missileActive_ = player.SpawnTarget({std::string(kTrainingMissileId),
        missilePosition_, 1, {}, false, config_.projectile.missileRadius,
        1.0f, TargetDisplayKind::Projectile});
}

void TutorialEncounter::MoveTrainingMissile(float seconds,
    FirstPersonGame& player) {
    previousMissilePosition_ = missilePosition_;
    if (missileFlightState_ == MissileFlightState::Outbound &&
        missilePosition_.z <=
        config_.projectile.missileTurnBoundaryZ) {
        missileFlightState_ = MissileFlightState::Turning;
        missileTurnStartDirection_ = missileDirection_;
        missileTurnElapsed_ = 0.0f;
    }
    if (missileFlightState_ == MissileFlightState::Turning) {
        missileTurnElapsed_ += seconds;
        const float ratio = std::clamp(missileTurnElapsed_ /
            config_.projectile.missileTurnDuration, 0.0f, 1.0f);
        const float targetX = missileTargetPosition_.x - missilePosition_.x;
        const float targetZ = missileTargetPosition_.z - missilePosition_.z;
        const float targetLength = std::hypot(targetX, targetZ);
        const Vector3 targetDirection = targetLength > 0.00001f
            ? Vector3{targetX / targetLength, 0.0f, targetZ / targetLength}
            : missileTurnStartDirection_;
        const float blendX = missileTurnStartDirection_.x * (1.0f - ratio) +
            targetDirection.x * ratio;
        const float blendZ = missileTurnStartDirection_.z * (1.0f - ratio) +
            targetDirection.z * ratio;
        const float blendLength = std::hypot(blendX, blendZ);
        if (blendLength > 0.00001f) {
            missileDirection_ = {blendX / blendLength, 0.0f,
                blendZ / blendLength};
        }
        if (ratio >= 1.0f) missileFlightState_ = MissileFlightState::BaseRun;
    }
    missilePosition_.x += missileDirection_.x *
        config_.projectile.missileSpeed * seconds;
    missilePosition_.z += missileDirection_.z *
        config_.projectile.missileSpeed * seconds;
    player.MoveTarget(kTrainingMissileId, missilePosition_);
}

void TutorialEncounter::RemoveTrainingMissile(FirstPersonGame& player) {
    player.RemoveTarget(kTrainingMissileId);
    missileActive_ = false;
}

bool TutorialEncounter::ResolveAfterPlayer(FirstPersonGame& player) {
    if (!missileActive_) return false;
    const bool intercepted = std::any_of(player.GetEvents().targetHits.begin(),
        player.GetEvents().targetHits.end(), [](const CombatHitEvent& hit) {
            return hit.targetId == kTrainingMissileId;
        });
    const auto& targets = player.GetTargets();
    const auto missile = std::find_if(targets.begin(), targets.end(),
        [](const FirstPersonTarget& target) {
            return target.id == kTrainingMissileId;
        });
    if (missile == targets.end() || missile->hitPoints <= 0) {
        if (intercepted) projectileImpact_ =
            TutorialProjectileImpact{missilePosition_, 1.0f};
        RemoveTrainingMissile(player);
        return intercepted;
    }
    Vector3 playerPosition = player.GetPlayer().GetPosition();
    Vector3 previousPlayerPosition = player.GetPlayer().GetPreviousPosition();
    playerPosition.y += FirstPersonTuning::kPlayerColliderCenterHeight;
    previousPlayerPosition.y += FirstPersonTuning::kPlayerColliderCenterHeight;
    if (Collision::SweepSphereAgainstMovingSphere(previousMissilePosition_,
            missilePosition_, config_.projectile.missileRadius,
            previousPlayerPosition, playerPosition,
            FirstPersonTuning::kPlayerColliderRadius).didHit) {
        const PlayerHitResult result = player.ReceivePlayerHit(
            PlayerHitType::Projectile);
        projectileImpact_ = TutorialProjectileImpact{missilePosition_,
            result.shieldLayersLost > 0 ? 0.55f : 1.0f,
            result.shieldLayersLost > 0};
        RemoveTrainingMissile(player);
        return true;
    }
    if (missilePosition_.z < kBattlefieldMinimumZ - 2.0f ||
        missilePosition_.z > kBattlefieldMaximumZ + 2.0f) {
        RemoveTrainingMissile(player);
    }
    return false;
}

std::optional<TutorialProjectileImpact>
TutorialEncounter::ConsumeProjectileImpact() {
    auto impact = projectileImpact_;
    projectileImpact_.reset();
    return impact;
}
