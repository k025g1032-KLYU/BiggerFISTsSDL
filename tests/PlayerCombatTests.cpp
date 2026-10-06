#include "Collision/Collision.h"
#include "Data/PlayerCombatConfig.h"
#include "Game/FirstPersonGame.h"
#include "Gameplay/Player/PlayerVitality.h"
#include "Gameplay/SimulationTiming.h"
#include "Rendering/FirstPersonScenePresenter.h"

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <string>

namespace {
constexpr float kStep = static_cast<float>(SimulationTiming::kFixedStepSeconds);

bool Expect(bool value, const char* description) {
    if (!value) {
        std::printf("FAIL: %s\n", description);
    }
    return value;
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        std::puts("FAIL: expected player combat config path");
        return 1;
    }
    bool success = true;
    PlayerCombatConfig config;
    std::string error;
    success &= Expect(LoadPlayerCombatConfig(argv[1], config, error) &&
        config.maximumHitPoints == 5 && config.maximumShieldLayers == 3 &&
        std::abs(config.chargeTime - 0.55f) < 0.0001f,
        "Original player combat defaults load from the data file");
    success &= Expect(!LoadPlayerCombatConfig(std::filesystem::path(argv[1]).parent_path() /
        "missing-player-combat.cfg", config, error) &&
        error.find("missing-player-combat.cfg") != std::string::npos,
        "A missing config reports its filename");
    const std::filesystem::path invalidPath = std::filesystem::temp_directory_path() /
        "BiggerFISTsSDL-invalid-player-combat.cfg";
    {
        std::ofstream output(invalidPath);
        output << "chargeTime 0\n";
    }
    const bool acceptedZeroCharge = LoadPlayerCombatConfig(invalidPath, config, error);
    std::filesystem::remove(invalidPath);
    success &= Expect(!acceptedZeroCharge && error.find("chargeTime") != std::string::npos,
        "A zero charge duration is rejected with the key name");

    PlayerVitality vitality(config);
    success &= Expect(vitality.GetHitPoints() == 5 && vitality.GetShieldLayers() == 3,
        "Reset starts with five HP and three shield layers");
    const PlayerHitResult shieldHit = vitality.ReceiveHit(PlayerHitType::Projectile);
    success &= Expect(shieldHit.shieldLayersLost == 1 &&
        !shieldHit.didDamageHitPoints && vitality.GetShieldLayers() == 2,
        "Projectile consumes one shield layer");
    vitality.Update(2.0f);
    success &= Expect(vitality.GetShieldLayers() == 2,
        "Shield does not recharge early");
    vitality.Update(1.0f);
    success &= Expect(vitality.GetShieldLayers() == 3,
        "Shield recharges after three seconds");
    const PlayerHitResult chargeHit = vitality.ReceiveHit(PlayerHitType::ChargeUnit);
    success &= Expect(chargeHit.shieldLayersLost == 3 &&
        vitality.GetShieldLayers() == 0 && vitality.GetHitPoints() == 5,
        "Charge impact clears all shields without HP loss");
    const PlayerHitResult hitPointsHit = vitality.ReceiveHit(PlayerHitType::Projectile);
    success &= Expect(hitPointsHit.didDamageHitPoints &&
        hitPointsHit.hitPointsAfterHit == 4,
        "Unshielded projectile consumes one HP");
    vitality.Reset();
    const PlayerHitResult burstHit = vitality.ReceiveHit(PlayerHitType::BossSpreadBurst);
    success &= Expect(burstHit.shieldLayersLost == 3 &&
        vitality.GetHitPoints() == 5,
        "Spread burst uses the original all-shield damage rule");

    const Collision::SweepHit movingHit = Collision::SweepSphereAgainstMovingSphere(
        {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.5f,
        {0.0f, 0.0f, -10.0f}, {0.0f, 0.0f, 10.0f}, 0.5f);
    success &= Expect(movingHit.didHit && movingHit.time > 0.0f &&
        movingHit.time < 1.0f,
        "Relative sweep detects a fast target crossing a stationary fist");
    success &= Expect(!Collision::SweepSphereAgainstMovingSphere(
        {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.5f,
        {5.0f, 0.0f, -10.0f}, {5.0f, 0.0f, 10.0f}, 0.5f).didHit,
        "Relative sweep rejects a distant target");

    PlayerMovement movement(0.05f, config);
    movement.StartChargeImpact({0.0f, 0.0f, 1.0f}, true);
    movement.Update(kStep, 0.0f, -1.0f, 0.0f);
    success &= Expect(movement.GetImpactState() == PlayerImpactState::Knockback &&
        movement.GetVelocity().z > 0.0f,
        "Shielded charge impact overrides normal movement");
    movement.Reset({0.0f, 1.7f, 0.0f});
    movement.StartChargeImpact({0.0f, 0.0f, 1.0f}, false);
    movement.Update(kStep, 0.0f, -1.0f, 0.0f);
    success &= Expect(movement.GetImpactState() == PlayerImpactState::Holding &&
        std::abs(movement.GetVelocity().z) < 0.0001f,
        "Unshielded charge holds before knockback");
    for (int frame = 0; frame < 6; ++frame) {
        movement.Update(kStep, 0.0f, -1.0f, 0.0f);
    }
    success &= Expect(movement.GetImpactState() == PlayerImpactState::Knockback,
        "Unshielded charge enters knockback after the hold");
    movement.StartBurstImpact({1.0f, 0.0f, 0.0f});
    success &= Expect(movement.GetVelocity().x > 0.0f &&
        movement.GetImpactState() == PlayerImpactState::Knockback,
        "Spread burst applies its own knockback");
    movement.StartBurstImpact({std::numeric_limits<float>::infinity(), 0.0f, 0.0f});
    success &= Expect(std::isfinite(movement.GetVelocity().x) &&
        std::isfinite(movement.GetVelocity().z),
        "Invalid impact directions fall back to the player heading");
    movement.Reset({0.0f, 1.7f, -7.8f});
    movement.SetResearchInstituteCollisionEnabled(true);
    movement.Update(kStep, 0.0f, 0.0f, 0.0f);
    success &= Expect(movement.GetPosition().z >= -7.55f - 0.0001f,
        "Player cannot enter the original Research Institute collision circle");

    PunchArm left(PunchArmSide::Left, config);
    PunchArm right(PunchArmSide::Right, config);
    const Vector3 origin{0.0f, 1.7f, 0.0f};
    left.Reset(origin, 0.0f);
    right.Reset(origin, 0.0f);
    left.Update(kStep, origin, 0.0f, {true, false});
    right.Update(kStep, origin, 0.0f, {false, true});
    left.Update(kStep, origin, 0.0f, {});
    success &= Expect(left.GetState() == PunchArmState::Ready &&
        right.GetState() == PunchArmState::Charging,
        "One arm can cancel while the other continues charging");
    left.Update(kStep, origin, 0.0f, {true, false}, false);
    success &= Expect(left.GetState() == PunchArmState::Ready,
        "Impact prevents a new charge without interrupting the other arm");
    PlayerCombatConfig fastCharge = config;
    fastCharge.chargeTime = 0.1f;
    PunchArm configurable(PunchArmSide::Left, fastCharge);
    configurable.Reset(origin, 0.0f);
    configurable.Update(kStep, origin, 0.0f, {true, false});
    for (int frame = 0; frame < 7; ++frame) {
        configurable.Update(kStep, origin, 0.0f, {true, false});
    }
    const Vector3 visualShake = configurable.GetChargedVisualOffset();
    success &= Expect(configurable.GetState() == PunchArmState::Charged &&
        std::hypot(visualShake.x, visualShake.y, visualShake.z) > 0.0f,
        "Injected combat data changes charge timing and charged visual feedback");

    FirstPersonCamera bobCamera(config);
    bobCamera.Reset({0.0f, 0.0f, 0.0f});
    float largestBob = 0.0f;
    for (int frame = 0; frame < 120; ++frame) {
        bobCamera.Follow({0.0f, 0.0f, 0.0f}, kStep, 4.8f);
        largestBob = std::max(largestBob,
            std::hypot(bobCamera.GetPosition().x, bobCamera.GetPosition().y));
    }
    success &= Expect(largestBob > 0.02f,
        "Moving at normal speed adds visible step bob");
    for (int frame = 0; frame < 90; ++frame) {
        bobCamera.Follow({0.0f, 0.0f, 0.0f}, kStep, 0.0f);
    }
    success &= Expect(std::hypot(bobCamera.GetPosition().x,
        bobCamera.GetPosition().y) < 0.0001f,
        "Step bob fades when movement stops");

    FirstPersonGame game(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{{"target", {0.0f, 4.55f, 4.5f}, 3}}, config);
    GameInput punch{};
    punch.leftPunchKeyDown = true;
    for (int frame = 0; frame < 35; ++frame) {
        game.Update(kStep, punch, 0.0f, 0.0f);
    }
    game.Update(kStep, {}, 0.0f, 0.0f);
    success &= Expect(game.GetEvents().leftPunchBegan,
        "A full charge emits a Punch start event");
    for (int frame = 0; frame < 20; ++frame) {
        game.Update(kStep, {}, 0.0f, 0.0f);
    }
    success &= Expect(game.GetTargets().front().hitPoints == 2,
        "A long Punch window damages a target only once");
    GameInput projectile{};
    projectile.testProjectileHit.pressed = true;
    game.Update(kStep, projectile, 0.0f, 0.0f);
    success &= Expect(game.GetEvents().playerHit &&
        game.GetVitality().GetShieldLayers() == 2 &&
        game.GetCamera().GetShakeTimeRemaining() > 0.0f,
        "The test input applies shield damage and camera shake");
    GameInput reset{};
    reset.resetTarget.pressed = true;
    game.Update(kStep, reset, 0.0f, 0.0f);
    success &= Expect(game.GetTargets().front().hitPoints == 3 &&
        game.GetVitality().GetShieldLayers() == 3 &&
        game.GetVitality().GetHitPoints() == 5 &&
        game.GetLeftArm().GetState() == PunchArmState::Ready,
        "R restores Targets, player vitality and arm state");

    const PlayerHitResult finalShieldHit = game.ReceivePlayerHit(
        PlayerHitType::BossSpreadBurst);
    success &= Expect(finalShieldHit.shieldLayersLost == 3 &&
        game.GetPlayer().GetImpactState() == PlayerImpactState::Knockback,
        "Spread burst clears shields and starts knockback");
    for (int hit = 0; hit < 5; ++hit) {
        game.ReceivePlayerHit(PlayerHitType::Projectile);
    }
    success &= Expect(game.GetVitality().IsDefeated() &&
        game.GetEvents().playerDefeated,
        "Zero HP exposes defeat without requiring a Game Over scene");
    game.Update(kStep, reset, 0.0f, 0.0f);

    FirstPersonGame movingGame(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{{"target", {-1.55f, 4.55f, -10.0f}, 2}}, config);
    for (int frame = 0; frame < 35; ++frame) {
        movingGame.Update(kStep, punch, 0.0f, 0.0f);
    }
    movingGame.Update(kStep, {}, 0.0f, 0.0f);
    movingGame.MoveTarget("target", {-1.55f, 4.55f, 0.0f});
    movingGame.MoveTarget("target", {-1.55f, 4.55f, 10.0f});
    movingGame.Update(kStep, {}, 0.0f, 0.0f);
    success &= Expect(movingGame.GetTargets().front().hitPoints == 1,
        "A target crossing the fist between fixed steps is hit once");
    for (int frame = 0; frame < 20; ++frame) {
        movingGame.Update(kStep, {}, 0.0f, 0.0f);
    }
    success &= Expect(movingGame.GetTargets().front().hitPoints == 1,
        "A target cannot take repeated damage from one Punch");

    FirstPersonGame rocketGame(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{{"target", {0.0f, 4.55f, 8.0f}, 3}}, config);
    GameInput rocket{};
    rocket.leftMouseDown = true;
    for (int frame = 0; frame < 35; ++frame) {
        rocketGame.Update(kStep, rocket, 0.0f, 0.0f);
    }
    rocketGame.Update(kStep, {}, 0.0f, 0.0f);
    for (int frame = 0; frame < 20; ++frame) {
        rocketGame.Update(kStep, {}, 0.0f, 0.0f);
    }
    success &= Expect(rocketGame.GetTargets().front().hitPoints == 2,
        "Rocket Punch penetrates without repeatedly damaging one target");

    ModelScene scene;
    scene.models.clear();
    scene.models.push_back({});
    scene.models.back().id = "left_upper_arm";
    scene.models.push_back({});
    scene.models.back().id = "right_upper_arm";
    FirstPersonScenePresenter presenter(scene);
    presenter.Sync(game);
    const ModelInstance* upperArm = FindModelById(presenter.GetScene(), "left_upper_arm");
    success &= Expect(upperArm != nullptr && std::isfinite(upperArm->position.x) &&
        std::isfinite(upperArm->initialXDegrees) && upperArm->scale > 0.0f,
        "Presenter supplies a valid original upper-arm pose");

    if (success) {
        std::puts("PASS: player combat config, damage, moving collision, arms and presentation");
    }
    return success ? 0 : 1;
}
