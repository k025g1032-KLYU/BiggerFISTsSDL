#include "Game/FirstPersonGame.h"
#include "Gameplay/Player/FirstPersonTuning.h"
#include "Gameplay/SimulationTiming.h"
#include "Rendering/FirstPersonScenePresenter.h"

#include <cmath>
#include <cstdio>
#include <numbers>

namespace {
constexpr float kStep = static_cast<float>(SimulationTiming::kFixedStepSeconds);
constexpr Vector3 kPlayerPosition{0.0f, 1.7f, 0.0f};

bool Expect(bool condition, const char* description) {
    if (!condition) {
        std::printf("FAIL: %s\n", description);
    }
    return condition;
}

void ChargeRocket(PunchArm& arm, const Vector3& playerPosition, float yaw) {
    const PunchArmInput rocket{false, true};
    arm.Update(kStep, playerPosition, yaw, rocket);
    for (int frame = 0; frame < 34; ++frame) {
        arm.Update(kStep, playerPosition, yaw, rocket);
    }
}
}

int main() {
    bool success = true;
    PunchArm left(PunchArmSide::Left);
    left.Reset(kPlayerPosition, 0.0f);
    success &= Expect(left.GetRocketFlameScale() == 0.0f,
        "A ready fist has no rocket flame");
    left.Update(kStep, kPlayerPosition, 0.0f, {false, true});
    success &= Expect(left.GetState() == PunchArmState::Charging &&
        left.GetAttackType() == PunchArmAttackType::RocketPunch,
        "An unshifted mouse input selects Rocket Punch");
    left.Update(kStep, kPlayerPosition, 0.0f, {});
    success &= Expect(left.GetState() == PunchArmState::Ready &&
        left.GetAttackType() == PunchArmAttackType::None,
        "Early release cancels Rocket Punch");

    ChargeRocket(left, kPlayerPosition, 0.0f);
    success &= Expect(left.GetState() == PunchArmState::Charged &&
        std::abs(left.GetFistPosition().z -
            FirstPersonTuning::kRocketChargeFinalForwardOffset) < 0.0001f,
        "A fully charged fist reaches the original rocket dock position");
    left.Update(kStep, kPlayerPosition, 0.0f, {});
    success &= Expect(left.GetState() == PunchArmState::RocketFlying &&
        left.DidLaunchRocketThisFrame() && left.IsFistAway() &&
        !left.IsRocketDamageActiveThisFrame() &&
        std::abs(left.GetRocketFlameScale() - 1.0f) < 0.0001f,
        "Release commits the attack and launches the entire fist");

    const float launchZ = left.GetFistPosition().z;
    left.Update(kStep, kPlayerPosition, 0.0f, {});
    success &= Expect(left.IsRocketDamageActiveThisFrame() &&
        std::abs(left.GetFistPosition().z - launchZ -
            FirstPersonTuning::kRocketSpeed * kStep) < 0.001f,
        "Flight uses the original rocket speed and damage window");

    PunchArm right(PunchArmSide::Right);
    right.Reset(kPlayerPosition, 0.0f);
    right.Update(kStep, kPlayerPosition, 0.0f, {true, false});
    success &= Expect(left.IsFistAway() && right.GetState() == PunchArmState::Charging,
        "The opposite arm can charge while the rocket fist is away");

    bool reachedReturn = false;
    for (int frame = 0; frame < 80 && !reachedReturn; ++frame) {
        left.Update(kStep, kPlayerPosition, 0.0f, {});
        reachedReturn = left.GetState() == PunchArmState::Returning;
    }
    success &= Expect(reachedReturn &&
        left.GetRocketTravelDistance() <= left.GetRocketMaximumRange() + 0.001f &&
        left.GetFistPosition().z <= FirstPersonTuning::kBattlefieldMaximumZ +
            FirstPersonTuning::kRocketBattlefieldExitMargin + 0.001f &&
        std::abs(left.GetRocketFlameScale() - 0.15f) < 0.0001f,
        "The rocket reaches its clipped battlefield range and begins returning");

    bool reconnected = false;
    for (int frame = 0; frame < 240 && !reconnected; ++frame) {
        left.Update(kStep, kPlayerPosition, 0.0f, {});
        reconnected = left.DidReconnectThisFrame();
    }
    success &= Expect(reconnected && left.GetState() == PunchArmState::Recovery &&
        !left.IsFistAway() &&
        left.GetRocketFlameScale() == 0.0f &&
        std::abs(left.GetFistPosition().z -
            FirstPersonTuning::kReadyFistForwardOffset) < 0.001f,
        "The rocket ascends, returns, docks and enters Recovery");
    for (int frame = 0; frame < 20; ++frame) {
        left.Update(kStep, kPlayerPosition, 0.0f, {});
    }
    left.Update(kStep, kPlayerPosition, 0.0f, {true, false});
    success &= Expect(left.GetState() == PunchArmState::Charging &&
        left.GetAttackType() == PunchArmAttackType::Punch,
        "The arm can Punch again after reconnecting and recovering");

    PunchArm towardBase(PunchArmSide::Left);
    towardBase.Reset(kPlayerPosition, std::numbers::pi_v<float>);
    ChargeRocket(towardBase, kPlayerPosition, std::numbers::pi_v<float>);
    towardBase.Update(kStep, kPlayerPosition, std::numbers::pi_v<float>, {});
    bool evacuated = false;
    for (int frame = 0; frame < 20 && !evacuated; ++frame) {
        towardBase.Update(kStep, kPlayerPosition, std::numbers::pi_v<float>, {});
        evacuated = towardBase.GetState() == PunchArmState::RocketEvacuating;
    }
    success &= Expect(evacuated && !towardBase.IsRocketDamageActiveThisFrame(),
        "A rocket aimed toward the base begins a safe evacuation return");

    FirstPersonGame game(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{{"target", {0.0f, 4.55f, 8.0f}, 1},
            {"target_far", {0.0f, 4.55f, 12.0f}, 1}});
    ModelScene scene;
    scene.models.front().id = "target";
    scene.models.front().position = {0.0f, 4.55f, 8.0f};
    scene.models.push_back({});
    scene.models.back().id = "target_far";
    scene.models.back().position = {0.0f, 4.55f, 12.0f};
    FirstPersonScenePresenter view(scene);
    GameInput mouse{};
    mouse.leftMouseDown = true;
    for (int frame = 0; frame < 20; ++frame) {
        game.Update(kStep, mouse, 0.0f, 0.0f);
    }
    mouse.shiftDown = true;
    for (int frame = 20; frame < 35; ++frame) {
        game.Update(kStep, mouse, 0.0f, 0.0f);
    }
    success &= Expect(game.GetLeftArm().GetState() == PunchArmState::Charged &&
        game.GetLeftArm().GetAttackType() == PunchArmAttackType::RocketPunch,
        "Changing Shift during a mouse hold keeps the selected Rocket Punch");
    game.Update(kStep, {}, 0.0f, 0.0f);
    success &= Expect(game.GetEvents().leftRocketLaunched &&
        game.GetLeftArm().GetState() == PunchArmState::RocketFlying &&
        game.GetCamera().GetShakeTimeRemaining() > 0.0f,
        "A left Rocket Punch launch triggers the lighter camera shake");
    const float launchShakeRemaining = game.GetCamera().GetShakeTimeRemaining();
    GameInput rightPunch{};
    rightPunch.rightPunchKeyDown = true;
    game.Update(kStep, rightPunch, 0.0f, 0.0f);
    game.Update(kStep, rightPunch, 0.0f, 0.0f);
    success &= Expect(game.GetEvents().rocketHit &&
        !game.GetEvents().punchHit &&
        game.GetCamera().GetShakeTimeRemaining() < launchShakeRemaining,
        "Rocket impact is reported without restarting the launch shake");
    for (int frame = 2; frame < 5; ++frame) {
        game.Update(kStep, rightPunch, 0.0f, 0.0f);
    }
    view.Sync(game);
    success &= Expect(game.GetTargets()[0].hitPoints == 0 &&
        game.GetTargets()[1].hitPoints == 0 &&
        game.GetLeftArm().GetState() == PunchArmState::RocketFlying &&
        game.GetRightArm().GetState() == PunchArmState::Charging &&
        FindModelById(view.GetScene(), "target") == nullptr &&
        FindModelById(view.GetScene(), "target_far") == nullptr,
        "A swept Rocket Punch destroys two Targets without stopping or locking the other arm");
    GameInput reset{};
    reset.resetTarget.pressed = true;
    game.Update(kStep, reset, 0.0f, 0.0f);
    view.Sync(game);
    success &= Expect(game.GetTargets()[0].hitPoints == 1 &&
        game.GetTargets()[1].hitPoints == 1 &&
        game.GetLeftArm().GetState() == PunchArmState::Ready &&
        game.GetRightArm().GetState() == PunchArmState::Ready &&
        FindModelById(view.GetScene(), "target") != nullptr &&
        FindModelById(view.GetScene(), "target_far") != nullptr,
        "R resets a flying fist, the other arm and both Targets");

    GameInput rightRocket{};
    rightRocket.rightMouseDown = true;
    for (int frame = 0; frame < 35; ++frame) {
        game.Update(kStep, rightRocket, 0.0f, 0.0f);
    }
    success &= Expect(game.GetRightArm().GetState() == PunchArmState::Charged &&
        game.GetRightArm().GetAttackType() == PunchArmAttackType::RocketPunch,
        "The right mouse button charges the right Rocket Punch");
    game.Update(kStep, {}, 0.0f, 0.0f);
    success &= Expect(game.GetEvents().rightRocketLaunched &&
        game.GetRightArm().GetState() == PunchArmState::RocketFlying,
        "Releasing the right mouse button launches the right Rocket Punch");

    if (success) {
        std::puts("PASS: Rocket Punch charge, flight, hit, return and reset");
    }
    return success ? 0 : 1;
}
