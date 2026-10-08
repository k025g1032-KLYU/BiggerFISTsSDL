#include "Data/BattleCombatConfig.h"
#include "Game/FirstPersonGame.h"
#include "Gameplay/Combat/CombatSession.h"
#include "Gameplay/Combat/EncounterConfig.h"
#include "Input/FixedStepInputBuffer.h"
#include "Platform/FixedStepClock.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

namespace {
constexpr float kStep = 1.0f / 60.0f;

struct Snapshot {
    Vector3 position{};
    std::size_t enemyCount = 0;
    int wave = 0;
};

Snapshot Simulate(int fps, const EncounterConfig& encounter,
    const BattleCombatConfig& combat) {
    FirstPersonGame player(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession session(BattlePreviewKind::Day1, player, encounter, combat);
    session.Start(true);
    FixedStepClock clock;
    FixedStepInputBuffer buffer;
    for (int frame = 0; frame < fps * 2; ++frame) {
        buffer.QueueFrame({}, 0.0f, 0.0f);
        const int steps = clock.Advance(1.0 / fps);
        for (int step = 0; step < steps; ++step) {
            const SimulationStepInput input = buffer.ConsumeStep(steps - step);
            session.Update(kStep, input.game, input.mouseDeltaX,
                input.mouseDeltaY);
        }
    }
    Snapshot result;
    result.enemyCount = session.GetEnemies().size();
    result.wave = session.GetWaveIndex();
    if (!session.GetEnemies().empty()) {
        result.position = session.GetEnemies().front().position;
    }
    return result;
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 1;
    }
    const std::filesystem::path directory = argv[1];
    EncounterConfig day1;
    EncounterConfig day2;
    BattleCombatConfig combat;
    std::string error;
    if (!LoadEncounterConfig(directory / "day1-encounter.cfg", day1, error) ||
        !LoadEncounterConfig(directory / "day2-encounter.cfg", day2, error) ||
        !LoadBattleCombatConfig(directory, combat, error)) {
        std::printf("FAIL: %s\n", error.c_str());
        return 1;
    }
    const Snapshot a = Simulate(30, day1, combat);
    const Snapshot b = Simulate(60, day1, combat);
    const Snapshot c = Simulate(120, day1, combat);
    if (a.enemyCount != b.enemyCount || b.enemyCount != c.enemyCount ||
        a.wave != b.wave || b.wave != c.wave ||
        std::abs(a.position.y - b.position.y) > 0.0001f ||
        std::abs(b.position.y - c.position.y) > 0.0001f) {
        std::puts("FAIL: battle state differs at 30/60/120 rendered FPS");
        return 1;
    }
    FirstPersonGame player(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession session(BattlePreviewKind::Day1, player, day1, combat);
    session.Start(true);
    bool sawExplosion = false;
    for (int step = 0; step < 10000 && session.GetPhase() == BattlePhase::Playing;
        ++step) {
        session.Update(kStep, {}, 0.0f, 0.0f);
        for (const BattleEnemy& enemy : session.GetEnemies()) {
            if (!enemy.defeated) {
                session.ApplyTargetDamage(enemy.id, 1);
                sawExplosion |= std::any_of(session.GetVisualEvents().begin(),
                    session.GetVisualEvents().end(),
                    [](const BattleVisualEvent& event) {
                        return event.effect == BattleVisualEffect::Explosion;
                    });
            }
        }
    }
    if (!sawExplosion || session.GetPhase() != BattlePhase::Clear ||
        session.GetWaveIndex() != 2) {
        std::puts("FAIL: Day1 waves did not advance through Clear");
        return 1;
    }
    FirstPersonGame day2Player(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession day2Session(BattlePreviewKind::Day2, day2Player, day2, combat);
    day2Session.Start(true);
    for (int step = 0; step < 10000 &&
            day2Session.GetPhase() == BattlePhase::Playing; ++step) {
        day2Session.Update(kStep, {}, 0.0f, 0.0f);
        for (const BattleEnemy& enemy : day2Session.GetEnemies()) {
            if (!enemy.defeated) {
                day2Session.ApplyTargetDamage(enemy.id, 1);
            }
        }
    }
    if (day2Session.GetPhase() != BattlePhase::Clear ||
        day2Session.GetWaveIndex() != 2 ||
        day2Session.GetBaseHitPoints() != combat.enemy.day2BaseHitPoints) {
        std::puts("FAIL: Day2 waves or 20 HP base did not reach Clear");
        return 1;
    }
    GameInput reset{};
    reset.resetTarget.pressed = true;
    session.Update(kStep, reset, 0.0f, 0.0f);
    if (session.GetPhase() != BattlePhase::Ready ||
        !session.GetEnemies().empty() ||
        session.GetBaseHitPoints() != combat.enemy.baseHitPoints) {
        std::puts("FAIL: battle reset retained old state");
        return 1;
    }
    FirstPersonGame retryPlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession retrySession(BattlePreviewKind::Day1, retryPlayer,
        day1, combat);
    retrySession.Start(true);
    while (!retryPlayer.GetVitality().IsDefeated()) {
        retryPlayer.ReceivePlayerHit(PlayerHitType::NeedleFinisher);
    }
    retrySession.Update(kStep, {}, 0.0f, 0.0f);
    if (retrySession.GetPhase() != BattlePhase::GameOver) {
        std::puts("FAIL: defeated player did not enter GameOver");
        return 1;
    }
    GameInput retry{};
    retry.battleStart.pressed = true;
    retrySession.Update(kStep, retry, 0.0f, 0.0f);
    if (retrySession.GetPhase() != BattlePhase::Countdown ||
        retryPlayer.GetVitality().IsDefeated() ||
        retrySession.GetBaseHitPoints() != combat.enemy.baseHitPoints ||
        !retrySession.GetEnemies().empty()) {
        std::puts("FAIL: Enter retry did not reset the battle and start countdown");
        return 1;
    }
    CombatSession bossSession(BattlePreviewKind::Boss, player, {}, combat);
    bossSession.Start(true);
    if (!bossSession.GetBoss() ||
        std::abs(bossSession.GetBoss()->GetVisualPose().rootRotation.x +
            1.570796327f) > 0.001f ||
        std::abs(bossSession.GetBoss()->GetVisualPose().cones[0]
            .translation.x - 1.0f) > 0.001f) {
        std::puts("FAIL: original Boss entrance or cone pose was not preserved");
        return 1;
    }
    if (!bossSession.GetBoss() ||
        !bossSession.ApplyTargetDamage("target_boss_core", 17) ||
        bossSession.GetBoss()->GetHitPoints() != 3 ||
        !bossSession.ApplyTargetDamage("target_boss_core", 3) ||
        bossSession.GetBoss()->GetHitPoints() != 1 ||
        bossSession.ApplyTargetDamage("target_boss_core", 1)) {
        std::puts("FAIL: Boss core accepts Punch and Ultimate damage");
        return 1;
    }
    for (int step = 0; step < 360; ++step) {
        bossSession.Update(kStep, {}, 0.0f, 0.0f);
        for (const BattleProjectile& projectile : bossSession.GetProjectiles()) {
            bossSession.ApplyTargetDamage(projectile.id, 1);
        }
    }
    if (!bossSession.ApplyTargetDamage("target_boss_core", 1)) {
        std::puts("FAIL: Boss core remained locked after Full Barrage");
        return 1;
    }
    for (int step = 0; step < 300; ++step) {
        bossSession.Update(kStep, {}, 0.0f, 0.0f);
        for (const BattleProjectile& projectile : bossSession.GetProjectiles()) {
            bossSession.ApplyTargetDamage(projectile.id, 1);
        }
    }
    if (bossSession.GetPhase() != BattlePhase::Clear) {
        std::puts("FAIL: Boss death did not reach Clear");
        return 1;
    }
    FirstPersonGame chargePlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession chargeSession(BattlePreviewKind::ChargeUnit, chargePlayer, {}, combat);
    chargeSession.Start(true);
    bool sawShieldEffect = false;
    for (int step = 0; step < 900 &&
            chargePlayer.GetVitality().GetShieldLayers() == 3; ++step) {
        chargeSession.Update(kStep, {}, 0.0f, 0.0f);
        sawShieldEffect |= std::any_of(chargeSession.GetVisualEvents().begin(),
            chargeSession.GetVisualEvents().end(),
            [](const BattleVisualEvent& event) {
                return event.effect == BattleVisualEffect::ShieldHit &&
                    event.shieldLayers == 3;
            });
    }
    if (!sawShieldEffect || chargePlayer.GetVitality().GetShieldLayers() >= 3 ||
        chargeSession.GetEnemies().empty() ||
        chargeSession.GetEnemies().front().defeated) {
        if (!chargeSession.GetEnemies().empty()) {
            const BattleEnemy& enemy = chargeSession.GetEnemies().front();
            std::printf("FAIL: ChargeUnit shield=%d state=%d position=(%.2f,%.2f,%.2f)\n",
                chargePlayer.GetVitality().GetShieldLayers(),
                static_cast<int>(enemy.chargeState), enemy.position.x,
                enemy.position.y, enemy.position.z);
        } else {
            std::puts("FAIL: ChargeUnit disappeared");
        }
        return 1;
    }
    FirstPersonGame missilePlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession missileSession(BattlePreviewKind::MissileUnit,
        missilePlayer, {}, combat);
    missileSession.Start(true);
    bool launched = false;
    bool turnedTowardBase = false;
    for (int step = 0; step < 1200 &&
            missileSession.GetPhase() == BattlePhase::Playing; ++step) {
        missileSession.Update(kStep, {}, 0.0f, 0.0f);
        launched |= !missileSession.GetProjectiles().empty();
        for (const BattleProjectile& projectile :
                missileSession.GetProjectiles()) {
            turnedTowardBase |= projectile.missileFlightState ==
                BattleProjectile::MissileFlightState::BaseRun &&
                projectile.direction.x * projectile.position.x <= 0.0f;
        }
    }
    if (!launched || !turnedTowardBase ||
        (missileSession.GetBaseHitPoints() == combat.enemy.baseHitPoints &&
            missilePlayer.GetVitality().GetShieldLayers() == 3)) {
        std::puts("FAIL: MissileUnit did not turn toward base or hit player/base");
        return 1;
    }
    FirstPersonGame countdownPlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession countdownSession(BattlePreviewKind::MissileUnit,
        countdownPlayer, {}, combat);
    countdownPlayer.ReceivePlayerHit(PlayerHitType::Projectile);
    GameInput moving{};
    moving.moveY = 1.0f;
    countdownSession.Update(kStep, moving, 0.0f, 0.0f);
    if (countdownPlayer.GetPlayer().GetPosition().z == 0.0f) {
        std::puts("FAIL: Ready state did not update player movement");
        return 1;
    }
    countdownSession.Start();
    const Vector3 resetPosition = countdownPlayer.GetPlayer().GetPosition();
    if (countdownPlayer.GetVitality().GetShieldLayers() != 3 ||
        std::abs(resetPosition.z) > 0.001f) {
        std::puts("FAIL: starting countdown did not reset player");
        return 1;
    }
    for (int step = 0; step < 60; ++step) {
        countdownSession.Update(kStep, moving, 0.0f, 0.0f);
    }
    if (countdownSession.GetPhase() != BattlePhase::Countdown ||
        !countdownSession.GetEnemies().empty() ||
        std::abs(countdownPlayer.GetPlayer().GetPosition().z) > 0.001f) {
        std::puts("FAIL: countdown simulated player or spawned enemies");
        return 1;
    }
    FirstPersonGame windupPlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession windupSession(BattlePreviewKind::MissileUnit,
        windupPlayer, {}, combat);
    windupSession.Start(true);
    for (int step = 0; step < 600 &&
            windupSession.GetEnemies().front().missileState !=
                BattleEnemy::MissileState::FireWindup; ++step) {
        windupSession.Update(kStep, {}, 0.0f, 0.0f);
    }
    if (windupSession.GetEnemies().front().missileState !=
            BattleEnemy::MissileState::FireWindup ||
        !windupSession.GetProjectiles().empty()) {
        std::puts("FAIL: MissileUnit skipped fire windup");
        return 1;
    }
    for (int step = 0; step < 30 && windupSession.GetProjectiles().empty();
        ++step) {
        windupSession.Update(kStep, {}, 0.0f, 0.0f);
    }
    if (windupSession.GetProjectiles().empty() ||
        windupSession.GetEnemies().front().missileState !=
            BattleEnemy::MissileState::FireRecovery) {
        std::puts("FAIL: MissileUnit did not launch after windup");
        return 1;
    }
    FirstPersonGame bomberPlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession bomberSession(BattlePreviewKind::Bomber, bomberPlayer, {},
        combat);
    bomberSession.Start(true);
    const float bomberSpeed = std::abs(
        bomberSession.GetEnemies().front().velocity.x);
    if (bomberSpeed < combat.enemy.bomberMinimumMoveSpeed ||
        bomberSpeed > combat.enemy.bomberMaximumMoveSpeed) {
        std::puts("FAIL: Bomber patrol speed is outside original range");
        return 1;
    }
    bool droppedBomb = false;
    bool bombMovedLaterally = false;
    bool bombStayedAtCombatHeight = true;
    for (int step = 0; step < 900 &&
            bomberSession.GetPhase() == BattlePhase::Playing; ++step) {
        bomberSession.Update(kStep, {}, 0.0f, 0.0f);
        droppedBomb |= !bomberSession.GetProjectiles().empty();
        for (const BattleProjectile& projectile :
                bomberSession.GetProjectiles()) {
            bombMovedLaterally |= std::abs(projectile.position.x -
                projectile.forwardPosition.x) > 0.1f;
            bombStayedAtCombatHeight &= projectile.position.y >=
                combat.enemy.combatHeightMinimum && projectile.position.y <=
                combat.enemy.combatHeightMaximum;
        }
    }
    if (!droppedBomb || !bombMovedLaterally ||
        !bombStayedAtCombatHeight) {
        std::puts("FAIL: Bomber bomb did not move laterally at combat height");
        return 1;
    }
    FirstPersonGame aimPlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession aimSession(BattlePreviewKind::Boss, aimPlayer, {}, combat);
    aimSession.Start(true);
    GameInput singleRocket{};
    singleRocket.leftMouseDown = true;
    bool bossDodgedAim = false;
    for (int step = 0; step < 150 &&
            aimSession.GetPhase() == BattlePhase::Playing; ++step) {
        aimSession.Update(kStep, singleRocket, 0.0f, 0.0f);
        bossDodgedAim |= aimSession.GetBoss() &&
            aimSession.GetBoss()->DidBeginRocketDodgeThisStep();
    }
    if (!bossDodgedAim) {
        std::puts("FAIL: CombatSession did not send charged Rocket aim to Boss");
        return 1;
    }
    FirstPersonGame ultimatePlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession ultimateSession(BattlePreviewKind::Boss, ultimatePlayer,
        {}, combat);
    ultimateSession.Start(true);
    GameInput dualRocket{};
    dualRocket.leftMouseDown = true;
    dualRocket.rightMouseDown = true;
    bool dodgedUltimateCharge = false;
    for (int step = 0; step < 240; ++step) {
        ultimateSession.Update(kStep, dualRocket, 0.0f, 0.0f);
        dodgedUltimateCharge |= ultimateSession.GetBoss() &&
            ultimateSession.GetBoss()->DidBeginRocketDodgeThisStep();
    }
    if (ultimatePlayer.GetUltimateState() != UltimateState::Ready ||
        dodgedUltimateCharge) {
        std::puts("FAIL: dual Rocket charge did not prepare Ultimate cleanly");
        return 1;
    }
    bool ultimateLaunched = false;
    for (int step = 0; step < 120; ++step) {
        ultimateSession.Update(kStep, {}, 0.0f, 0.0f);
        ultimateLaunched |= ultimatePlayer.GetEvents().ultimateLaunched;
    }
    if (!ultimateLaunched || !ultimateSession.GetBoss() ||
        ultimateSession.GetBoss()->GetHitPoints() >= combat.boss.coreHitPoints) {
        std::puts("FAIL: Ultimate did not hit Boss through battle collision");
        return 1;
    }
    BattleCombatConfig crystalCombat = combat;
    crystalCombat.boss.idleDecisionInterval = 100.0f;
    crystalCombat.boss.crystalSpikeInitialDelay = 0.05f;
    crystalCombat.boss.crystalSpikeInterval = 100.0f;
    crystalCombat.boss.crystalCubeInitialDelay = 100.0f;
    FirstPersonGame crystalPlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession crystalSession(BattlePreviewKind::Boss, crystalPlayer,
        {}, crystalCombat);
    crystalSession.Start(true);
    bool launchedFromSlot = false;
    for (int step = 0; step < 180; ++step) {
        crystalSession.Update(kStep, {}, 0.0f, 0.0f);
        for (const BattleProjectile& projectile :
                crystalSession.GetProjectiles()) {
            if (projectile.type == BattleProjectileType::CrystalSpike &&
                projectile.position.y > crystalCombat.boss.hoverHeight &&
                projectile.direction.y < 0.0f) {
                launchedFromSlot = true;
            }
        }
    }
    if (!launchedFromSlot) {
        std::puts("FAIL: Boss crystal did not spawn above halo and fly downward");
        return 1;
    }
    std::puts("PASS: fixed timing, Day1/Day2/Boss Clear, reset/countdown, Charge impact, Missile windup and Bomber");
    return 0;
}
