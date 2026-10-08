#include "Game/FirstPersonGame.h"
#include "Input/FixedStepInputBuffer.h"
#include "Platform/FixedStepClock.h"

#include <cmath>
#include <cstdio>
#include <limits>

namespace {
bool Expect(bool condition, const char* description) {
    if (!condition) {
        std::printf("FAIL: %s\n", description);
    }
    return condition;
}

struct Result {
    Vector3 playerPosition{};
    float cameraYaw = 0.0f;
    PunchArmState armState = PunchArmState::Ready;
    double elapsedSeconds = 0.0;
    int simulationSteps = 0;
    int shieldLayers = 0;
};

Result SimulateOneSecond(int renderedFrames) {
    FirstPersonGame game(GameConfig{}, {0.0f, 5.85f, 0.0f}, {0.0f, 4.55f, 8.0f});
    FixedStepClock clock;
    FixedStepInputBuffer inputBuffer;
    Result result{};
    GameInput input{};
    input.moveY = -1.0f;
    input.leftPunchKeyDown = true;
    input.testProjectileHit.pressed = true;

    for (int frame = 0; frame < renderedFrames; ++frame) {
        input.testProjectileHit.pressed = frame == 0;
        inputBuffer.QueueFrame(input, 60.0f / renderedFrames, 0.0f);
        const int steps = clock.Advance(1.0 / renderedFrames);
        for (int step = 0; step < steps; ++step) {
            const SimulationStepInput stepInput = inputBuffer.ConsumeStep(steps - step);
            game.Update(FixedStepClock::kStepSeconds, stepInput.game,
                stepInput.mouseDeltaX, stepInput.mouseDeltaY);
            ++result.simulationSteps;
        }
    }
    result.playerPosition = game.GetPlayer().GetPosition();
    result.cameraYaw = game.GetCamera().GetYaw();
    result.armState = game.GetLeftArm().GetState();
    result.elapsedSeconds = game.GetElapsedSeconds();
    result.shieldLayers = game.GetVitality().GetShieldLayers();
    return result;
}
}

int main() {
    bool success = true;
    FixedStepClock renderClock;
    success &= Expect(renderClock.Advance(1.0 / 120.0) == 0 &&
        std::abs(renderClock.GetInterpolationAlpha() - 0.5) < 0.0001 &&
        renderClock.Advance(1.0 / 120.0) == 1 &&
        renderClock.GetInterpolationAlpha() < 0.0001,
        "Render alpha tracks time between fixed updates");
    const Result thirtyFps = SimulateOneSecond(30);
    const Result sixtyFps = SimulateOneSecond(60);
    const Result oneHundredTwentyFps = SimulateOneSecond(120);
    success &= Expect(thirtyFps.simulationSteps == 60 &&
        sixtyFps.simulationSteps == 60 && oneHundredTwentyFps.simulationSteps == 60,
        "30, 60 and 120 rendered frames all execute 60 simulation steps");
    success &= Expect(std::abs(thirtyFps.playerPosition.x - sixtyFps.playerPosition.x) < 0.0001f &&
        std::abs(sixtyFps.playerPosition.x - oneHundredTwentyFps.playerPosition.x) < 0.0001f &&
        std::abs(thirtyFps.playerPosition.z - oneHundredTwentyFps.playerPosition.z) < 0.0001f &&
        std::abs(thirtyFps.cameraYaw - oneHundredTwentyFps.cameraYaw) < 0.0001f &&
        thirtyFps.armState == oneHundredTwentyFps.armState &&
        thirtyFps.shieldLayers == 2 && sixtyFps.shieldLayers == 2 &&
        oneHundredTwentyFps.shieldLayers == 2 &&
        std::abs(thirtyFps.elapsedSeconds - 1.0) < 0.0001,
        "Movement, look and charge state agree across render frame rates");

    GameConfig shortCap{};
    shortCap.maxMovementDeltaTime = 0.001f;
    FirstPersonGame cappedGame(shortCap, {0.0f, 5.85f, 0.0f}, {0.0f, 4.55f, 8.0f});
    GameInput forward{};
    forward.moveY = -1.0f;
    cappedGame.Update(FixedStepClock::kStepSeconds, forward, 0.0f, 0.0f);
    success &= Expect(cappedGame.GetPlayer().GetVelocity().z > 0.09f,
        "A smaller legacy delta cap cannot shorten the fixed simulation step");

    FixedStepInputBuffer inputBuffer;
    GameInput press{};
    press.resetTarget.pressed = true;
    press.testChargeHit.pressed = true;
    inputBuffer.QueueFrame(press, 12.0f, -6.0f);
    inputBuffer.QueueFrame({}, 0.0f, 0.0f);
    const SimulationStepInput first = inputBuffer.ConsumeStep(2);
    const SimulationStepInput second = inputBuffer.ConsumeStep(1);
    success &= Expect(first.game.resetTarget.pressed &&
        first.game.testChargeHit.pressed &&
        !second.game.resetTarget.pressed &&
        !second.game.testChargeHit.pressed &&
        std::abs(first.mouseDeltaX - 6.0f) < 0.0001f &&
        std::abs(second.mouseDeltaX - 6.0f) < 0.0001f &&
        std::abs(first.mouseDeltaY + 3.0f) < 0.0001f &&
        std::abs(second.mouseDeltaY + 3.0f) < 0.0001f,
        "A queued reset survives a frame without a step and mouse look is shared");

    inputBuffer.QueueFrame(press, 5.0f, 4.0f);
    inputBuffer.Reset();
    const SimulationStepInput afterFocusLoss = inputBuffer.ConsumeStep(1);
    success &= Expect(!afterFocusLoss.game.resetTarget.pressed &&
        !afterFocusLoss.game.testChargeHit.pressed &&
        afterFocusLoss.mouseDeltaX == 0.0f && afterFocusLoss.mouseDeltaY == 0.0f,
        "Focus loss clears pending actions and mouse motion");

    FixedStepClock stalledClock;
    success &= Expect(stalledClock.Advance(0.5) == FixedStepClock::kMaximumStepsPerFrame &&
        stalledClock.GetDroppedSecondsLastFrame() > 0.4,
        "A long frame bounds catch-up work and reports dropped simulation time");
    success &= Expect(stalledClock.Advance(std::numeric_limits<double>::quiet_NaN()) == 0,
        "An invalid frame duration cannot advance the simulation");

    if (success) {
        std::puts("PASS: fixed 60 Hz simulation and buffered input");
    }
    return success ? 0 : 1;
}
