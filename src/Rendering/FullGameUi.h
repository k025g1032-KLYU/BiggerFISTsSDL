#pragma once

#include "Game/GameFlow.h"

class TutorialSession;
class CombatSession;
class FirstPersonGame;
class UiDrawList;

void BuildFullGameUi(const FlowScreenState& flow, const TutorialSession* tutorial,
    const CombatSession* battle, const FirstPersonGame& player,
    double elapsedSeconds, float cockpitStartupRemaining, UiDrawList& list);
