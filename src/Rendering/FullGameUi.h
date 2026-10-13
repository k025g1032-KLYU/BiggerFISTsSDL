#pragma once

#include "Game/GameFlow.h"

class TutorialSession;
class CombatSession;
class FirstPersonGame;
class UiDrawList;
class CockpitPresentation;
struct ModelScene;

void BuildFullGameUi(const FlowScreenState& flow, const TutorialSession* tutorial,
    const CombatSession* battle, const FirstPersonGame& player,
    double elapsedSeconds, const CockpitPresentation& cockpit,
    bool combatDataVisible, const ModelScene& scene, UiDrawList& list,
    float damageVignetteAlpha = 0.0f,
    int outputWidth = 1280, int outputHeight = 720);
