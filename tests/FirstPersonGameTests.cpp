#include "Game/FirstPersonGame.h"
#include "Gameplay/Player/FirstPersonTuning.h"

#include <cmath>
#include <cstdio>
#include <numbers>

namespace {
bool Expect(bool condition, const char* description) {
    if (!condition) {
        std::printf("FAIL: %s\n", description);
    }
    return condition;
}

bool Near(float a, float b, float tolerance = 0.0001f) {
    return std::abs(a - b) < tolerance;
}
}

int main() {
    bool success = true;

    ModelScene scene;
    scene.models.front().id = "target";
    scene.models.front().position = { 0.0f, 4.55f, 8.0f };
    scene.models.front().initialXDegrees = 0.0f;
    scene.models.front().initialYDegrees = 0.0f;
    scene.models.front().rotationXDegreesPerSecond = 0.0f;
    scene.models.front().rotationYDegreesPerSecond = 0.0f;
    scene.models.push_back({});
    scene.models.back().id = "left_fist";
    scene.models.back().meshIndex = 1;
    scene.models.back().initialXDegrees = 0.0f;
    scene.models.back().initialYDegrees = 0.0f;
    scene.models.back().initialZDegrees = 90.0f;
    scene.models.back().rotationXDegreesPerSecond = 0.0f;
    scene.models.back().rotationYDegreesPerSecond = 0.0f;
    scene.models.push_back({});
    scene.models.back().id = "right_fist";
    scene.models.back().meshIndex = 2;
    scene.models.back().initialXDegrees = 0.0f;
    scene.models.back().initialYDegrees = 0.0f;
    scene.models.back().initialZDegrees = -90.0f;
    scene.models.back().rotationXDegreesPerSecond = 0.0f;
    scene.models.back().rotationYDegreesPerSecond = 0.0f;
    scene.cameraPosition = { 0.0f, 5.85f, 0.0f };

    FirstPersonGame game(GameConfig{}, scene);
    const float halfHorizontalDistance = FirstPersonTuning::kReadyFistForwardOffset * 0.5f;
    const float expectedElbowDrop = std::sqrt(
        FirstPersonTuning::kUpperArmFixedLength * FirstPersonTuning::kUpperArmFixedLength -
        halfHorizontalDistance * halfHorizontalDistance);
    const float expectedFistPitch = -std::atan2(expectedElbowDrop, halfHorizontalDistance) *
        180.0f / std::numbers::pi_v<float>;
    success &= Expect(Near(game.GetPlayer().GetPosition().y, 1.7f) &&
        Near(game.GetScene().cameraPosition.y, 5.85f) &&
        Near(game.GetScene().cameraTarget.z, 1.0f),
        "Player starts at original root height and camera looks forward");
    const ModelInstance* initialFist = FindModelById(game.GetScene(), "left_fist");
    success &= Expect(initialFist != nullptr &&
        Near(initialFist->position.x, -FirstPersonTuning::kShoulderHalfWidth) &&
        Near(initialFist->position.y, 1.7f + FirstPersonTuning::kShoulderHeight) &&
        Near(initialFist->position.z, FirstPersonTuning::kReadyFistForwardOffset) &&
        Near(initialFist->initialXDegrees, expectedFistPitch) &&
        Near(initialFist->initialZDegrees, 90.0f),
        "Idle left fist starts at the original shoulder-relative pose");
    const ModelInstance* initialRightFist = FindModelById(game.GetScene(), "right_fist");
    success &= Expect(initialRightFist != nullptr &&
        Near(initialRightFist->position.x, FirstPersonTuning::kShoulderHalfWidth) &&
        Near(initialRightFist->position.y, 1.7f + FirstPersonTuning::kShoulderHeight) &&
        Near(initialRightFist->position.z, FirstPersonTuning::kReadyFistForwardOffset) &&
        Near(initialRightFist->initialXDegrees, expectedFistPitch) &&
        Near(initialRightFist->initialZDegrees, -90.0f),
        "Idle right fist starts at the mirrored original shoulder-relative pose");
    Matrix4x4 initialTransform{};
    success &= Expect(TryBuildModelTransform(game.GetScene(), 1280, 720, initialTransform),
        "Initial Target has a valid render transform");
    const float initialW = initialTransform.elements[3][3];
    success &= Expect(initialW > 0.0f &&
        std::abs(initialTransform.elements[3][0] / initialW) < 1.0f &&
        std::abs(initialTransform.elements[3][1] / initialW) < 1.0f,
        "The Target starts inside the camera view");

    GameInput input{};
    input.moveY = -1.0f;
    game.Update(0.05, input, 0.0f, 0.0f);
    success &= Expect(Near(game.GetPlayer().GetVelocity().z, 0.3f) &&
        Near(game.GetPlayer().GetPosition().z, 0.015f),
        "W accelerates toward +Z using the original movement values");
    success &= Expect(game.GetScene().cameraPosition.z > 0.0f &&
        game.GetScene().cameraPosition.z < game.GetPlayer().GetPosition().z,
        "Camera follows the moving player with a small lag");
    const ModelInstance* movedFist = FindModelById(game.GetScene(), "left_fist");
    success &= Expect(movedFist != nullptr &&
        Near(movedFist->position.z,
            game.GetPlayer().GetPosition().z + FirstPersonTuning::kReadyFistForwardOffset),
        "Idle fist follows player translation");
    const ModelInstance* movedRightFist = FindModelById(game.GetScene(), "right_fist");
    success &= Expect(movedRightFist != nullptr &&
        Near(movedRightFist->position.z,
            game.GetPlayer().GetPosition().z + FirstPersonTuning::kReadyFistForwardOffset),
        "Both idle fists follow player translation");

    PlayerMovement rotated(0.05f);
    rotated.Update(0.05, 0.0f, 1.0f, std::numbers::pi_v<float> * 0.5f);
    success &= Expect(rotated.GetPosition().x > 0.0f && Near(rotated.GetPosition().z, 0.0f),
        "Forward movement follows camera yaw");
    PlayerMovement capped(0.05f);
    capped.Update(1.0, 0.0f, 1.0f, 0.0f);
    success &= Expect(Near(capped.GetPosition().z, 0.015f),
        "Long frames cap movement time");
    PlayerMovement diagonal(0.05f);
    for (int i = 0; i < 20; ++i) {
        diagonal.Update(0.05, 1.0f, 1.0f, 0.0f);
    }
    success &= Expect(Near(diagonal.GetVelocity().x, diagonal.GetVelocity().z) &&
        Near(std::hypot(diagonal.GetVelocity().x, diagonal.GetVelocity().z),
            FirstPersonTuning::kMaximumMoveSpeed),
        "Diagonal movement is normalized");

    FirstPersonCamera camera;
    camera.Reset({ 0.0f, 5.85f, 0.0f });
    camera.UpdateLook(0.0f, 10000.0f);
    success &= Expect(Near(camera.GetPitch(), FirstPersonTuning::kMaximumPitch) &&
        camera.GetTarget().y < camera.GetPosition().y,
        "Mouse Y looks down and pitch is clamped");

    const ModelInstance targetBefore = game.GetScene().models.front();
    game.Update(0.25, input, 100.0f, -50.0f);
    const ModelInstance& targetAfter = game.GetScene().models.front();
    success &= Expect(targetAfter.id == "target" &&
        Near(targetAfter.position.x, targetBefore.position.x) &&
        Near(targetAfter.position.y, targetBefore.position.y) &&
        Near(targetAfter.position.z, targetBefore.position.z) &&
        Near(targetAfter.rotationYDegreesPerSecond, 0.0f),
        "The Target remains static while the player moves and looks around");
    const ModelInstance* turnedFist = FindModelById(game.GetScene(), "left_fist");
    const float bodyYaw = game.GetPlayer().GetBodyYaw();
    success &= Expect(turnedFist != nullptr && bodyYaw > 0.0f &&
        bodyYaw < game.GetCamera().GetYaw() &&
        Near(turnedFist->position.x,
            game.GetPlayer().GetPosition().x -
            std::cos(bodyYaw) * FirstPersonTuning::kShoulderHalfWidth +
            std::sin(bodyYaw) * FirstPersonTuning::kReadyFistForwardOffset) &&
        Near(turnedFist->initialYDegrees,
            bodyYaw * 180.0f / std::numbers::pi_v<float>) &&
        Near(turnedFist->initialXDegrees, expectedFistPitch),
        "Idle fist follows the body yaw while the body lags the camera");
    const ModelInstance* turnedRightFist = FindModelById(game.GetScene(), "right_fist");
    success &= Expect(turnedFist != nullptr && turnedRightFist != nullptr &&
        Near(turnedRightFist->position.x,
            game.GetPlayer().GetPosition().x +
            std::cos(bodyYaw) * FirstPersonTuning::kShoulderHalfWidth +
            std::sin(bodyYaw) * FirstPersonTuning::kReadyFistForwardOffset) &&
        Near(turnedRightFist->initialYDegrees, turnedFist->initialYDegrees) &&
        Near(turnedRightFist->initialXDegrees, expectedFistPitch) &&
        Near(turnedRightFist->initialZDegrees, -90.0f),
        "Right fist keeps its mirrored roll and follows the same body yaw");
    success &= Expect(std::abs(game.GetScene().elapsedSeconds - 0.30) < 0.0001,
        "Scene time advances by real time");
    Matrix4x4 transform{};
    success &= Expect(TryBuildModelTransform(game.GetScene(), 1280, 720, transform),
        "First-person camera and Target produce a valid render transform");

    FirstPersonGame punchGame(GameConfig{}, scene);
    GameInput leftPunch{};
    leftPunch.leftPunchKeyDown = true;
    punchGame.Update(0.01, leftPunch, 0.0f, 0.0f);
    const ModelInstance* chargingFist = FindModelById(punchGame.GetScene(), "left_fist");
    success &= Expect(punchGame.GetLeftArm().GetState() == PunchArmState::Charging &&
        punchGame.GetRightArm().GetState() == PunchArmState::Ready &&
        chargingFist != nullptr && Near(chargingFist->initialZDegrees, 0.0f),
        "Q starts only the left punch charge and removes its idle roll");
    punchGame.Update(0.20, leftPunch, 0.0f, 0.0f);
    success &= Expect(punchGame.GetLeftArm().GetChargeRatio() > 0.0f &&
        punchGame.GetLeftArm().GetChargeRatio() < 1.0f &&
        punchGame.GetLeftArm().GetFistPosition().z < FirstPersonTuning::kReadyFistForwardOffset,
        "Charging retracts the fist and caps a long frame");
    punchGame.Update(0.01, GameInput{}, 0.0f, 0.0f);
    success &= Expect(punchGame.GetLeftArm().GetState() == PunchArmState::Ready &&
        Near(punchGame.GetLeftArm().GetChargeRatio(), 0.0f) &&
        Near(punchGame.GetLeftArm().GetFistPosition().z,
            FirstPersonTuning::kReadyFistForwardOffset),
        "Releasing before full charge cancels the punch");

    GameInput bothPunches{};
    bothPunches.leftPunchKeyDown = true;
    bothPunches.rightPunchKeyDown = true;
    punchGame.Update(0.01, bothPunches, 0.0f, 0.0f);
    for (int frame = 0; frame < 12; ++frame) {
        punchGame.Update(0.05, bothPunches, 0.0f, 0.0f);
    }
    success &= Expect(punchGame.GetLeftArm().GetState() == PunchArmState::Charged &&
        punchGame.GetRightArm().GetState() == PunchArmState::Charged &&
        Near(punchGame.GetLeftArm().GetChargeRatio(), 1.0f),
        "Both arms can finish charging independently");
    GameInput rightOnly{};
    rightOnly.rightPunchKeyDown = true;
    punchGame.Update(0.01, rightOnly, 0.0f, 0.0f);
    success &= Expect(punchGame.GetLeftArm().GetState() == PunchArmState::Punching &&
        punchGame.GetRightArm().GetState() == PunchArmState::Charged,
        "Releasing one charged arm commits only that punch");
    punchGame.Update(0.05, rightOnly, 0.0f, 0.0f);
    const ModelInstance* punchingFist = FindModelById(punchGame.GetScene(), "left_fist");
    success &= Expect(punchingFist != nullptr &&
        punchingFist->position.x > -FirstPersonTuning::kShoulderHalfWidth &&
        punchingFist->position.z > FirstPersonTuning::kReadyFistForwardOffset &&
        Near(punchingFist->initialZDegrees, 0.0f) &&
        punchGame.GetRightArm().GetState() == PunchArmState::Charged,
        "The left punch extends toward the center while the right arm stays charged");
    Matrix4x4 punchingTransform{};
    success &= Expect(punchingFist != nullptr &&
        TryBuildModelTransform(punchGame.GetScene(), *punchingFist, 1280, 720,
            punchingTransform),
        "The moving fist produces a valid GPU transform");
    for (int frame = 0; frame < 5; ++frame) {
        punchGame.Update(0.05, rightOnly, 0.0f, 0.0f);
    }
    success &= Expect(punchGame.GetLeftArm().GetState() == PunchArmState::Recovery &&
        punchGame.GetRightArm().GetState() == PunchArmState::Charged,
        "The fist returns and recovers without interrupting the other arm");
    for (int frame = 0; frame < 7; ++frame) {
        punchGame.Update(0.05, rightOnly, 0.0f, 0.0f);
    }
    const ModelInstance* recoveredFist = FindModelById(punchGame.GetScene(), "left_fist");
    success &= Expect(punchGame.GetLeftArm().GetState() == PunchArmState::Ready &&
        punchGame.GetRightArm().GetState() == PunchArmState::Charged &&
        recoveredFist != nullptr && Near(recoveredFist->initialZDegrees, 90.0f),
        "The left arm becomes ready again while the right arm remains charged");
    punchGame.Update(0.01, GameInput{}, 0.0f, 0.0f);
    success &= Expect(punchGame.GetRightArm().GetState() == PunchArmState::Punching &&
        Near(punchGame.GetScene().models.front().position.z, 8.0f),
        "The right arm can punch later and the Target remains static");

    FirstPersonGame mousePunchGame(GameConfig{}, scene);
    GameInput mousePunch{};
    mousePunch.leftMouseDown = true;
    mousePunchGame.Update(0.01, mousePunch, 0.0f, 0.0f);
    success &= Expect(mousePunchGame.GetLeftArm().GetState() == PunchArmState::Ready,
        "An unshifted mouse click is reserved for Rocket Punch");
    mousePunch.shiftDown = true;
    mousePunchGame.Update(0.01, mousePunch, 0.0f, 0.0f);
    mousePunch.shiftDown = false;
    for (int frame = 0; frame < 12; ++frame) {
        mousePunchGame.Update(0.05, mousePunch, 0.0f, 0.0f);
    }
    success &= Expect(mousePunchGame.GetLeftArm().GetState() == PunchArmState::Charged,
        "Releasing Shift while holding the mouse keeps the selected punch type");
    mousePunch.leftMouseDown = false;
    mousePunchGame.Update(0.01, mousePunch, 0.0f, 0.0f);
    success &= Expect(mousePunchGame.GetLeftArm().GetState() == PunchArmState::Punching,
        "Releasing the mouse after full charge starts the punch");

    ModelScene hitScene = scene;
    hitScene.models.front().position = {0.0f, 4.55f, 5.8f};
    FirstPersonGame hitGame(GameConfig{}, hitScene);
    GameInput heldLeft{};
    heldLeft.leftPunchKeyDown = true;
    hitGame.Update(0.01, heldLeft, 0.0f, 0.0f);
    for (int frame = 0; frame < 12; ++frame) {
        hitGame.Update(0.05, heldLeft, 0.0f, 0.0f);
    }
    success &= Expect(hitGame.GetTargetHitPoints() == 1 &&
        FindModelById(hitGame.GetScene(), "target") != nullptr,
        "Charging beside a Target does not damage it");
    hitGame.Update(0.01, GameInput{}, 0.0f, 0.0f);
    success &= Expect(hitGame.GetTargetHitPoints() == 1 &&
        !hitGame.DidDestroyTargetThisFrame(),
        "Releasing a charged fist does not hit before the punch moves");
    hitGame.Update(0.05, GameInput{}, 0.0f, 0.0f);
    success &= Expect(hitGame.GetTargetHitPoints() == 1,
        "The Target stays alive until the fist reaches it");
    hitGame.Update(0.05, GameInput{}, 0.0f, 0.0f);
    success &= Expect(hitGame.GetTargetHitPoints() == 0 &&
        hitGame.DidDestroyTargetThisFrame() &&
        FindModelById(hitGame.GetScene(), "target") == nullptr &&
        hitGame.GetScene().models.size() == 2,
        "One charged punch destroys the one-HP Target and removes its mesh");
    hitGame.Update(0.05, GameInput{}, 0.0f, 0.0f);
    success &= Expect(!hitGame.DidDestroyTargetThisFrame() &&
        hitGame.GetTargetHitPoints() == 0,
        "A destroyed Target cannot be hit again on the next frame");

    GameInput resetTarget{};
    resetTarget.resetTarget.pressed = true;
    hitGame.Update(0.01, resetTarget, 0.0f, 0.0f);
    const ModelInstance* restoredTarget = FindModelById(hitGame.GetScene(), "target");
    success &= Expect(restoredTarget != nullptr &&
        Near(restoredTarget->position.z, 5.8f) &&
        hitGame.GetScene().models.size() == 3 &&
        hitGame.GetTargetHitPoints() == 1 &&
        hitGame.GetLeftArm().GetState() == PunchArmState::Ready &&
        hitGame.GetRightArm().GetState() == PunchArmState::Ready,
        "R restores the original Target position and readies both arms");
    GameInput heldRight{};
    heldRight.rightPunchKeyDown = true;
    hitGame.Update(0.01, heldRight, 0.0f, 0.0f);
    for (int frame = 0; frame < 12; ++frame) {
        hitGame.Update(0.05, heldRight, 0.0f, 0.0f);
    }
    hitGame.Update(0.01, GameInput{}, 0.0f, 0.0f);
    hitGame.Update(0.05, GameInput{}, 0.0f, 0.0f);
    hitGame.Update(0.05, GameInput{}, 0.0f, 0.0f);
    success &= Expect(hitGame.GetTargetHitPoints() == 0 &&
        hitGame.DidDestroyTargetThisFrame() &&
        hitGame.GetLeftArm().GetState() == PunchArmState::Ready,
        "The right arm can independently destroy the restored Target");

    ModelScene missScene = scene;
    missScene.models.front().position = {8.0f, 4.55f, 5.8f};
    FirstPersonGame missGame(GameConfig{}, missScene);
    missGame.Update(0.01, heldLeft, 0.0f, 0.0f);
    for (int frame = 0; frame < 12; ++frame) {
        missGame.Update(0.05, heldLeft, 0.0f, 0.0f);
    }
    missGame.Update(0.01, GameInput{}, 0.0f, 0.0f);
    for (int frame = 0; frame < 5; ++frame) {
        missGame.Update(0.05, GameInput{}, 0.0f, 0.0f);
    }
    success &= Expect(missGame.GetTargetHitPoints() == 1 &&
        FindModelById(missGame.GetScene(), "target") != nullptr,
        "A punch that misses leaves the Target intact");

    if (success) {
        std::puts("PASS: first-person movement, independent punches, Target damage and reset");
    }
    return success ? 0 : 1;
}
