#pragma once

#include "Gameplay/Combat/BattleMode.h"

struct ApplicationOptions {
    bool smokeTest = false;
    int smokeFrames = 30;
    bool scenePreview = false;
    bool missileUnitModelPreview = false;
    bool firstPersonPreview = false;
    bool playerCombatPreview = false;
    bool dynamicTargetSmokeTest = false;
    BattlePreviewKind battlePreview = BattlePreviewKind::None;
};

class Application {
public:
    int Run(const ApplicationOptions& options = {});
};
