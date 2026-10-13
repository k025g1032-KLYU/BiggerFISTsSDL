#pragma once

#include "Gameplay/Combat/BattleMode.h"

#include <string>

struct ApplicationOptions {
    bool smokeTest = false;
    int smokeFrames = 30;
    bool scenePreview = false;
    bool missileUnitModelPreview = false;
    bool firstPersonPreview = false;
    bool playerCombatPreview = false;
    bool showHitboxes = false;
    bool dynamicTargetSmokeTest = false;
    bool presentationPreview = false;
    bool presentationBoot = false;
    int presentationTick = -1;
    std::string presentationCapture;
    std::string captureFrame;
    int captureAtFrame = -1;
    std::string replayInput;
    BattlePreviewKind battlePreview = BattlePreviewKind::None;
};

class Application {
public:
    int Run(const ApplicationOptions& options = {});
};
