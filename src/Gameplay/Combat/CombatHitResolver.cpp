#include "Gameplay/Combat/CombatHitResolver.h"

#include "Collision/Collision.h"
#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <cmath>

std::vector<CombatHitEvent> FindArmHits(const PunchArm& arm, PunchArmSide side,
    std::span<const CombatHitTarget> targets,
    const std::unordered_set<std::string>& alreadyHit) {
    if (!arm.IsPunchDamageActiveThisFrame() && !arm.IsRocketDamageActiveThisFrame()) {
        return {};
    }
    std::vector<CombatHitEvent> hits;
    for (const CombatHitTarget& target : targets) {
        if (target.id.empty() || alreadyHit.contains(std::string(target.id)) ||
            !std::isfinite(target.punchRadiusMultiplier) ||
            target.punchRadiusMultiplier <= 0.0f) {
            continue;
        }
        const float fistRadius = FirstPersonTuning::kFistRadius *
            (arm.IsPunchDamageActiveThisFrame() ? target.punchRadiusMultiplier : 1.0f);
        const Collision::SweepHit hit = Collision::SweepSphereAgainstMovingSphere(
            arm.GetPreviousFistPosition(), arm.GetFistPosition(),
            fistRadius, target.previousCenter,
            target.currentCenter, target.radius);
        if (hit.didHit) {
            hits.push_back({std::string(target.id), side, arm.GetAttackType(), hit.time});
        }
    }
    std::sort(hits.begin(), hits.end(), [](const CombatHitEvent& first,
        const CombatHitEvent& second) { return first.time < second.time; });
    return hits;
}
