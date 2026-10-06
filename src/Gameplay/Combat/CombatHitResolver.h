#pragma once

#include "Gameplay/Player/PunchArm.h"

#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <vector>

struct CombatHitTarget {
    std::string_view id;
    Vector3 previousCenter{};
    Vector3 currentCenter{};
    float radius = 0.0f;
    float punchRadiusMultiplier = 1.0f;
};

struct CombatHitEvent {
    std::string targetId;
    PunchArmSide armSide = PunchArmSide::Left;
    PunchArmAttackType attackType = PunchArmAttackType::None;
    float time = 0.0f;
};

std::vector<CombatHitEvent> FindArmHits(const PunchArm& arm, PunchArmSide side,
    std::span<const CombatHitTarget> targets,
    const std::unordered_set<std::string>& alreadyHit);
