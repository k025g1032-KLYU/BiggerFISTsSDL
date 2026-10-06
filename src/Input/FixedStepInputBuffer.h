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
    bool dodgePressed_ = false;
    bool projectileHitPressed_ = false;
    bool chargeHitPressed_ = false;
    bool burstHitPressed_ = false;
    float pendingMouseDeltaX_ = 0.0f;
    float pendingMouseDeltaY_ = 0.0f;
};
