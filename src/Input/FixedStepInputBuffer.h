#pragma once

#include "Input/GameInput.h"

struct SimulationStepInput {
    GameInput game{};
    float mouseDeltaX = 0.0f;
    float mouseDeltaY = 0.0f;
};

class FixedStepInputBuffer {
public:
    void QueueFrame(const GameInput& input, float mouseDeltaX, float mouseDeltaY);
    SimulationStepInput ConsumeStep(int remainingSteps);
    void Reset();

private:
    GameInput current_{};
    bool backgroundPressed_ = false;
    bool backgroundReleased_ = false;
    bool resetPressed_ = false;
    bool resetReleased_ = false;
    bool battleStartPressed_ = false;
    bool confirmPressed_ = false;
    bool skipPressed_ = false;
    bool skipPhasePressed_ = false;
    bool switchViewPressed_ = false;
    bool toggleThirdPersonPressed_ = false;
    bool cycleCockpitModelPressed_ = false;
    bool toggleCombatDataPressed_ = false;
    bool dodgePressed_ = false;
    bool projectileHitPressed_ = false;
    bool chargeHitPressed_ = false;
    bool burstHitPressed_ = false;
    PracticeCommand practiceCommand_ = PracticeCommand::None;
    float pendingMouseDeltaX_ = 0.0f;
    float pendingMouseDeltaY_ = 0.0f;
};
