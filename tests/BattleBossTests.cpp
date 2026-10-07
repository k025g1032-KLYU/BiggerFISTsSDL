#include "Data/BattleCombatConfig.h"
#include "Gameplay/Combat/BattleBoss.h"

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
    for (int step = 0; step < 180; ++step) {
        boxerHit |= HasAttack(boss.Update(kStep, player),
            BossAttackType::BoxerStrike);
    }
    boss.BeginBehavior(BossMode::NeedleCombo);
    bool needleHit = false;
    for (int step = 0; step < 180; ++step) {
        needleHit |= HasAttack(boss.Update(kStep, player),
            BossAttackType::NeedleStrike);
    }
    boss.BeginBehavior(BossMode::FullBarrage);
    int crystalShots = 0;
    for (int step = 0; step < 300; ++step) {
        for (const BossAttackEvent& event : boss.Update(kStep, player)) {
            crystalShots += event.type == BossAttackType::CrystalSpike ||
                event.type == BossAttackType::CrystalCube;
        }
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
