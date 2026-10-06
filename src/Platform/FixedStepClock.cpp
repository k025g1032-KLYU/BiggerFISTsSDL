#include "Platform/FixedStepClock.h"

#include <algorithm>
#include <cmath>

int FixedStepClock::Advance(double frameSeconds) {
    droppedSecondsLastFrame_ = 0.0;
    if (!std::isfinite(frameSeconds) || frameSeconds < 0.0) {
        return 0;
    }

    constexpr double maximumAcceptedFrameSeconds = 0.25;
    const double acceptedSeconds = std::min(frameSeconds, maximumAcceptedFrameSeconds);
    droppedSecondsLastFrame_ = frameSeconds - acceptedSeconds;
    accumulatedSeconds_ += acceptedSeconds;

    int steps = 0;
    while (steps < kMaximumStepsPerFrame &&
        accumulatedSeconds_ + 1.0e-12 >= kStepSeconds) {
        accumulatedSeconds_ -= kStepSeconds;
        ++steps;
    }
    if (steps == kMaximumStepsPerFrame && accumulatedSeconds_ + 1.0e-12 >= kStepSeconds) {
        const double wholeSteps = std::floor((accumulatedSeconds_ + 1.0e-12) / kStepSeconds);
        const double skippedSeconds = wholeSteps * kStepSeconds;
        droppedSecondsLastFrame_ += skippedSeconds;
        accumulatedSeconds_ -= skippedSeconds;
    }
    accumulatedSeconds_ = std::max(0.0, accumulatedSeconds_);
    return steps;
}
