#pragma once

#include "Game/FirstPersonGame.h"
#include "Gameplay/Combat/BattleFeedbackEvent.h"

#include <algorithm>
#include <vector>

inline std::vector<BattleFeedbackEvent> BuildPlayerAudioEvents(
    const FirstPersonEvents& events,
    const std::vector<FirstPersonTarget>& targets, bool battleActive) {
    std::vector<BattleFeedbackEvent> result;
    for (const CombatHitEvent& hit : events.targetHits) {
        const bool staticTarget = std::any_of(targets.begin(), targets.end(),
            [&hit](const FirstPersonTarget& target) {
                return target.id == hit.targetId &&
                    target.displayKind == TargetDisplayKind::Static;
            });
        if (!battleActive || staticTarget) {
            result.push_back(hit.attackType == PunchArmAttackType::Punch
                ? BattleFeedbackEvent::PunchImpact
                : BattleFeedbackEvent::RocketImpact);
        }
    }
    if (events.leftPunchBegan)
        result.push_back(BattleFeedbackEvent::PunchSwing);
    if (events.rightPunchBegan)
        result.push_back(BattleFeedbackEvent::PunchSwing);
    if (events.leftRocketReconnected)
        result.push_back(BattleFeedbackEvent::RocketReconnect);
    if (events.rightRocketReconnected)
        result.push_back(BattleFeedbackEvent::RocketReconnect);
    return result;
}
