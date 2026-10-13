#pragma once

class CombatSession;
struct ModelScene;
class UiDrawList;

void BuildBattleHitboxOverlay(const CombatSession& battle,
    const ModelScene& scene, UiDrawList& list);
