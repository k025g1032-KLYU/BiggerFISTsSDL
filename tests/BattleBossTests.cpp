#include "Data/BattleCombatConfig.h"
#include "Gameplay/Combat/BattleBoss.h"
#include "Collision/Collision.h"
#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <numbers>
#include <vector>

namespace {
constexpr float kStep = 1.0f / 60.0f;

bool HasAttack(const std::vector<BossAttackEvent>& attacks, BossAttackType type) {
    return std::any_of(attacks.begin(), attacks.end(),
        [type](const BossAttackEvent& event) { return event.type == type; });
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 1;
    }
    BattleCombatConfig config;
    std::string error;
    if (!LoadBattleCombatConfig(std::filesystem::path(argv[1]), config, error)) {
        std::printf("FAIL: %s\n", error.c_str());
        return 1;
    }
    BattleBoss boss(config.boss);
    const Vector3 player{0.0f, 1.7f, 18.0f};
    const BossVisualPose initialPose = boss.GetVisualPose();
    if (std::abs(initialPose.rootRotation.x + 1.570796327f) > 0.001f ||
        boss.GetNeedlePose() != BossNeedlePose::ChargeForm ||
        std::abs(initialPose.cones[0].translation.x) > 0.001f ||
        std::abs(initialPose.coreRotation.x - 1.570796327f) > 0.001f ||
        std::abs(initialPose.spikes[0].translation.x -
            config.boss.crystalHaloRadius) > 0.001f ||
        std::abs(initialPose.spikes[0].translation.z -
            config.boss.crystalHaloBackOffset) > 0.001f) {
        std::puts("FAIL: original Boss entrance, cone or crystal local pose changed");
        return 1;
    }
    BattleBoss poseBoss(config.boss);
    poseBoss.Reset({0.0f, config.boss.hoverHeight, 34.5f});
    poseBoss.BeginBehavior(BossMode::Idle);
    const Vector3 distantPlayer{0.0f, 1.7f, 0.0f};
    const auto settlePose = [&] {
        for (int step = 0; step < 90; ++step)
            poseBoss.Update(kStep, distantPlayer);
    };
    const auto near = [](float actual, float expected) {
        return std::abs(actual - expected) < 0.001f;
    };
    const auto nearAngle = [](float actual, float expected) {
        return std::abs(std::remainder(actual - expected,
            2.0f * std::numbers::pi_v<float>)) < 0.001f;
    };
    settlePose();
    BossVisualPose calibratedPose = poseBoss.GetVisualPose();
    if (!nearAngle(calibratedPose.cones[0].rotation.y, -0.70f) ||
        !nearAngle(calibratedPose.cones[1].rotation.y, 0.70f) ||
        !nearAngle(calibratedPose.cones[2].rotation.x, -0.72f)) {
        std::puts("FAIL: original Boss Spread cone local rotations changed");
        return 1;
    }
    poseBoss.CyclePracticeNeedlePose();
    settlePose();
    calibratedPose = poseBoss.GetVisualPose();
    if (!near(calibratedPose.cones[0].translation.x, 2.10f) ||
        !near(calibratedPose.cones[1].translation.x, -2.10f) ||
        !near(calibratedPose.cones[2].translation.y, -2.10f) ||
        !near(calibratedPose.cones[0].translation.z, 2.50f) ||
        !nearAngle(calibratedPose.cones[0].rotation.y,
            std::numbers::pi_v<float> / 18.0f)) {
        std::puts("FAIL: original Boss Needle Strike local pose changed");
        return 1;
    }
    poseBoss.CyclePracticeNeedlePose();
    settlePose();
    calibratedPose = poseBoss.GetVisualPose();
    if (!nearAngle(calibratedPose.coreRotation.x,
            std::numbers::pi_v<float> * 0.5f) ||
        !near(calibratedPose.cones[0].translation.x, 0.0f) ||
        !nearAngle(calibratedPose.cones[1].rotation.y, 0.0f)) {
        std::puts("FAIL: original Boss Charge Form local pose changed");
        return 1;
    }
    poseBoss.CyclePracticeNeedlePose();
    settlePose();
    calibratedPose = poseBoss.GetVisualPose();
    if (!near(calibratedPose.cones[0].translation.x, 1.0f) ||
        !near(calibratedPose.cones[1].translation.x, -1.0f) ||
        !nearAngle(calibratedPose.cones[0].rotation.y,
            std::numbers::pi_v<float> - 0.70f) ||
        !nearAngle(calibratedPose.cones[1].rotation.y,
            std::numbers::pi_v<float> + 0.70f) ||
        !nearAngle(calibratedPose.coreRotation.x, 0.0f)) {
        std::puts("FAIL: original Boss Cruise local pose changed");
        return 1;
    }
    poseBoss.Reset({0.0f, config.boss.hoverHeight, 34.5f});
    for (int step = 0; step < 240; ++step)
        poseBoss.Update(kStep, distantPlayer);
    const float expectedHaloAngle = std::remainder(
        240.0f * kStep * config.boss.crystalHaloRotationSpeed,
        2.0f * std::numbers::pi_v<float>);
    if (!near(poseBoss.GetVisualPose().crystalAngle, expectedHaloAngle)) {
        std::puts("FAIL: Boss crystal halo did not wrap like the original");
        return 1;
    }
    BattleBoss visualBoss(config.boss);
    visualBoss.Reset({0.0f, config.boss.hoverHeight, 34.5f});
    visualBoss.BeginBehavior(BossMode::Idle);
    for (int step = 0; step < 60; ++step)
        visualBoss.Update(kStep, player);
    const float coreBeforeCharge = visualBoss.GetVisualPose().coreRotation.x;
    visualBoss.BeginBehavior(BossMode::Charge);
    visualBoss.Update(kStep, player);
    const float blendedCore = visualBoss.GetVisualPose().coreRotation.x;
    const float drawMidCore = visualBoss.GetInterpolatedVisualPose(0.5f)
        .coreRotation.x;
    const float expectedCore = coreBeforeCharge +
        (1.570796327f - coreBeforeCharge) *
            kStep * config.boss.poseBlendSpeed;
    if (blendedCore <= coreBeforeCharge ||
        blendedCore >= 1.570796327f ||
        std::abs(drawMidCore - (coreBeforeCharge + blendedCore) * 0.5f) >
            0.001f ||
        std::abs(blendedCore - expectedCore) > 0.001f) {
        std::puts("FAIL: Boss core skipped its fixed-step pose blend");
        return 1;
    }
    visualBoss.BeginBehavior(BossMode::Cruise);
    for (int step = 0; step < 20; ++step)
        visualBoss.Update(kStep, player);
    visualBoss.BeginBehavior(BossMode::SpreadCounter);
    const float spreadStartYaw = visualBoss.GetVisualPose().cones[0].rotation.y;
    for (int step = 0; step < 7; ++step) {
        visualBoss.Update(kStep, player);
    }
    const float spreadMidYaw = visualBoss.GetVisualPose().cones[0].rotation.y;
    const float shortestTurn = std::remainder(-0.7f - spreadStartYaw,
        2.0f * std::numbers::pi_v<float>);
    const float expectedMidYaw = spreadStartYaw + shortestTurn *
        (7.0f * kStep / config.boss.spreadWindupDuration);
    if (std::abs(spreadMidYaw - expectedMidYaw) > 0.001f ||
        std::abs(spreadMidYaw + 0.7f) < 0.1f) {
        std::puts("FAIL: Boss spread windup skipped its midpoint pose");
        return 1;
    }
    for (int step = 0; step < 70; ++step) {
        visualBoss.Update(kStep, player);
    }
    if (std::abs(visualBoss.GetVisualPose().spreadSpin -
            4.0f * std::numbers::pi_v<float>) > 0.001f) {
        std::puts("FAIL: Boss spread pose snapped back after its spin");
        return 1;
    }
    visualBoss.Reset({0.0f, config.boss.hoverHeight, 34.5f});
    if (std::abs(visualBoss.GetVisualPose().cones[0].rotation.y -
            initialPose.cones[0].rotation.y) > 0.001f ||
        std::abs(visualBoss.GetVisualPose().coreRotation.x -
            initialPose.coreRotation.x) > 0.001f) {
        std::puts("FAIL: Boss visual pose did not reset");
        return 1;
    }
    for (int step = 0; step < 180; ++step) {
        boss.Update(kStep, player);
    }
    if (boss.GetMode() != BossMode::Idle || boss.GetHitPoints() != 20) {
        std::puts("FAIL: Boss entrance did not finish with full core HP");
        return 1;
    }
    BattleBoss rangedBoss(config.boss);
    rangedBoss.Reset({0.0f, config.boss.hoverHeight, 34.5f});
    rangedBoss.BeginBehavior(BossMode::Idle);
    rangedBoss.Update(kStep, {0.0f, 1.7f, 0.0f});
    const BossVisualPose ringPose = rangedBoss.GetVisualPose();
    if (std::abs(ringPose.spikes[0].translation.y -
            std::sin(ringPose.crystalAngle) *
                config.boss.crystalHaloRadius) > 0.001f) {
        std::puts("FAIL: Boss crystal ring is not in its original local XY plane");
        return 1;
    }
    bool idleSpikeFired = false;
    for (int step = 0; step < 110; ++step) {
        idleSpikeFired |= HasAttack(rangedBoss.Update(kStep,
            {0.0f, 1.7f, 0.0f}), BossAttackType::CrystalSpike);
    }
    if (!idleSpikeFired) {
        std::puts("FAIL: Boss Idle did not launch a crystal spike");
        return 1;
    }
    BossCombatConfig cruiseConfig = config.boss;
    cruiseConfig.farBoxerWeight = 0.0f;
    cruiseConfig.farChargeWeight = 0.0f;
    cruiseConfig.farRangedWeight = 0.0f;
    cruiseConfig.farCruiseWeight = 1.0f;
    BattleBoss cruiseBoss(cruiseConfig);
    cruiseBoss.Reset({0.0f, cruiseConfig.hoverHeight, 34.5f});
    cruiseBoss.BeginBehavior(BossMode::Idle);
    bool enteredCruise = false;
    bool finishedCruise = false;
    bool movedAcrossField = false;
    for (int step = 0; step < 480; ++step) {
        cruiseBoss.Update(kStep, {0.0f, 1.7f, 0.0f});
        enteredCruise |= cruiseBoss.GetMode() == BossMode::Cruise;
        finishedCruise |= enteredCruise && cruiseBoss.GetMode() == BossMode::Idle;
        movedAcrossField |= cruiseBoss.GetPosition().x < -4.0f ||
            cruiseBoss.GetPosition().x > 4.0f;
    }
    if (!enteredCruise || !finishedCruise || !movedAcrossField) {
        std::puts("FAIL: weighted Cruise action did not relocate and return to Idle");
        return 1;
    }
    BossCombatConfig dodgeConfig = config.boss;
    dodgeConfig.idleDecisionInterval = 10.0f;
    BattleBoss dodgeBoss(dodgeConfig);
    dodgeBoss.Reset({0.0f, dodgeConfig.hoverHeight, 12.0f});
    dodgeBoss.BeginBehavior(BossMode::Idle);
    const Vector3 farPlayer{0.0f, 1.7f, 0.0f};
    BossPlayerThreat threat;
    threat.isRocketCharged[0] = true;
    for (int step = 0; step < 12; ++step) {
        dodgeBoss.Update(kStep, farPlayer, threat);
        if (dodgeBoss.DidBeginRocketDodgeThisStep()) {
            std::puts("FAIL: Boss dodged without a charged Rocket aim");
            return 1;
        }
    }
    threat.isChargedRocketAimedAtCore[0] = true;
    for (int step = 0; step < 4; ++step) {
        dodgeBoss.Update(kStep, farPlayer, threat);
    }
    threat.isChargedRocketAimedAtCore[0] = false;
    dodgeBoss.Update(kStep, farPlayer, threat);
    threat.isChargedRocketAimedAtCore[0] = true;
    bool beganDodge = false;
    for (int step = 0; step < 12; ++step) {
        dodgeBoss.Update(kStep, farPlayer, threat);
        beganDodge |= dodgeBoss.DidBeginRocketDodgeThisStep();
    }
    if (!beganDodge || !dodgeBoss.IsRocketDodging()) {
        std::puts("FAIL: Boss did not dodge a sustained charged Rocket aim");
        return 1;
    }
    const float dodgeStartX = dodgeBoss.GetPosition().x;
    for (int step = 0; step < 12; ++step) {
        dodgeBoss.Update(kStep, farPlayer, threat);
    }
    if (std::abs(dodgeBoss.GetPosition().x - dodgeStartX) < 0.2f) {
        std::puts("FAIL: Boss Rocket dodge did not move sideways");
        return 1;
    }
    for (int step = 0; step < 120; ++step) {
        dodgeBoss.Update(kStep, farPlayer, threat);
        if (dodgeBoss.DidBeginRocketDodgeThisStep()) {
            std::puts("FAIL: one charged Rocket caused repeated Boss dodges");
            return 1;
        }
    }
    threat.isRocketCharged[0] = false;
    threat.isChargedRocketAimedAtCore[0] = false;
    dodgeBoss.Update(kStep, farPlayer, threat);
    threat.isRocketCharged[0] = true;
    threat.isChargedRocketAimedAtCore[0] = true;
    bool dodgedNewCharge = false;
    for (int step = 0; step < 12; ++step) {
        dodgeBoss.Update(kStep, farPlayer, threat);
        dodgedNewCharge |= dodgeBoss.DidBeginRocketDodgeThisStep();
    }
    if (!dodgedNewCharge) {
        std::puts("FAIL: Boss did not react to a new charged Rocket");
        return 1;
    }
    BattleBoss incomingBoss(dodgeConfig);
    incomingBoss.Reset({0.0f, dodgeConfig.hoverHeight, 12.0f});
    incomingBoss.BeginBehavior(BossMode::Idle);
    BossPlayerThreat incoming;
    incoming.hasIncomingRocketPunch = true;
    incomingBoss.Update(kStep, player, incoming);
    if (!incomingBoss.DidBeginRocketDodgeThisStep()) {
        std::puts("FAIL: Boss did not react to an incoming Rocket Punch");
        return 1;
    }
    BattleBoss boxerDodge(dodgeConfig);
    boxerDodge.Reset({0.0f, dodgeConfig.hoverHeight, 12.0f});
    boxerDodge.BeginBehavior(BossMode::Boxer);
    threat.isRocketCharged[0] = true;
    for (int step = 0; step < 12; ++step) {
        boxerDodge.Update(kStep, player, threat);
    }
    if (!boxerDodge.IsRocketDodging()) {
        std::puts("FAIL: Boxer did not evade a sustained charged Rocket aim");
        return 1;
    }
    BattleBoss pressureBoss(config.boss);
    pressureBoss.Reset({0.0f, config.boss.hoverHeight, 20.0f});
    pressureBoss.BeginBehavior(BossMode::Idle);
    for (int step = 0; step < 30; ++step) {
        pressureBoss.Update(kStep, player);
    }
    if (pressureBoss.GetMode() != BossMode::SpreadCounter) {
        std::puts("FAIL: nearby player did not trigger Spread Counter");
        return 1;
    }
    boss.BeginBehavior(BossMode::Charge);
    bool chargeHit = false;
    for (int step = 0; step < 120; ++step) {
        chargeHit |= HasAttack(boss.Update(kStep, player),
            BossAttackType::ChargeImpact);
    }
    if (!chargeHit) {
        std::puts("FAIL: Boss charge did not register swept player contact");
        return 1;
    }
    BattleBoss chargeSequence(config.boss);
    chargeSequence.Reset({0.0f, config.boss.hoverHeight, 34.5f});
    chargeSequence.BeginBehavior(BossMode::Charge);
    const Vector3 chargeTarget{0.0f, player.y, 18.0f};
    std::array<std::uint64_t, 3> chargeIds{};
    int chargeIdCount = 0;
    int chargeLaunches = 0;
    bool sawPullback = false;
    bool chargeTipAligned = true;
    bool chargeBodyAvailable = true;
    for (int step = 0; step < 700; ++step) {
        const Vector3 before = chargeSequence.GetPosition();
        const auto events = chargeSequence.Update(kStep, chargeTarget);
        chargeLaunches += chargeSequence.DidBeginChargeThisStep() ? 1 : 0;
        if (step < 50 && chargeSequence.GetPosition().z > before.z + 0.01f) {
            sawPullback = true;
        }
        for (const BossAttackEvent& event : events) {
            if (event.type != BossAttackType::ChargeImpact) continue;
            if (chargeIdCount == 0 ||
                chargeIds[chargeIdCount - 1] != event.attackId) {
                if (chargeIdCount < 3) {
                    chargeIds[chargeIdCount++] = event.attackId;
                }
            }
            const Vector3 direction = chargeSequence.GetChargeDirection();
            const Vector3 current = chargeSequence.GetPosition();
            const Vector3 previous = chargeSequence.GetPreviousPosition();
            chargeTipAligned &=
                std::abs(event.radius - config.boss.chargeTipRadius) <
                    0.0001f &&
                std::hypot(event.position.x - current.x -
                        direction.x * config.boss.chargeTipOffset,
                    event.position.y - current.y,
                    event.position.z - current.z -
                        direction.z * config.boss.chargeTipOffset) < 0.002f &&
                std::hypot(event.previousPosition.x - previous.x -
                        direction.x * config.boss.chargeTipOffset,
                    event.previousPosition.y - previous.y,
                    event.previousPosition.z - previous.z -
                        direction.z * config.boss.chargeTipOffset) < 0.002f;
            chargeBodyAvailable &= HasAttack(events,
                BossAttackType::ChargeBodyContact);
        }
        if (chargeIdCount == 3 &&
            chargeSequence.GetMode() == BossMode::Idle) break;
    }
    if (!sawPullback || !chargeTipAligned || !chargeBodyAvailable ||
        chargeIdCount != config.boss.chargeSequenceCount ||
        chargeLaunches != config.boss.chargeSequenceCount ||
        chargeSequence.GetMode() != BossMode::Idle) {
        std::printf("FAIL: original Boss charge pullback, tip, body or repeat sequence pullback=%d tip=%d body=%d ids=%d mode=%d position=(%.2f,%.2f,%.2f)\n",
            sawPullback, chargeTipAligned, chargeBodyAvailable,
            chargeIdCount, static_cast<int>(chargeSequence.GetMode()),
            chargeSequence.GetPosition().x, chargeSequence.GetPosition().y,
            chargeSequence.GetPosition().z);
        return 1;
    }
    BattleBoss interruptedCharge(config.boss);
    interruptedCharge.Reset({0.0f, config.boss.hoverHeight, 34.5f});
    interruptedCharge.BeginBehavior(BossMode::Charge);
    bool registeredTip = false;
    for (int step = 0; step < 180 && !registeredTip; ++step) {
        for (const BossAttackEvent& event :
                interruptedCharge.Update(kStep, chargeTarget)) {
            if (event.type == BossAttackType::ChargeImpact) {
                registeredTip = interruptedCharge.RegisterChargeTipHit(
                    event.attackId);
                if (interruptedCharge.RegisterChargeTipHit(event.attackId)) {
                    std::puts("FAIL: repeated Boss charge tip hit registered twice");
                    return 1;
                }
            }
        }
    }
    for (int step = 0; step < 30; ++step) {
        interruptedCharge.Update(kStep, chargeTarget);
    }
    if (!registeredTip ||
        interruptedCharge.GetMode() != BossMode::Idle) {
        std::puts("FAIL: Boss tip hit did not end the charge sequence");
        return 1;
    }
    BattleBoss bodyOnlyCharge(config.boss);
    bodyOnlyCharge.Reset({0.0f, config.boss.hoverHeight, 34.5f});
    bodyOnlyCharge.BeginBehavior(BossMode::Charge);
    bool registeredBody = false;
    for (int step = 0; step < 120 && !registeredBody; ++step) {
        for (const BossAttackEvent& event :
                bodyOnlyCharge.Update(kStep, chargeTarget)) {
            if (event.type != BossAttackType::ChargeBodyContact) continue;
            registeredBody = bodyOnlyCharge.RegisterChargeBodyContact(
                event.attackId);
            if (bodyOnlyCharge.RegisterChargeBodyContact(event.attackId)) {
                std::puts("FAIL: repeated Boss body contact registered twice");
                return 1;
            }
        }
    }
    if (!registeredBody || bodyOnlyCharge.GetMode() != BossMode::Charge) {
        std::puts("FAIL: body push incorrectly ended Boss charge");
        return 1;
    }
    boss.Reset({0.0f, config.boss.hoverHeight, 20.0f});
    boss.BeginBehavior(BossMode::Boxer);
    bool boxerHit = false;
    std::uint64_t boxerAttackId = 0;
    int boxerWindowSteps = 0;
    bool sweptMovingPlayer = false;
    bool boxerWindupVisible = false;
    bool boxerRecoveryVisible = false;
    bool boxerTipMatchedPose = true;
    Vector3 firstBoxerWindupRoot{};
    float boxerWindupDisplacement = 0.0f;
    bool boxerCueBeforeHit = false;
    int boxerCueCount = 0;
    for (int step = 0; step < 180; ++step) {
        const bool hadBoxerHit = boxerHit;
        const auto events = boss.Update(kStep, player);
        if (boss.DidBeginStrikeThisStep()) {
            ++boxerCueCount;
            boxerCueBeforeHit |= !boxerHit && events.empty();
        }
        for (const BossAttackEvent& event : events) {
            if (event.type != BossAttackType::BoxerStrike) {
                continue;
            }
            boxerHit = true;
            if (boxerAttackId == 0) {
                boxerAttackId = event.attackId;
            }
            if (event.attackId == boxerAttackId) {
                ++boxerWindowSteps;
                const BossVisualPose strikePose = boss.GetVisualPose();
                const Vector3& local = strikePose.cones[0].rootTranslation;
                const float sine = std::sin(strikePose.rootRotation.y);
                const float cosine = std::cos(strikePose.rootRotation.y);
                const Vector3 root{
                    boss.GetPosition().x + local.x * cosine +
                        local.z * sine,
                    boss.GetPosition().y + local.y,
                    boss.GetPosition().z - local.x * sine +
                        local.z * cosine};
                const float strikeYaw = strikePose.rootRotation.y -
                    std::numbers::pi_v<float>;
                const Vector3 target{
                    boss.GetPosition().x + std::sin(strikeYaw) *
                        config.boss.boxerStrikeOffset,
                    boss.GetPosition().y,
                    boss.GetPosition().z + std::cos(strikeYaw) *
                        config.boss.boxerStrikeOffset};
                const Vector3 delta{target.x - root.x,
                    target.y - root.y, target.z - root.z};
                const float length = std::hypot(delta.x, delta.y, delta.z);
                const Vector3 expectedTip = length > 0.00001f
                    ? Vector3{root.x + delta.x / length *
                            config.boss.needleTipOffset,
                        root.y + delta.y / length *
                            config.boss.needleTipOffset,
                        root.z + delta.z / length *
                            config.boss.needleTipOffset}
                    : Vector3{root.x + std::sin(strikeYaw) *
                            config.boss.needleTipOffset,
                        root.y,
                        root.z + std::cos(strikeYaw) *
                            config.boss.needleTipOffset};
                const float tipDifference = std::hypot(
                    event.position.x - expectedTip.x,
                    event.position.y - expectedTip.y,
                    event.position.z - expectedTip.z);
                boxerTipMatchedPose &= tipDifference < 0.002f;
            }
            sweptMovingPlayer |= Collision::SweepSphereAgainstMovingSphere(
                event.previousPosition, event.position, event.radius,
                {0.0f, 3.5f, 18.0f}, {0.0f, 3.5f, 17.5f},
                FirstPersonTuning::kPlayerColliderRadius).didHit;
        }
        const BossVisualPose pose = boss.GetVisualPose();
        const bool hasStrikeRoot = std::any_of(pose.cones.begin(),
            pose.cones.end(), [](const BossPartPose& cone) {
                return std::abs(cone.rootTranslation.x) > 0.01f ||
                    std::abs(cone.rootTranslation.y) > 0.01f ||
                    std::abs(cone.rootTranslation.z) > 0.01f;
            });
        boxerWindupVisible |= !boxerHit && hasStrikeRoot;
        boxerRecoveryVisible |= hadBoxerHit && events.empty() &&
            hasStrikeRoot;
        if (!boxerHit && hasStrikeRoot) {
            for (const BossPartPose& cone : pose.cones) {
                const Vector3& root = cone.rootTranslation;
                if (std::abs(root.x) <= 0.01f &&
                    std::abs(root.y) <= 0.01f &&
                    std::abs(root.z) <= 0.01f) continue;
                if (firstBoxerWindupRoot.x == 0.0f &&
                    firstBoxerWindupRoot.y == 0.0f &&
                    firstBoxerWindupRoot.z == 0.0f) {
                    firstBoxerWindupRoot = root;
                }
                const float dx = root.x - firstBoxerWindupRoot.x;
                const float dy = root.y - firstBoxerWindupRoot.y;
                const float dz = root.z - firstBoxerWindupRoot.z;
                boxerWindupDisplacement = std::max(
                    boxerWindupDisplacement,
                    std::sqrt(dx * dx + dy * dy + dz * dz));
            }
        }
    }
    if (!boxerHit || boxerAttackId == 0 || boxerWindowSteps < 2 ||
        boxerWindowSteps > 7 || !sweptMovingPlayer ||
        !boxerWindupVisible || !boxerRecoveryVisible ||
        !boxerTipMatchedPose || !boxerCueBeforeHit || boxerCueCount == 0 ||
        boxerWindupDisplacement < 2.5f) {
        std::printf("FAIL: Boxer collision or windup/recovery pose hit=%d id=%llu steps=%d sweep=%d windup=%d recovery=%d tip=%d displacement=%.3f\n",
            boxerHit, static_cast<unsigned long long>(boxerAttackId),
            boxerWindowSteps, sweptMovingPlayer, boxerWindupVisible,
            boxerRecoveryVisible, boxerTipMatchedPose,
            boxerWindupDisplacement);
        return 1;
    }
    BattleBoss boxerFacing(config.boss);
    boxerFacing.Reset({0.0f, config.boss.hoverHeight, 20.0f});
    boxerFacing.BeginBehavior(BossMode::Boxer);
    bool foundBoxerWindup = false;
    for (int step = 0; step < 180; ++step) {
        boxerFacing.Update(kStep, player);
        const BossVisualPose pose = boxerFacing.GetVisualPose();
        if (std::any_of(pose.cones.begin(), pose.cones.end(),
                [](const BossPartPose& cone) {
                    return std::abs(cone.rootTranslation.z) > 0.01f;
                })) {
            foundBoxerWindup = true;
            const float strikeYaw = pose.rootRotation.y;
            boxerFacing.Update(kStep, {12.0f, player.y, player.z});
            if (std::abs(std::remainder(
                    boxerFacing.GetVisualPose().rootRotation.y - strikeYaw,
                    6.283185307f)) > 0.001f) {
                std::puts("FAIL: Boxer strike facing followed moving player");
                return 1;
            }
            break;
        }
    }
    if (!foundBoxerWindup) {
        std::puts("FAIL: Boxer never exposed the windup pose");
        return 1;
    }
    BattleBoss entryBoss(config.boss);
    entryBoss.Reset({0.0f, config.boss.hoverHeight, 34.5f});
    entryBoss.BeginBehavior(BossMode::Boxer);
    bool boxerPulledBack = false;
    bool boxerDashed = false;
    for (int step = 0; step < 120; ++step) {
        const float previousZ = entryBoss.GetPosition().z;
        entryBoss.Update(kStep, player);
        boxerPulledBack |= entryBoss.GetPosition().z - previousZ > 0.01f;
        boxerDashed |= previousZ - entryBoss.GetPosition().z > 0.30f;
    }
    if (!boxerPulledBack || !boxerDashed) {
        std::puts("FAIL: Boxer entry lacked pullback or dash");
        return 1;
    }
    if (std::abs(config.boss.boxerEntryTimeout - 8.0f) > 0.001f ||
        std::abs(config.boss.boxerBehaviorDuration - 8.5f) > 0.001f) {
        std::puts("FAIL: original Boxer entry or combat duration changed");
        return 1;
    }
    BattleBoss boxerTiming(config.boss);
    boxerTiming.Reset({0.0f, config.boss.hoverHeight, 20.0f});
    boxerTiming.BeginBehavior(BossMode::Boxer);
    bool startedBoxerCombat = false;
    int ticksSinceFirstStrike = 0;
    bool boxerStayedActiveAtFourSeconds = false;
    bool boxerCruisedAfterDuration = false;
    for (int step = 0; step < 900; ++step) {
        boxerTiming.Update(kStep, player);
        if (!startedBoxerCombat && boxerTiming.DidBeginStrikeThisStep()) {
            startedBoxerCombat = true;
        }
        if (!startedBoxerCombat) continue;
        ++ticksSinceFirstStrike;
        if (ticksSinceFirstStrike == 240)
            boxerStayedActiveAtFourSeconds =
                boxerTiming.GetMode() == BossMode::Boxer;
        if (boxerTiming.GetMode() == BossMode::Cruise) {
            if (ticksSinceFirstStrike * kStep <
                    config.boss.boxerBehaviorDuration - kStep) {
                std::puts("FAIL: Boxer exited before original combat duration");
                return 1;
            }
            boxerCruisedAfterDuration = true;
            break;
        }
    }
    if (!startedBoxerCombat || !boxerStayedActiveAtFourSeconds ||
        !boxerCruisedAfterDuration) {
        std::puts("FAIL: Boxer did not retain original combat and Cruise timing");
        return 1;
    }
    boss.Reset({0.0f, config.boss.hoverHeight, 34.5f});
    boss.BeginBehavior(BossMode::NeedleCombo);
    std::array<Vector3, 3> needleWindupStart{};
    for (int step = 0; step < 38; ++step) {
        boss.Update(kStep, player);
        const BossVisualPose pose = boss.GetVisualPose();
        if (step == 0) {
            for (std::size_t index = 0; index < 3; ++index) {
                needleWindupStart[index] =
                    pose.cones[index].rootTranslation;
            }
        }
        if (step == 37) {
            for (std::size_t index = 0; index < 3; ++index) {
                const Vector3& current = pose.cones[index].rootTranslation;
                const Vector3& start = needleWindupStart[index];
                const float distance = std::sqrt(
                    (current.x - start.x) * (current.x - start.x) +
                    (current.y - start.y) * (current.y - start.y) +
                    (current.z - start.z) * (current.z - start.z));
                if (distance < 2.9f || distance > 3.2f) {
                    std::puts("FAIL: Needle Combo three-cone pullback pose");
                    return 1;
                }
            }
        }
    }
    bool needleHit = false;
    std::uint64_t needleIds[3]{};
    int needleIdCount = 0;
    std::array<bool, 3> needlePartMoved{};
    bool needleReachedPlayer = false;
    bool needleImpact = false;
    bool needleImpactStartedAfterStrike = true;
    bool needleImpactMatchedTarget = true;
    bool needleTipMatchedPose = true;
    bool needleSweepStayedContinuous = true;
    int needleCueCount = 0;
    Vector3 previousStrikeTip{};
    std::uint64_t previousStrikeId = 0;
    for (int step = 0; step < 180; ++step) {
        const BossVisualPose previousPose = boss.GetVisualPose();
        const auto events = boss.Update(kStep, player);
        needleCueCount += boss.DidBeginStrikeThisStep() ? 1 : 0;
        const BossVisualPose currentPose = boss.GetVisualPose();
        const bool enteredImpact = HasAttack(events,
            BossAttackType::NeedleImpact);
        if (enteredImpact && !needleImpact) {
            const Vector3 expectedImpact{boss.GetPosition().x,
                boss.GetPosition().y,
                boss.GetPosition().z - config.boss.needleStrikeOffset};
            for (const BossAttackEvent& event : events) {
                if (event.type != BossAttackType::NeedleImpact) {
                    continue;
                }
                needleImpactStartedAfterStrike &=
                    !HasAttack(events, BossAttackType::NeedleStrike) &&
                    std::hypot(event.previousPosition.x -
                            previousStrikeTip.x,
                        event.previousPosition.y - previousStrikeTip.y,
                        event.previousPosition.z - previousStrikeTip.z) <
                        0.002f;
                needleImpactMatchedTarget &=
                    std::hypot(event.position.x - expectedImpact.x,
                        event.position.y - expectedImpact.y,
                        event.position.z - expectedImpact.z) < 0.002f;
            }
        }
        for (const BossAttackEvent& event : events) {
            needleImpact |= event.type == BossAttackType::NeedleImpact;
            if (event.type == BossAttackType::NeedleStrike) {
                needleHit = true;
                if (Collision::SweepSphereAgainstMovingSphere(
                        event.previousPosition, event.position, event.radius,
                        {player.x, 3.5f, player.z},
                        {player.x, 3.5f, player.z},
                        FirstPersonTuning::kPlayerColliderRadius).didHit) {
                    boss.RegisterNeedleStrikeHit(event.attackId);
                }
                needleReachedPlayer |= std::hypot(boss.GetPosition().x -
                    player.x, boss.GetPosition().z - player.z) < 5.1f;
                if (needleIdCount == 0 ||
                    event.attackId != needleIds[needleIdCount - 1]) {
                    if (needleIdCount < 3) {
                        needleIds[needleIdCount++] = event.attackId;
                    }
                }
                if (needleIdCount >= 1 && needleIdCount <= 3) {
                    const std::size_t index = static_cast<std::size_t>(
                        needleIdCount - 1);
                    const Vector3& local =
                        currentPose.cones[index].rootTranslation;
                    const float sine = std::sin(currentPose.rootRotation.y);
                    const float cosine = std::cos(currentPose.rootRotation.y);
                    const Vector3 worldRoot{
                        boss.GetPosition().x + local.x * cosine +
                            local.z * sine,
                        boss.GetPosition().y + local.y,
                        boss.GetPosition().z - local.x * sine +
                            local.z * cosine};
                    const Vector3 expectedTip{
                        worldRoot.x,
                        worldRoot.y,
                        worldRoot.z - config.boss.needleTipOffset};
                    const float tipDifference =
                        std::hypot(event.position.x - expectedTip.x,
                            event.position.y - expectedTip.y,
                            event.position.z - expectedTip.z);
                    // The final strike changes to ChargeImpact before rendering.
                    const bool returnedToBasePose =
                        boss.GetNeedlePose() == BossNeedlePose::ChargeForm;
                    if (!enteredImpact && !returnedToBasePose) {
                        needleTipMatchedPose &= tipDifference < 0.002f;
                    }
                    if (previousStrikeId != 0) {
                        needleSweepStayedContinuous &=
                            std::hypot(event.previousPosition.x -
                                    previousStrikeTip.x,
                                event.previousPosition.y -
                                    previousStrikeTip.y,
                                event.previousPosition.z -
                                    previousStrikeTip.z) < 0.002f;
                    }
                    previousStrikeTip = event.position;
                    previousStrikeId = event.attackId;
                    const Vector3& before =
                        previousPose.cones[index].rootTranslation;
                    const Vector3& after =
                        currentPose.cones[index].rootTranslation;
                    needlePartMoved[index] |= std::abs(after.x - before.x) >
                            0.05f || std::abs(after.y - before.y) > 0.05f ||
                        std::abs(after.z - before.z) > 0.05f;
                }
            }
        }
    }
    if (!needleHit || !needleReachedPlayer || !needleImpact ||
        !needleImpactStartedAfterStrike || !needleImpactMatchedTarget ||
        !needleTipMatchedPose || !needleSweepStayedContinuous ||
        needleIdCount != 3 || needleCueCount != 3 ||
        !std::all_of(needlePartMoved.begin(),
            needlePartMoved.end(), [](bool moved) { return moved; }) ||
        needleIds[0] == needleIds[1] || needleIds[1] == needleIds[2]) {
        std::printf("FAIL: Needle Combo windows hit=%d reached=%d impact=%d impactOrder=%d impactTarget=%d pose=%d sweep=%d ids=%d parts=%d%d%d\n",
            needleHit, needleReachedPlayer, needleImpact,
            needleImpactStartedAfterStrike, needleImpactMatchedTarget,
            needleTipMatchedPose, needleSweepStayedContinuous,
            needleIdCount, needlePartMoved[0], needlePartMoved[1],
            needlePartMoved[2]);
        return 1;
    }
    BattleBoss missedNeedle(config.boss);
    missedNeedle.Reset({0.0f, config.boss.hoverHeight, 34.5f});
    missedNeedle.BeginBehavior(BossMode::NeedleCombo);
    int missedNeedleStrikes = 0;
    for (int step = 0; step < 180; ++step) {
        for (const BossAttackEvent& event : missedNeedle.Update(kStep,
                player)) {
            missedNeedleStrikes += event.type ==
                BossAttackType::NeedleStrike;
        }
        if (missedNeedle.GetMode() == BossMode::Boxer) {
            break;
        }
    }
    if (missedNeedle.GetMode() != BossMode::Boxer ||
        missedNeedleStrikes == 0) {
        std::puts("FAIL: missed first Needle strike did not switch to Boxer");
        return 1;
    }
    boss.BeginBehavior(BossMode::FullBarrage);
    int crystalShots = 0;
    bool turnedUpBeforeAscent = false;
    bool turnedDownBeforeFiring = false;
    bool spreadBeforeFiring = false;
    bool firedAboveDeployment = false;
    bool chargeFormAfterFiring = false;
    for (int step = 0; step < 600; ++step) {
        for (const BossAttackEvent& event : boss.Update(kStep, player)) {
            if (event.type == BossAttackType::CrystalSpike ||
                event.type == BossAttackType::CrystalCube) {
                ++crystalShots;
                firedAboveDeployment |= boss.GetPosition().y >
                    config.boss.hoverHeight + 0.01f;
            }
        }
        const BossVisualPose pose = boss.GetVisualPose();
        turnedUpBeforeAscent |= crystalShots == 0 &&
            pose.rootRotation.x > 0.5f &&
            boss.GetPosition().y == config.boss.hoverHeight;
        turnedDownBeforeFiring |= crystalShots == 0 &&
            pose.rootRotation.x < -0.5f;
        spreadBeforeFiring |= crystalShots == 0 &&
            boss.GetNeedlePose() == BossNeedlePose::Spread;
        chargeFormAfterFiring |= crystalShots == 10 &&
            boss.GetNeedlePose() == BossNeedlePose::ChargeForm;
        if (step > 0 && boss.GetMode() != BossMode::FullBarrage) break;
    }
    const int consumedSlots = static_cast<int>(std::count_if(
        boss.GetSpikeSlots().begin(), boss.GetSpikeSlots().end(),
        [](const BossCrystalSlot& slot) { return slot.consumed; })) +
        static_cast<int>(std::count_if(boss.GetCubeSlots().begin(),
            boss.GetCubeSlots().end(),
            [](const BossCrystalSlot& slot) { return slot.consumed; }));
    if (crystalShots != 10 || consumedSlots != 10 ||
        !turnedUpBeforeAscent || !turnedDownBeforeFiring ||
        !spreadBeforeFiring || firedAboveDeployment ||
        !chargeFormAfterFiring) {
        std::puts("FAIL: final barrage ascent, dive, firing and pose sequence");
        return 1;
    }
    BattleBoss closeBarrage(config.boss);
    closeBarrage.Reset({0.0f, config.boss.hoverHeight,
        config.boss.barrageDeploymentZ});
    closeBarrage.BeginBehavior(BossMode::FullBarrage);
    for (int step = 0; step < 600 &&
            closeBarrage.GetMode() == BossMode::FullBarrage; ++step) {
        closeBarrage.Update(kStep,
            {0.0f, 1.7f, config.boss.barrageDeploymentZ});
    }
    if (closeBarrage.GetMode() != BossMode::Cruise) {
        std::puts("FAIL: final barrage did not avoid close-range charge");
        return 1;
    }
    BattleBoss travelingBarrage(config.boss);
    travelingBarrage.Reset({10.0f, config.boss.hoverHeight, 20.0f});
    travelingBarrage.BeginBehavior(BossMode::FullBarrage);
    bool facedDeploymentTravel = false;
    for (int step = 0; step < 180 && !facedDeploymentTravel; ++step) {
        const Vector3 before = travelingBarrage.GetPosition();
        travelingBarrage.Update(kStep, player);
        if (travelingBarrage.GetPosition().x < before.x - 0.001f) {
            const float expectedYaw = std::atan2(-before.x,
                config.boss.barrageDeploymentZ - before.z) +
                3.141592654f;
            facedDeploymentTravel = std::abs(std::remainder(
                travelingBarrage.GetVisualPose().rootRotation.y - expectedYaw,
                6.283185307f)) < 0.001f;
        }
    }
    if (!facedDeploymentTravel) {
        std::puts("FAIL: final barrage did not face deployment travel");
        return 1;
    }
    BossCombatConfig slotConfig = config.boss;
    slotConfig.idleDecisionInterval = 100.0f;
    slotConfig.crystalSpikeInitialDelay = 0.05f;
    slotConfig.crystalSpikeInterval = 100.0f;
    slotConfig.crystalCubeInitialDelay = 100.0f;
    BattleBoss slotBoss(slotConfig);
    slotBoss.Reset({0.0f, slotConfig.hoverHeight, 20.0f});
    slotBoss.BeginBehavior(BossMode::Idle);
    bool slotLaunched = false;
    bool slotFired = false;
    bool slotReloaded = false;
    bool slotPreservedDuringCruise = false;
    bool remoteSpikeShot = false;
    bool cruiseFacedTravel = false;
    int crystalStartCues = 0;
    for (int step = 0; step < 240; ++step) {
        const auto events = slotBoss.Update(kStep, {0.0f, 1.7f, 0.0f});
        const Vector3 travel{
            slotBoss.GetPosition().x - slotBoss.GetPreviousPosition().x,
            0.0f,
            slotBoss.GetPosition().z - slotBoss.GetPreviousPosition().z};
        if (slotBoss.GetMode() == BossMode::Cruise &&
            std::hypot(travel.x, travel.z) > 0.001f) {
            cruiseFacedTravel |= nearAngle(
                slotBoss.GetVisualPose().rootRotation.y,
                std::atan2(travel.x, travel.z) +
                    std::numbers::pi_v<float>);
        }
        for (const BossAttackEvent& event : events) {
            if (event.type != BossAttackType::CrystalSpike) continue;
            remoteSpikeShot = near(event.position.x,
                    -24.0f + slotConfig.coreRadius) &&
                near(event.position.y, slotConfig.hoverHeight) &&
                near(event.position.z,
                    42.0f + slotConfig.crystalRemoteSpawnMargin) &&
                event.direction.x > 0.0f && event.direction.z < 0.0f &&
                near(event.oscillationPhase, 0.0f);
        }
        crystalStartCues += slotBoss.DidBeginCrystalLaunchThisStep() ? 1 : 0;
        slotLaunched |= std::any_of(slotBoss.GetSpikeSlots().begin(),
            slotBoss.GetSpikeSlots().end(), [](const BossCrystalSlot& slot) {
                return slot.launchTime > 0.0f;
            });
        if (slotLaunched && !slotPreservedDuringCruise) {
            slotBoss.BeginBehavior(BossMode::Cruise);
            slotPreservedDuringCruise = std::any_of(
                slotBoss.GetSpikeSlots().begin(),
                slotBoss.GetSpikeSlots().end(),
                [](const BossCrystalSlot& slot) {
                    return slot.launchTime > 0.0f;
                });
        }
        slotFired |= HasAttack(events, BossAttackType::CrystalSpike);
        if (slotFired) {
            slotReloaded |= std::any_of(slotBoss.GetSpikeSlots().begin(),
                slotBoss.GetSpikeSlots().end(),
                [](const BossCrystalSlot& slot) {
                    return slot.reloadTime > 0.0f;
                });
        }
    }
    if (!slotLaunched || !slotPreservedDuringCruise || !slotFired ||
        !slotReloaded ||
        crystalStartCues != 1 || !remoteSpikeShot || !cruiseFacedTravel) {
        std::puts("FAIL: crystal slot did not launch, cue once, fire and reload");
        return 1;
    }
    BossCombatConfig cubeConfig = slotConfig;
    cubeConfig.crystalSpikeInitialDelay = 100.0f;
    cubeConfig.crystalCubeInitialDelay = 0.05f;
    cubeConfig.crystalCubeInterval = 0.4f;
    BattleBoss cubeBoss(cubeConfig);
    cubeBoss.Reset({0.0f, cubeConfig.hoverHeight, 20.0f});
    cubeBoss.BeginBehavior(BossMode::Idle);
    int cubeShots = 0;
    for (int step = 0; step < 180 && cubeShots < 2; ++step) {
        for (const BossAttackEvent& event :
                cubeBoss.Update(kStep, {0.0f, 1.7f, 0.0f})) {
            if (event.type != BossAttackType::CrystalCube) continue;
            const float lane = cubeShots == 0 ? 0.3f : 1.0f;
            const float minimumX = -24.0f + cubeConfig.coreRadius;
            const float maximumX = 24.0f - cubeConfig.coreRadius;
            if (!near(event.position.x,
                    minimumX + (maximumX - minimumX) * lane) ||
                !near(event.oscillationPhase,
                    cubeShots * std::numbers::pi_v<float> * 0.5f)) {
                std::puts("FAIL: remote Cube lane or oscillation sequence");
                return 1;
            }
            ++cubeShots;
        }
    }
    if (cubeShots != 2) {
        std::puts("FAIL: remote Cube launch sequence stopped early");
        return 1;
    }
    boss.Reset({0.0f, config.boss.hoverHeight, 20.0f});
    boss.BeginBehavior(BossMode::SpreadCounter);
    bool burstHit = false;
    for (int step = 0; step < 150; ++step) {
        burstHit |= HasAttack(boss.Update(kStep, player),
            BossAttackType::SpreadBurst);
    }
    if (!boxerHit || !needleHit || crystalShots != 10 || !burstHit) {
        std::printf("FAIL: Boss modes boxer=%d needle=%d crystal=%d burst=%d\n",
            boxerHit, needleHit, crystalShots, burstHit);
        return 1;
    }
    if (!boss.ReceiveHit(3) || boss.GetHitPoints() != 17 ||
        !boss.ReceiveHit(17) || boss.GetHitPoints() != 1 ||
        boss.ReceiveHit(1)) {
        std::puts("FAIL: Boss core did not lock at one HP");
        return 1;
    }
    bool enteredFinalBarrage = false;
    for (int step = 0; step < 600; ++step) {
        boss.Update(kStep, player);
        enteredFinalBarrage |= boss.GetMode() == BossMode::FullBarrage;
    }
    const float pitchBeforeDefeat = boss.GetVisualPose().rootRotation.x;
    if (!enteredFinalBarrage || !boss.ReceiveHit(1) ||
        boss.GetMode() != BossMode::Defeating || boss.ReceiveHit(1)) {
        std::puts("FAIL: Boss final barrage did not unlock defeat");
        return 1;
    }
    if (std::abs(boss.GetVisualPose().rootRotation.x -
            pitchBeforeDefeat) > 0.001f) {
        std::puts("FAIL: Boss root pitch snapped at the start of defeat");
        return 1;
    }
    bool roseAfterDefeat = false;
    bool touchedGround = false;
    bool bounced = false;
    bool emittedSmallExplosion = false;
    const float defeatStartHeight = boss.GetPosition().y;
    for (int step = 0; step < 500 && !boss.IsDefeated(); ++step) {
        boss.Update(kStep, player);
        roseAfterDefeat |= boss.GetPosition().y > defeatStartHeight + 0.05f;
        touchedGround |= boss.GetPosition().y <=
            config.boss.coreRadius + 0.001f;
        bounced |= touchedGround && boss.GetPosition().y >
            config.boss.coreRadius + 0.05f;
        emittedSmallExplosion |=
            boss.DidEmitDefeatSmallExplosionThisStep();
    }
    if (!boss.IsDefeated() || !roseAfterDefeat || !touchedGround ||
        !bounced || !emittedSmallExplosion) {
        std::puts("FAIL: Boss launch, bounce, explosion or pause was incorrect");
        return 1;
    }
    std::puts("PASS: Boss modes, attacks, core damage and defeat");
    return 0;
}
