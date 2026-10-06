#include "Input/FixedStepInputBuffer.h"

void FixedStepInputBuffer::QueueFrame(const GameInput& input,
    float mouseDeltaX, float mouseDeltaY) {
    current_ = input;
    backgroundPressed_ |= input.changeBackground.pressed;
    backgroundReleased_ |= input.changeBackground.released;
    resetPressed_ |= input.resetTarget.pressed;
    resetReleased_ |= input.resetTarget.released;
    pendingMouseDeltaX_ += mouseDeltaX;
    pendingMouseDeltaY_ += mouseDeltaY;
}

SimulationStepInput FixedStepInputBuffer::ConsumeStep(int remainingSteps) {
    SimulationStepInput result{};
    if (remainingSteps <= 0) {
        return result;
    }
    result.game = current_;
    result.game.changeBackground.pressed = backgroundPressed_;
    result.game.changeBackground.released = backgroundReleased_;
    result.game.resetTarget.pressed = resetPressed_;
    result.game.resetTarget.released = resetReleased_;
    backgroundPressed_ = false;
    backgroundReleased_ = false;
    resetPressed_ = false;
    resetReleased_ = false;

    result.mouseDeltaX = pendingMouseDeltaX_ / static_cast<float>(remainingSteps);
    result.mouseDeltaY = pendingMouseDeltaY_ / static_cast<float>(remainingSteps);
    pendingMouseDeltaX_ -= result.mouseDeltaX;
    pendingMouseDeltaY_ -= result.mouseDeltaY;
    return result;
}

void FixedStepInputBuffer::Reset() {
    *this = {};
}
