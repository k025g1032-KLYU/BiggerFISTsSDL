#include "Gameplay/Tutorial/TutorialSession.h"

#include <cstdio>

namespace {
bool Check(bool condition, const char* description) {
    if (!condition) std::printf("FAIL: %s\n", description);
    return condition;
}
void Wait(TutorialSession& tutorial, TutorialObservation& observation,
    int steps = 80) {
    for (int index = 0; index < steps; ++index)
        tutorial.Update(1.0f / 60.0f, {}, observation);
}
}

int main() {
    bool success = true;
    TutorialObservation player{};
    TutorialSession tutorial;
    tutorial.Start(player);
    GameInput fullInput;
    fullInput.moveX = 1.0f;
    fullInput.leftPunchKeyDown = fullInput.leftMouseDown = true;
    fullInput.dodge.pressed = true;
    const GameInput filtered = tutorial.FilterInput(fullInput);
    success &= Check(filtered.moveX == 0.0f && !filtered.leftPunchKeyDown &&
        !filtered.leftMouseDown && !filtered.dodge.pressed,
        "VisualLink filters movement, arms and dodge");
    Wait(tutorial, player);
    player.cameraYaw = 1.6f;
    GameInput view;
    view.switchView.pressed = true;
    tutorial.Update(1.0f / 60.0f, view, player);
    for (int index = 0; index < 80; ++index)
        tutorial.Update(1.0f / 60.0f, {}, player, true);
    success &= Check(tutorial.GetStep() == TutorialStep::VisualLink &&
        tutorial.IsConfirming(), "confirmation waits for the system voice");
    tutorial.Update(1.0f / 60.0f, {}, player, false);
    success &= Check(tutorial.GetStep() == TutorialStep::Mobility &&
        tutorial.IsCockpitConnected(), "VisualLink connects cockpit");
    Wait(tutorial, player);
    player.cameraYaw = 2.2f;
    player.playerPosition.z = 5.1f;
    tutorial.Update(1.0f / 60.0f, {}, player);
    Wait(tutorial, player);
    success &= Check(tutorial.GetStep() == TutorialStep::Punch,
        "Mobility requires movement and look");
    fullInput.shiftDown = true;
    fullInput.rightMouseDown = true;
    const GameInput punchInput = tutorial.FilterInput(fullInput);
    success &= Check(punchInput.leftMouseDown && punchInput.rightMouseDown &&
        punchInput.leftPunchKeyDown && !punchInput.allowRocketCharge &&
        !punchInput.dodge.pressed,
        "Punch step keeps Shift+mouse and keyboard Punch while blocking Rocket");
    player.leftPunchHit = player.rightPunchHit = true;
    tutorial.Update(1.0f / 60.0f, {}, player);
    player.leftPunchHit = player.rightPunchHit = false;
    Wait(tutorial, player);
    success &= Check(tutorial.GetStep() == TutorialStep::RocketPunch,
        "both ordinary punches advance to RocketPunch");
    success &= Check(tutorial.FilterInput(fullInput).allowRocketCharge,
        "Rocket Punch step restores mouse Rocket input");
    player.leftRocketHit = player.rightRocketHit = true;
    tutorial.Update(1.0f / 60.0f, {}, player);
    player.leftRocketHit = player.rightRocketHit = false;
    Wait(tutorial, player);
    success &= Check(tutorial.GetStep() == TutorialStep::Defense,
        "both returned rockets advance to Defense");
    player.playerHit = true;
    tutorial.Update(1.0f / 60.0f, {}, player);
    player.playerHit = false;
    Wait(tutorial, player);
    success &= Check(tutorial.GetStep() == TutorialStep::Dodge,
        "shield hit advances to Dodge");
    player.dodgeStarted = true;
    tutorial.Update(1.0f / 60.0f, {}, player);
    player.dodgeStarted = false;
    Wait(tutorial, player);
    success &= Check(tutorial.IsComplete() &&
        tutorial.FilterInput(fullInput).moveX == 1.0f,
        "Dodge enters unrestricted free practice");
    tutorial.Start(player);
    tutorial.SkipToFreePractice();
    success &= Check(tutorial.IsComplete() && tutorial.IsCockpitConnected(),
        "Tutorial skip enters free practice");
    return success ? 0 : 1;
}
