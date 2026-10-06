#pragma once

#include "Gameplay/SimulationTiming.h"

class FixedStepClock {
public:
    static constexpr double kStepSeconds = SimulationTiming::kFixedStepSeconds;
    static constexpr int kMaximumStepsPerFrame = 5;

    int Advance(double frameSeconds);
    double GetDroppedSecondsLastFrame() const { return droppedSecondsLastFrame_; }

private:
    double accumulatedSeconds_ = 0.0;
    double droppedSecondsLastFrame_ = 0.0;
};
