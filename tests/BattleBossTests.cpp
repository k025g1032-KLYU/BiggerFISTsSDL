#include "Data/BattleCombatConfig.h"
#include "Gameplay/Combat/BattleBoss.h"
#include "Collision/Collision.h"
#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
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
        std::abs(initialPose.cones[0].translation.x - 1.0f) > 0.001f ||
        std::abs(initialPose.spikes[0].translation.x -
            config.boss.crystalHaloRadius) > 0.001f ||
        std::abs(initialPose.spikes[0].translation.z -
            config.boss.crystalHaloBackOffset) > 0.001f) {
        std::puts("FAIL: original Boss entrance, cone or crystal local pose changed");
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
    boss.Reset({0.0f, config.boss.hoverHeight, 20.0f});
    boss.BeginBehavior(BossMode::Boxer);
    bool boxerHit = false;
    std::uint64_t boxerAttackId = 0;
    int boxerWindowSteps = 0;
    bool sweptMovingPlayer = false;
    for (int step = 0; step < 180; ++step) {
        for (const BossAttackEvent& event : boss.Update(kStep, player)) {
            if (event.type != BossAttackType::BoxerStrike) {
                continue;
            }
            boxerHit = true;
            if (boxerAttackId == 0) {
                boxerAttackId = event.attackId;
            }
            if (event.attackId == boxerAttackId) {
                ++boxerWindowSteps;
            }
            sweptMovingPlayer |= Collision::SweepSphereAgainstMovingSphere(
                event.previousPosition, event.position, event.radius,
                {0.0f, 3.5f, 18.0f}, {0.0f, 3.5f, 17.5f},
                FirstPersonTuning::kPlayerColliderRadius).didHit;
        }
    }
    if (!boxerHit || boxerAttackId == 0 || boxerWindowSteps < 2 ||
        boxerWindowSteps > 7 || !sweptMovingPlayer) {
        std::puts("FAIL: Boxer attack lacked a finite swept collision window");
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
    boss.Reset({0.0f, config.boss.hoverHeight, 34.5f});
    boss.BeginBehavior(BossMode::NeedleCombo);
    bool needleHit = false;
    std::uint64_t needleIds[3]{};
    int needleIdCount = 0;
    bool needleReachedPlayer = false;
    bool needleImpact = false;
    for (int step = 0; step < 180; ++step) {
        for (const BossAttackEvent& event : boss.Update(kStep, player)) {
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
            }
        }
    }
    if (!needleHit || !needleReachedPlayer || !needleImpact ||
        needleIdCount != 3 ||
        needleIds[0] == needleIds[1] || needleIds[1] == needleIds[2]) {
        std::puts("FAIL: Needle Combo did not create three distinct attack windows");
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
    for (int step = 0; step < 300; ++step) {
        for (const BossAttackEvent& event : boss.Update(kStep, player)) {
            crystalShots += event.type == BossAttackType::CrystalSpike ||
                event.type == BossAttackType::CrystalCube;
        }
    }
    const int consumedSlots = static_cast<int>(std::count_if(
        boss.GetSpikeSlots().begin(), boss.GetSpikeSlots().end(),
        [](const BossCrystalSlot& slot) { return slot.consumed; })) +
        static_cast<int>(std::count_if(boss.GetCubeSlots().begin(),
            boss.GetCubeSlots().end(),
            [](const BossCrystalSlot& slot) { return slot.consumed; }));
    if (crystalShots != 10 || consumedSlots != 10) {
        std::puts("FAIL: final barrage did not consume all ten crystal slots");
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
    for (int step = 0; step < 240; ++step) {
        const auto events = slotBoss.Update(kStep, {0.0f, 1.7f, 0.0f});
        slotLaunched |= std::any_of(slotBoss.GetSpikeSlots().begin(),
            slotBoss.GetSpikeSlots().end(), [](const BossCrystalSlot& slot) {
                return slot.launchTime > 0.0f;
            });
        slotFired |= HasAttack(events, BossAttackType::CrystalSpike);
        if (slotFired) {
            slotReloaded |= std::any_of(slotBoss.GetSpikeSlots().begin(),
                slotBoss.GetSpikeSlots().end(),
                [](const BossCrystalSlot& slot) {
                    return slot.reloadTime > 0.0f;
                });
        }
    }
    if (!slotLaunched || !slotFired || !slotReloaded) {
        std::puts("FAIL: crystal slot did not launch, fire and reload");
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
    for (int step = 0; step < 360; ++step) {
        boss.Update(kStep, player);
        enteredFinalBarrage |= boss.GetMode() == BossMode::FullBarrage;
    }
    if (!enteredFinalBarrage || !boss.ReceiveHit(1) ||
        boss.GetMode() != BossMode::Defeating || boss.ReceiveHit(1)) {
        std::puts("FAIL: Boss final barrage did not unlock defeat");
        return 1;
    }
    for (int step = 0; step < 200; ++step) {
        boss.Update(kStep, player);
    }
    if (!boss.IsDefeated()) {
        std::puts("FAIL: Boss defeat sequence did not finish");
        return 1;
    }
    std::puts("PASS: Boss modes, attacks, core damage and defeat");
    return 0;
}
