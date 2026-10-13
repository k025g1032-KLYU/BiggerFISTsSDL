#include "Gameplay/Tutorial/TutorialEncounter.h"
#include "Gameplay/Combat/CombatSession.h"
#include "Gameplay/Player/FirstPersonTuning.h"

#include "Data/GameConfig.h"
#include "Platform/FixedStepClock.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr float kStep = 1.0f / 60.0f;

bool Check(bool condition, const char* name) {
    if (!condition) std::printf("FAIL: %s\n", name);
    return condition;
}

int ActiveTargets(const FirstPersonGame& player) {
    return static_cast<int>(std::count_if(player.GetTargets().begin(),
        player.GetTargets().end(), [](const FirstPersonTarget& target) {
            return target.id.starts_with("target_tutorial_") &&
                target.id != "target_tutorial_missile" && target.hitPoints > 0;
        }));
}

const FirstPersonTarget* FindTarget(const FirstPersonGame& player,
    std::string_view id) {
    const auto& targets = player.GetTargets();
    const auto found = std::find_if(targets.begin(), targets.end(),
        [id](const FirstPersonTarget& target) { return target.id == id; });
    return found == targets.end() ? nullptr : &*found;
}

void Wait(TutorialSession& tutorial, TutorialObservation& observation) {
    for (int tick = 0; tick < 80; ++tick)
        tutorial.Update(kStep, {}, observation);
}

void AdvanceToPunch(TutorialSession& tutorial,
    TutorialObservation& observation) {
    tutorial.Start(observation);
    Wait(tutorial, observation);
    observation.cameraYaw = 1.6f;
    GameInput connect{};
    connect.switchView.pressed = true;
    tutorial.Update(kStep, connect, observation);
    Wait(tutorial, observation);
    observation.cameraYaw = 2.2f;
    observation.playerPosition.z = 5.1f;
    tutorial.Update(kStep, {}, observation);
    Wait(tutorial, observation);
}

void AdvanceToRocket(TutorialSession& tutorial,
    TutorialObservation& observation) {
    observation.leftPunchHit = observation.rightPunchHit = true;
    tutorial.Update(kStep, {}, observation);
    observation.leftPunchHit = observation.rightPunchHit = false;
    Wait(tutorial, observation);
}

void AdvanceToDefense(TutorialSession& tutorial,
    TutorialObservation& observation) {
    observation.leftRocketHit = observation.rightRocketHit = true;
    tutorial.Update(kStep, {}, observation);
    observation.leftRocketHit = observation.rightRocketHit = false;
    Wait(tutorial, observation);
}

Vector3 DefenseMissileAtTwoSeconds(int renderedFps) {
    FirstPersonGame player(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    TutorialEncounter encounter;
    TutorialSession tutorial;
    TutorialObservation observation{};
    encounter.Start(player);
    AdvanceToPunch(tutorial, observation);
    AdvanceToRocket(tutorial, observation);
    AdvanceToDefense(tutorial, observation);
    encounter.EnterStep(TutorialStep::Defense, player);
    FixedStepClock clock;
    for (int frame = 0; frame < renderedFps * 2; ++frame) {
        const int steps = clock.Advance(1.0 / renderedFps);
        for (int index = 0; index < steps; ++index)
            encounter.UpdateBeforePlayer(kStep, tutorial, player);
    }
    const FirstPersonTarget* missile =
        FindTarget(player, "target_tutorial_missile");
    return missile ? missile->position : Vector3{};
}
}

int main() {
    bool pass = true;
    FirstPersonGame player(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    TutorialEncounter encounter;
    TutorialSession tutorial;
    TutorialObservation observation{};
    encounter.Start(player);
    pass &= Check(ActiveTargets(player) == 6,
        "Tutorial begins with six original targets");
    const float expectedX[]{-4.0f, 4.0f, -8.0f, 8.0f, -12.0f, 12.0f};
    const float expectedZ[]{8.0f, 8.0f, 16.0f, 16.0f, 24.0f, 24.0f};
    for (int index = 0; index < 6; ++index) {
        const auto* target = FindTarget(player,
            "target_tutorial_" + std::to_string(index));
        pass &= Check(target && std::abs(target->position.x - expectedX[index]) < 0.001f &&
            std::abs(target->position.y - 4.55f) < 0.001f &&
            std::abs(target->position.z - expectedZ[index]) < 0.001f &&
            std::abs(target->radius - 1.25f) < 0.001f,
            "Tutorial target position and radius match the original");
    }

    AdvanceToPunch(tutorial, observation);
    pass &= Check(tutorial.GetStep() == TutorialStep::Punch,
        "Tutorial reached Punch");
    encounter.EnterStep(tutorial.GetStep(), player);
    for (int index = 0; index < 6; ++index)
        player.RemoveTarget("target_tutorial_" + std::to_string(index));
    for (int tick = 0; tick < 47; ++tick)
        encounter.UpdateBeforePlayer(kStep, tutorial, player);
    pass &= Check(ActiveTargets(player) == 0,
        "Punch targets remain absent during the recovery delay");
    for (int tick = 0; tick < 2; ++tick)
        encounter.UpdateBeforePlayer(kStep, tutorial, player);
    pass &= Check(ActiveTargets(player) == 6,
        "Punch restores targets only to prevent an incomplete step stalling");

    AdvanceToRocket(tutorial, observation);
    pass &= Check(tutorial.GetStep() == TutorialStep::RocketPunch,
        "Tutorial reached Rocket Punch");
    encounter.EnterStep(tutorial.GetStep(), player);
    pass &= Check(ActiveTargets(player) == 6,
        "Rocket Punch resets all targets on step entry");
    player.RemoveTarget("target_tutorial_0");
    for (int tick = 0; tick < 60; ++tick)
        encounter.UpdateBeforePlayer(kStep, tutorial, player);
    pass &= Check(ActiveTargets(player) == 5,
        "Rocket Punch does not immediately restore an individual hit target");
    for (int index = 1; index < 6; ++index)
        player.RemoveTarget("target_tutorial_" + std::to_string(index));
    for (int tick = 0; tick < 47; ++tick)
        encounter.UpdateBeforePlayer(kStep, tutorial, player);
    pass &= Check(ActiveTargets(player) == 0,
        "Rocket Punch waits 0.8 seconds after all targets are gone");
    for (int tick = 0; tick < 2; ++tick)
        encounter.UpdateBeforePlayer(kStep, tutorial, player);
    pass &= Check(ActiveTargets(player) == 6,
        "Rocket Punch restores all targets after the original delay");
    GameInput chargeRocket{};
    chargeRocket.leftMouseDown = true;
    for (int tick = 0; tick < 40; ++tick)
        player.Update(kStep, chargeRocket, 0.0f, 0.0f);
    player.Update(kStep, {}, 0.0f, 0.0f);
    pass &= Check(player.GetLeftArm().IsFistAway(),
        "Rocket Punch can launch during tutorial");
    for (int index = 0; index < 6; ++index)
        player.RemoveTarget("target_tutorial_" + std::to_string(index));
    for (int tick = 0; tick < 60; ++tick)
        encounter.UpdateBeforePlayer(kStep, tutorial, player);
    pass &= Check(ActiveTargets(player) == 0,
        "Targets do not respawn while a Rocket fist is away");
    for (int tick = 0; tick < 500 && player.GetLeftArm().IsFistAway(); ++tick) {
        encounter.UpdateBeforePlayer(kStep, tutorial, player);
        player.Update(kStep, {}, 0.0f, 0.0f);
    }
    pass &= Check(!player.GetLeftArm().IsFistAway() &&
        ActiveTargets(player) == 0,
        "Rocket return does not instantly restore targets");
    for (int tick = 0; tick < 49; ++tick)
        encounter.UpdateBeforePlayer(kStep, tutorial, player);
    pass &= Check(ActiveTargets(player) == 6,
        "Rocket targets return after reconnection and the full delay");

    AdvanceToDefense(tutorial, observation);
    pass &= Check(tutorial.GetStep() == TutorialStep::Defense,
        "Tutorial reached Defense");
    encounter.EnterStep(tutorial.GetStep(), player);
    for (int tick = 0; tick < 59; ++tick)
        encounter.UpdateBeforePlayer(kStep, tutorial, player);
    pass &= Check(!encounter.HasTrainingMissile(),
        "Defense waits before the first training projectile");
    for (int tick = 0; tick < 2; ++tick)
        encounter.UpdateBeforePlayer(kStep, tutorial, player);
    const FirstPersonTarget* missile =
        FindTarget(player, "target_tutorial_missile");
    pass &= Check(encounter.HasTrainingMissile() && missile &&
        missile->displayKind == TargetDisplayKind::Projectile &&
        std::abs(missile->radius - 0.45f) < 0.001f &&
        std::abs(missile->position.y - 4.2f) < 0.001f &&
        std::abs(missile->position.z - 18.0f) < 0.001f,
        "Defense projectile has the original spawn and hitbox");
    encounter.UpdateBeforePlayer(kStep, tutorial, player);
    missile = FindTarget(player, "target_tutorial_missile");
    pass &= Check(missile &&
        std::abs(missile->position.z - (18.0f - 6.5f * kStep)) < 0.001f,
        "Defense projectile uses the original speed");
    bool contacted = false;
    for (int tick = 0; tick < 240 && !contacted; ++tick) {
        encounter.UpdateBeforePlayer(kStep, tutorial, player);
        player.Update(kStep, {}, 0.0f, 0.0f);
        contacted = encounter.ResolveAfterPlayer(player);
    }
    pass &= Check(contacted && !encounter.HasTrainingMissile(),
        "Training projectile contacts the player and is removed");
    pass &= Check(player.GetEvents().playerHits.size() == 1 &&
        player.GetEvents().playerHits.front().result.wasApplied &&
        player.GetEvents().playerHits.front().result.shieldLayersLost > 0,
        "Defense contact exposes one applied shield hit for the visual adapter");
    Vector3 shieldCenter = player.GetPlayer().GetPosition();
    shieldCenter.y += FirstPersonTuning::kPlayerColliderCenterHeight;
    const auto shieldVisual = player.GetEvents().playerHits.empty()
        ? std::nullopt : MakeShieldHitVisualEvent(
            player.GetEvents().playerHits.front(), shieldCenter);
    pass &= Check(shieldVisual &&
        shieldVisual->effect == BattleVisualEffect::ShieldHit &&
        shieldVisual->shieldImpact == ShieldImpactState::TwoOrMoreRemaining &&
        std::abs(shieldVisual->position.y - shieldCenter.y) < 0.001f,
        "Tutorial shield hit maps to the shared battle visual event");
    PlayerHitFeedback noShieldHit{};
    noShieldHit.result.wasApplied = true;
    pass &= Check(!MakeShieldHitVisualEvent(noShieldHit, shieldCenter),
        "A hit without shield loss does not create a shield effect");
    noShieldHit.result.shieldImpact = ShieldImpactState::Broken;
    pass &= Check(!MakeShieldHitVisualEvent(noShieldHit, shieldCenter),
        "A visual state without shield loss does not create an effect");
    noShieldHit.result.wasApplied = false;
    noShieldHit.result.shieldLayersLost = 1;
    pass &= Check(!MakeShieldHitVisualEvent(noShieldHit, shieldCenter),
        "An unapplied hit cannot create a shield effect");
    const auto projectileImpact = encounter.ConsumeProjectileImpact();
    pass &= Check(projectileImpact.has_value() &&
        projectileImpact->scale == 0.55f &&
        !encounter.ConsumeProjectileImpact().has_value(),
        "Training projectile creates one shield-scaled impact event");
    for (int tick = 0; tick < 71; ++tick)
        encounter.UpdateBeforePlayer(kStep, tutorial, player);
    pass &= Check(!encounter.HasTrainingMissile(),
        "Defense respawn timer starts only after the projectile is gone");
    for (int tick = 0; tick < 2; ++tick)
        encounter.UpdateBeforePlayer(kStep, tutorial, player);
    pass &= Check(encounter.HasTrainingMissile(),
        "Defense spawns the next projectile after 1.2 seconds");
    const Vector3 missile30 = DefenseMissileAtTwoSeconds(30);
    const Vector3 missile60 = DefenseMissileAtTwoSeconds(60);
    const Vector3 missile120 = DefenseMissileAtTwoSeconds(120);
    pass &= Check(missile30.z > 0.0f &&
        std::abs(missile30.x - missile60.x) < 0.001f &&
        std::abs(missile30.z - missile60.z) < 0.001f &&
        std::abs(missile60.z - missile120.z) < 0.001f,
        "Tutorial missile position is independent of rendered FPS");

    if (pass) std::puts("PASS: tutorial targets and defense encounter");
    return pass ? 0 : 1;
}
