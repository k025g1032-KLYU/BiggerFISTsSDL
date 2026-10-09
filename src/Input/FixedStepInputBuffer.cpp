#include "Input/FixedStepInputBuffer.h"

void FixedStepInputBuffer::QueueFrame(const GameInput& input,
    float mouseDeltaX, float mouseDeltaY) {
    current_ = input;
    backgroundPressed_ |= input.changeBackground.pressed;
    backgroundReleased_ |= input.changeBackground.released;
    resetPressed_ |= input.resetTarget.pressed;
    resetReleased_ |= input.resetTarget.released;
    battleStartPressed_ |= input.battleStart.pressed;
    confirmPressed_ |= input.confirm.pressed;
    skipPressed_ |= input.skip.pressed;
    skipPhasePressed_ |= input.skipPhase.pressed;
    switchViewPressed_ |= input.switchView.pressed;
    dodgePressed_ |= input.dodge.pressed;
    projectileHitPressed_ |= input.testProjectileHit.pressed;
    chargeHitPressed_ |= input.testChargeHit.pressed;
    burstHitPressed_ |= input.testBurstHit.pressed;
    if (practiceCommand_ == PracticeCommand::None)
        practiceCommand_ = input.practiceCommand;
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
    result.game.battleStart.pressed = battleStartPressed_;
    result.game.confirm.pressed = confirmPressed_;
    result.game.skip.pressed = skipPressed_;
    result.game.skipPhase.pressed = skipPhasePressed_;
    result.game.switchView.pressed = switchViewPressed_;
    result.game.dodge.pressed = dodgePressed_;
    result.game.testProjectileHit.pressed = projectileHitPressed_;
    result.game.testChargeHit.pressed = chargeHitPressed_;
    result.game.testBurstHit.pressed = burstHitPressed_;
    result.game.practiceCommand = practiceCommand_;
    backgroundPressed_ = false;
    backgroundReleased_ = false;
    resetPressed_ = false;
    resetReleased_ = false;
    battleStartPressed_ = false;
    confirmPressed_ = false;
    skipPressed_ = false;
    skipPhasePressed_ = false;
    switchViewPressed_ = false;
    dodgePressed_ = false;
    projectileHitPressed_ = false;
    chargeHitPressed_ = false;
    burstHitPressed_ = false;
    practiceCommand_ = PracticeCommand::None;

    result.mouseDeltaX = pendingMouseDeltaX_ / static_cast<float>(remainingSteps);
    result.mouseDeltaY = pendingMouseDeltaY_ / static_cast<float>(remainingSteps);
    pendingMouseDeltaX_ -= result.mouseDeltaX;
    pendingMouseDeltaY_ -= result.mouseDeltaY;
    return result;
}

void FixedStepInputBuffer::Reset() {
    *this = {};
}
