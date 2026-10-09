#pragma once

#include "Game/GameFlow.h"

class TutorialSession;
class CombatSession;
class FirstPersonGame;
class UiDrawList;
class CockpitPresentation;

void BuildFullGameUi(const FlowScreenState& flow, const TutorialSession* tutorial,
    const CombatSession* battle, const FirstPersonGame& player,
    double elapsedSeconds, const CockpitPresentation& cockpit,
    bool practiceDataVisible, UiDrawList& list);
