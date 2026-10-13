#include "Data/BattleCombatConfig.h"
#include "Game/FirstPersonGame.h"
#include "Gameplay/Combat/CombatSession.h"
#include "Gameplay/Combat/EncounterConfig.h"
#include "Gameplay/Player/FirstPersonTuning.h"
#include "Input/FixedStepInputBuffer.h"
#include "Platform/FixedStepClock.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <utility>

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

bool DefeatPracticeEnemyWithInput(int renderedFps,
    const BattleCombatConfig& combat) {
    FirstPersonGame player(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession session(BattlePreviewKind::Practice, player, {}, combat);
    FixedStepClock clock;
    int tick = 0;
    for (int frame = 0; frame < renderedFps * 25; ++frame) {
        const int steps = clock.Advance(1.0 / renderedFps);
        for (int index = 0; index < steps; ++index, ++tick) {
            GameInput input{};
            float mouseX = 0.0f;
            if (tick == 0) input.practiceCommand = PracticeCommand::MissileUnit;
            if (!session.GetEnemies().empty()) {
                const BattleEnemy& enemy = session.GetEnemies().front();
                if (enemy.deployed && !enemy.defeated) {
                    const Vector3 origin = player.GetPlayer().GetPosition();
                    const float dx = enemy.position.x - origin.x;
                    const float dz = enemy.position.z - origin.z;
                    const float distance = std::hypot(dx, dz);
                    const float yaw = std::atan2(dx, dz) +
                        std::atan2(FirstPersonTuning::kShoulderHalfWidth,
                            std::max(1.0f, distance - 4.0f));
                    const float cameraError = std::remainder(yaw -
                        player.GetCamera().GetYaw(), 6.283185307f);
                    mouseX = std::clamp(cameraError /
                        FirstPersonTuning::kMouseLookSensitivity,
                        -75.0f, 75.0f);
                    const float bodyError = std::abs(std::remainder(yaw -
                        player.GetPlayer().GetBodyYaw(), 6.283185307f));
                    const PunchArmState arm = player.GetLeftArm().GetState();
                    if (bodyError < 0.055f &&
                        (arm == PunchArmState::Ready ||
                            arm == PunchArmState::Charging)) {
                        input.leftMouseDown = true;
                    }
                }
            }
            session.Update(kStep, input, mouseX, 0.0f);
            if (!session.GetEnemies().empty() &&
                session.GetEnemies().front().defeated) {
                return session.GetPhase() == BattlePhase::Ready;
            }
        }
    }
    return false;
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
    const auto defeatCheck = [&](BattleAttackType type,
        const Vector3& previous, const Vector3& current,
        const Vector3& expected) {
        FirstPersonGame testPlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
            std::vector<FirstPersonTarget>{});
        CombatSession testSession(BattlePreviewKind::MissileUnit,
            testPlayer, {}, combat);
        testSession.Start(true);
        const std::string id = testSession.GetEnemies().front().id;
        if (!testSession.ApplyTargetDamage(id, 1,
                {type, previous, current})) return false;
        const BattleEnemy& enemy = testSession.GetEnemies().front();
        const bool initial = enemy.defeated &&
            std::abs(enemy.defeatVelocity.x - expected.x) < 0.001f &&
            std::abs(enemy.defeatVelocity.y - expected.y) < 0.001f &&
            std::abs(enemy.defeatVelocity.z - expected.z) < 0.001f &&
            !testSession.ApplyTargetDamage(id, 1,
                {type, previous, current});
        testSession.Update(kStep, {}, 0.0f, 0.0f);
        const BattleEnemy& flying = testSession.GetEnemies().front();
        const bool advanced = flying.position.x != flying.previousPosition.x ||
            flying.position.z != flying.previousPosition.z;
        const bool rotation = flying.defeatRotation.x > 0.0f &&
            flying.defeatRotation.z > 0.0f;
        testSession.Reset();
        return initial && advanced && rotation &&
            testSession.GetEnemies().empty();
    };
    if (!defeatCheck(BattleAttackType::Punch, {0, 0, 0}, {1, 0, 0},
            {13, 11, 0}) ||
        !defeatCheck(BattleAttackType::RocketPunch, {0, 0, 0},
            {0, 0, -1}, {0, 11, -26}) ||
        !defeatCheck(BattleAttackType::Ultimate, {0, 0, 0},
            {0, 0, 1}, {0, 11, 40})) {
        std::puts("FAIL: attack-directed defeat launch, rotation or reset");
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
        sawExplosion |= std::any_of(session.GetVisualEvents().begin(),
            session.GetVisualEvents().end(),
            [](const BattleVisualEvent& event) {
                return event.effect == BattleVisualEffect::Explosion;
            });
        for (const BattleEnemy& enemy : session.GetEnemies()) {
            if (!enemy.defeated) {
                session.ApplyTargetDamage(enemy.id, 1);
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
            .translation.x) > 0.001f) {
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
    for (int step = 0; step < 600; ++step) {
        bossSession.Update(kStep, {}, 0.0f, 0.0f);
        for (const BattleProjectile& projectile : bossSession.GetProjectiles()) {
            bossSession.ApplyTargetDamage(projectile.id, 1);
        }
    }
    if (!bossSession.ApplyTargetDamage("target_boss_core", 1)) {
        std::puts("FAIL: Boss core remained locked after Full Barrage");
        return 1;
    }
    int smallDefeatExplosions = 0;
    int finalDefeatExplosions = 0;
    int defeatModeCues = 0;
    bool heldFinalExplosion = false;
    int finalExplosionStep = -1;
    int clearStep = -1;
    for (int step = 0; step < 300; ++step) {
        bossSession.Update(kStep, {}, 0.0f, 0.0f);
        for (const BattleFeedbackEvent event : bossSession.GetFeedbackEvents()) {
            smallDefeatExplosions +=
                event == BattleFeedbackEvent::BossDefeatSmallExplosion;
            finalDefeatExplosions +=
                event == BattleFeedbackEvent::BossDefeated;
            if (event == BattleFeedbackEvent::BossDefeated) {
                heldFinalExplosion = bossSession.GetPhase() ==
                    BattlePhase::Playing &&
                    bossSession.HasActiveExplosionEffects();
                finalExplosionStep = step;
            }
            defeatModeCues +=
                event == BattleFeedbackEvent::BossNeedlePoseChanged;
        }
        for (const BattleProjectile& projectile : bossSession.GetProjectiles()) {
            bossSession.ApplyTargetDamage(projectile.id, 1);
        }
        if (clearStep < 0 && bossSession.GetPhase() == BattlePhase::Clear)
            clearStep = step;
    }
    if (bossSession.GetPhase() != BattlePhase::Clear ||
        smallDefeatExplosions == 0 || finalDefeatExplosions != 1 ||
        !heldFinalExplosion || finalExplosionStep < 0 ||
        clearStep - finalExplosionStep < 58 ||
        bossSession.HasActiveExplosionEffects() ||
        defeatModeCues != 0) {
        std::puts("FAIL: Boss defeat explosion sequence or Clear was incorrect");
        return 1;
    }
    FirstPersonGame chargePlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession chargeSession(BattlePreviewKind::ChargeUnit, chargePlayer, {}, combat);
    chargeSession.Start(true);
    if (chargeSession.GetHitboxes().size() != 1 ||
        std::abs(chargeSession.GetHitboxes().front().radius -
            combat.enemy.radius) > 0.00001f) {
        std::puts("FAIL: ChargeUnit collision sphere differs from enemy rule");
        return 1;
    }
    bool sawShieldEffect = false;
    for (int step = 0; step < 900 &&
            chargePlayer.GetVitality().GetShieldLayers() == 3; ++step) {
        chargeSession.Update(kStep, {}, 0.0f, 0.0f);
        sawShieldEffect |= std::any_of(chargeSession.GetVisualEvents().begin(),
            chargeSession.GetVisualEvents().end(),
            [](const BattleVisualEvent& event) {
                return event.effect == BattleVisualEffect::ShieldHit &&
                    event.shieldImpact == ShieldImpactState::Broken;
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
    FirstPersonGame wallPlayer(GameConfig{}, {15.0f, 5.85f, -12.0f},
        std::vector<FirstPersonTarget>{});
    BattleCombatConfig wallCombat = combat;
    wallCombat.enemy.chargeTargetPassDistance = 1000.0f;
    CombatSession wallSession(BattlePreviewKind::ChargeUnit, wallPlayer, {}, wallCombat);
    wallSession.Start(true);
    bool chargePrepared = false;
    bool heardWallImpact = false;
    bool wallSoundDuringCharge = false;
    for (int step = 0; step < 1800 && !heardWallImpact; ++step) {
        GameInput input{};
        if (chargePrepared) input.moveX = 1.0f;
        wallSession.Update(kStep, input, 0.0f, 0.0f);
        if (!wallSession.GetEnemies().empty()) {
            const BattleEnemy& enemy = wallSession.GetEnemies().front();
            chargePrepared |= enemy.chargeState ==
                BattleEnemy::ChargeState::Preparing;
            heardWallImpact = std::find(wallSession.GetFeedbackEvents().begin(),
                wallSession.GetFeedbackEvents().end(),
                BattleFeedbackEvent::ChargeWallImpact) !=
                wallSession.GetFeedbackEvents().end();
            wallSoundDuringCharge = heardWallImpact &&
                enemy.chargeState == BattleEnemy::ChargeState::Charging &&
                enemy.chargeReachedObstacle;
        }
    }
    if (!chargePrepared || !heardWallImpact || !wallSoundDuringCharge) {
        std::puts("FAIL: ChargeUnit wall impact cue or deferred transition");
        return 1;
    }
    wallSession.Update(kStep, {}, 0.0f, 0.0f);
    if (wallSession.GetEnemies().empty() ||
        wallSession.GetEnemies().front().chargeState !=
            BattleEnemy::ChargeState::ReacquiringTarget ||
        std::find(wallSession.GetFeedbackEvents().begin(),
            wallSession.GetFeedbackEvents().end(),
            BattleFeedbackEvent::ChargeWallImpact) !=
            wallSession.GetFeedbackEvents().end()) {
        std::puts("FAIL: ChargeUnit wall impact repeated or did not reacquire");
        return 1;
    }
    FirstPersonGame missilePlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession missileSession(BattlePreviewKind::MissileUnit,
        missilePlayer, {}, combat);
    missileSession.Start(true);
    const auto initialHitboxes = missileSession.GetHitboxes();
    if (initialHitboxes.size() != 1 ||
        initialHitboxes.front().kind != BattleHitboxKind::Enemy ||
        std::abs(initialHitboxes.front().radius - combat.enemy.radius) >
            0.00001f ||
        initialHitboxes.front().center.y !=
            combat.enemy.dropStartHeight) {
        std::puts("FAIL: enemy hitbox did not expose simulation radius and spawn center");
        return 1;
    }
    bool launched = false;
    bool turnedTowardBase = false;
    bool projectileHitboxMatched = false;
    for (int step = 0; step < 1200 &&
            missileSession.GetPhase() == BattlePhase::Playing; ++step) {
        missileSession.Update(kStep, {}, 0.0f, 0.0f);
        launched |= !missileSession.GetProjectiles().empty();
        for (const BattleProjectile& projectile :
                missileSession.GetProjectiles()) {
            for (const BattleHitbox& hitbox : missileSession.GetHitboxes()) {
                if (hitbox.id == projectile.id &&
                    hitbox.kind == BattleHitboxKind::Projectile &&
                    std::abs(hitbox.radius -
                        combat.projectile.missileRadius) < 0.00001f &&
                    std::abs(hitbox.previousCenter.z -
                        projectile.previousPosition.z) < 0.00001f)
                    projectileHitboxMatched = true;
            }
            turnedTowardBase |= projectile.missileFlightState ==
                BattleProjectile::MissileFlightState::BaseRun &&
                projectile.direction.x * projectile.position.x <= 0.0f;
        }
    }
    if (!launched || !turnedTowardBase || !projectileHitboxMatched ||
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
    bool bombHitboxMatched = false;
    for (int step = 0; step < 900 &&
            bomberSession.GetPhase() == BattlePhase::Playing; ++step) {
        bomberSession.Update(kStep, {}, 0.0f, 0.0f);
        droppedBomb |= !bomberSession.GetProjectiles().empty();
        for (const BattleProjectile& projectile :
                bomberSession.GetProjectiles()) {
            for (const BattleHitbox& hitbox : bomberSession.GetHitboxes()) {
                if (hitbox.id == projectile.id &&
                    std::abs(hitbox.radius - combat.projectile.bombRadius) <
                        0.00001f) bombHitboxMatched = true;
            }
            bombMovedLaterally |= std::abs(projectile.position.x -
                projectile.forwardPosition.x) > 0.1f;
            bombStayedAtCombatHeight &= projectile.position.y >=
                combat.enemy.combatHeightMinimum && projectile.position.y <=
                combat.enemy.combatHeightMaximum;
        }
    }
    if (!droppedBomb || !bombMovedLaterally || !bombHitboxMatched ||
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
    bool spikeHitboxMatched = false;
    for (int step = 0; step < 180; ++step) {
        crystalSession.Update(kStep, {}, 0.0f, 0.0f);
        for (const BattleProjectile& projectile :
                crystalSession.GetProjectiles()) {
            if (projectile.type == BattleProjectileType::CrystalSpike &&
                projectile.position.y >= crystalCombat.enemy.combatHeightMinimum &&
                projectile.position.y <= crystalCombat.enemy.combatHeightMaximum &&
                std::abs(projectile.direction.y) < 0.00001f) {
                launchedFromSlot = true;
                for (const BattleHitbox& hitbox : crystalSession.GetHitboxes()) {
                    if (hitbox.id == projectile.id &&
                        std::abs(hitbox.radius -
                            crystalCombat.projectile.crystalSpikeRadius) <
                            0.00001f) spikeHitboxMatched = true;
                }
            }
        }
    }
    if (!launchedFromSlot || !spikeHitboxMatched) {
        std::puts("FAIL: Boss crystal did not follow original horizontal projectile flight");
        return 1;
    }
    BattleCombatConfig cubeCombat = crystalCombat;
    cubeCombat.boss.crystalSpikeInitialDelay = 100.0f;
    cubeCombat.boss.crystalCubeInitialDelay = 0.05f;
    cubeCombat.boss.crystalCubeInterval = 100.0f;
    FirstPersonGame cubePlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession cubeSession(BattlePreviewKind::Boss, cubePlayer,
        {}, cubeCombat);
    cubeSession.Start(true);
    bool cubeHitboxMatched = false;
    for (int step = 0; step < 180 && !cubeHitboxMatched; ++step) {
        cubeSession.Update(kStep, {}, 0.0f, 0.0f);
        for (const BattleProjectile& projectile : cubeSession.GetProjectiles()) {
            if (projectile.type != BattleProjectileType::CrystalCube ||
                projectile.position.y < cubeCombat.enemy.combatHeightMinimum ||
                projectile.position.y > cubeCombat.enemy.combatHeightMaximum ||
                std::abs(projectile.direction.y) > 0.00001f) continue;
            for (const BattleHitbox& hitbox : cubeSession.GetHitboxes()) {
                if (hitbox.id == projectile.id &&
                    std::abs(hitbox.radius -
                        cubeCombat.projectile.crystalCubeRadius) < 0.00001f)
                    cubeHitboxMatched = true;
            }
        }
    }
    if (!cubeHitboxMatched) {
        std::puts("FAIL: crystal cube center, altitude or collision radius");
        return 1;
    }
    FirstPersonGame attackPlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession attackSession(BattlePreviewKind::Practice, attackPlayer,
        {}, combat);
    GameInput attackCommand{};
    attackCommand.practiceCommand = PracticeCommand::BossSpawn;
    attackSession.Update(kStep, attackCommand, 0.0f, 0.0f);
    for (int step = 0; step < 120; ++step)
        attackSession.Update(kStep, {}, 0.0f, 0.0f);
    attackCommand.practiceCommand = PracticeCommand::BossConeCharge;
    attackSession.Update(kStep, attackCommand, 0.0f, 0.0f);
    bool activeBossHitbox = false;
    for (int step = 0; step < 360 && !activeBossHitbox; ++step) {
        attackSession.Update(kStep, {}, 0.0f, 0.0f);
        for (const BattleHitbox& hitbox : attackSession.GetHitboxes()) {
            if (hitbox.kind == BattleHitboxKind::BossAttack &&
                hitbox.radius > 0.0f &&
                std::isfinite(hitbox.previousCenter.x) &&
                std::isfinite(hitbox.center.x)) activeBossHitbox = true;
        }
    }
    attackSession.Reset();
    if (!activeBossHitbox || !attackSession.GetHitboxes().empty()) {
        std::puts("FAIL: active Boss hitbox or reset overlay state");
        return 1;
    }
    const auto practiceSnapshot = [&](int fps) {
        FirstPersonGame practicePlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
            std::vector<FirstPersonTarget>{});
        CombatSession practice(BattlePreviewKind::Practice, practicePlayer,
            {}, combat);
        FixedStepClock clock;
        FixedStepInputBuffer buffer;
        for (int frame = 0; frame < fps * 2; ++frame) {
            GameInput input{};
            if (frame == 0) input.practiceCommand = PracticeCommand::MissileUnits;
            buffer.QueueFrame(input, 0.0f, 0.0f);
            const int steps = clock.Advance(1.0 / fps);
            for (int step = 0; step < steps; ++step) {
                const SimulationStepInput fixed = buffer.ConsumeStep(steps - step);
                practice.Update(kStep, fixed.game, 0.0f, 0.0f);
            }
        }
        return std::pair{practice.GetEnemies().size(), practice.GetPhase()};
    };
    const auto practice30 = practiceSnapshot(30);
    const auto practice60 = practiceSnapshot(60);
    const auto practice120 = practiceSnapshot(120);
    if (practice30 != practice60 || practice60 != practice120 ||
        practice30.first != 2 || practice30.second != BattlePhase::Ready) {
        std::puts("FAIL: practice pattern differs by rendered FPS or entered battle flow");
        return 1;
    }
    if (!DefeatPracticeEnemyWithInput(30, combat) ||
        !DefeatPracticeEnemyWithInput(60, combat) ||
        !DefeatPracticeEnemyWithInput(120, combat)) {
        std::puts("FAIL: input-driven practice enemy hit differs by rendered FPS");
        return 1;
    }
    FirstPersonGame practiceHitPlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession practiceHit(BattlePreviewKind::Practice, practiceHitPlayer,
        {}, combat);
    GameInput shieldTest{};
    shieldTest.testProjectileHit.pressed = true;
    practiceHit.Update(kStep, shieldTest, 0.0f, 0.0f);
    const int shieldVisualCount = static_cast<int>(std::count_if(
        practiceHit.GetVisualEvents().begin(),
        practiceHit.GetVisualEvents().end(),
        [](const BattleVisualEvent& event) {
            return event.effect == BattleVisualEffect::ShieldHit &&
                event.shieldImpact == ShieldImpactState::TwoOrMoreRemaining;
        }));
    if (shieldVisualCount != 1 ||
        practiceHitPlayer.GetVitality().GetShieldLayers() != 2) {
        std::puts("FAIL: practice hit did not show one shield effect");
        return 1;
    }
    GameInput practiceHitInput{};
    practiceHitInput.practiceCommand = PracticeCommand::MissileUnit;
    practiceHit.Update(kStep, practiceHitInput, 0.0f, 0.0f);
    if (practiceHit.GetEnemies().size() != 1 ||
        !practiceHit.ApplyTargetDamage(practiceHit.GetEnemies().front().id, 1) ||
        !practiceHit.GetEnemies().front().defeated ||
        practiceHit.GetPhase() != BattlePhase::Ready) {
        std::puts("FAIL: practice enemy damage must work without entering battle flow");
        return 1;
    }
    practiceHitInput = {};
    practiceHitInput.practiceCommand = PracticeCommand::SingleMissile;
    practiceHit.Update(kStep, practiceHitInput, 0.0f, 0.0f);
    if (practiceHit.GetProjectiles().size() != 1 ||
        !practiceHit.ApplyTargetDamage(
            practiceHit.GetProjectiles().front().id, 1) ||
        practiceHit.GetProjectiles().front().age >= 0.0f) {
        std::puts("FAIL: practice projectile interception must apply damage");
        return 1;
    }
    const auto explosionCount = [&practiceHit]() {
        return std::count_if(practiceHit.GetVisualEvents().begin(),
            practiceHit.GetVisualEvents().end(),
            [](const BattleVisualEvent& event) {
                return event.effect == BattleVisualEffect::Explosion;
            });
    };
    if (explosionCount() != 1) {
        std::puts("FAIL: projectile intercept did not emit exactly one explosion");
        return 1;
    }
    practiceHit.Update(kStep, {}, 0.0f, 0.0f);
    if (explosionCount() != 0) {
        std::puts("FAIL: projectile intercept repeated its explosion");
        return 1;
    }
    practiceHitInput = {};
    practiceHitInput.practiceCommand = PracticeCommand::BossSpawn;
    practiceHit.Update(kStep, practiceHitInput, 0.0f, 0.0f);
    const int bossHitPoints = practiceHit.GetBoss()
        ? practiceHit.GetBoss()->GetHitPoints() : -1;
    if (bossHitPoints < 1 ||
        !practiceHit.ApplyTargetDamage("target_boss_core", 1) ||
        !practiceHit.GetBoss() ||
        practiceHit.GetBoss()->GetHitPoints() != bossHitPoints - 1) {
        std::puts("FAIL: practice Boss core must receive damage");
        return 1;
    }
    practiceHit.Reset();
    practiceHitInput.practiceCommand = PracticeCommand::MissileUnit;
    practiceHit.Update(kStep, practiceHitInput, 0.0f, 0.0f);
    if (practiceHit.GetEnemies().size() != 1 ||
        practiceHit.GetEnemies().front().defeated ||
        !practiceHit.ApplyTargetDamage(practiceHit.GetEnemies().front().id, 1) ||
        !practiceHit.GetEnemies().front().defeated ||
        practiceHit.GetPhase() != BattlePhase::Ready) {
        std::puts("FAIL: practice reset retained hit state or blocked a new enemy");
        return 1;
    }
    FirstPersonGame practiceBomberPlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession bomberPractice(BattlePreviewKind::Practice, practiceBomberPlayer, {}, combat);
    GameInput bomberInput{};
    bomberInput.practiceCommand = PracticeCommand::Bombers;
    bomberPractice.Update(kStep, bomberInput, 0.0f, 0.0f);
    for (int index = 0; index < 24; ++index)
        bomberPractice.Update(kStep, {}, 0.0f, 0.0f);
    const auto& practiceBombers = bomberPractice.GetEnemies();
    if (practiceBombers.size() != 2 ||
        std::abs(practiceBombers[0].rotation) > 0.0001f ||
        std::abs(practiceBombers[1].rotation - 3.14159265f) > 0.0001f ||
        practiceBombers[1].attackTimer <= practiceBombers[0].attackTimer) {
        std::puts("FAIL: Bomber practice rotation or staggered attack timing differs");
        return 1;
    }
    FirstPersonGame practicePlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession practice(BattlePreviewKind::Practice, practicePlayer, {}, combat);
    GameInput practiceInput{};
    practiceInput.practiceCommand = PracticeCommand::MissileUnits;
    practice.Update(kStep, practiceInput, 0.0f, 0.0f);
    practiceInput.practiceCommand = PracticeCommand::MixedProjectiles;
    practice.Update(kStep, practiceInput, 0.0f, 0.0f);
    for (int index = 0; index < 60; ++index)
        practice.Update(kStep, {}, 0.0f, 0.0f);
    if (practice.GetEnemies().size() != 2 || practice.GetProjectiles().empty() ||
        practice.GetPhase() != BattlePhase::Ready) {
        std::puts("FAIL: enemy and projectile practice patterns did not coexist");
        return 1;
    }
    practiceInput.practiceCommand = PracticeCommand::BossSpawn;
    practice.Update(kStep, practiceInput, 0.0f, 0.0f);
    if (!practice.GetBoss()) {
        std::puts("FAIL: practice Boss did not spawn");
        return 1;
    }
    for (int step = 0; step < 120; ++step)
        practice.Update(kStep, {}, 0.0f, 0.0f);
    if (!practice.GetBoss() ||
        practice.GetBoss()->GetMode() != BossMode::Idle) {
        std::puts("FAIL: practice Boss did not finish entrance");
        return 1;
    }
    practiceInput.practiceCommand = PracticeCommand::BossCycleNeedles;
    practice.Update(kStep, practiceInput, 0.0f, 0.0f);
    const auto countPoseCues = [&practice]() {
        return std::count(practice.GetFeedbackEvents().begin(),
            practice.GetFeedbackEvents().end(),
            BattleFeedbackEvent::BossNeedlePoseChanged);
    };
    if (practice.GetBoss()->GetNeedlePose() !=
            BossNeedlePose::NeedleStrike || countPoseCues() != 1) {
        std::puts("FAIL: practice needle pose did not emit one pose cue");
        return 1;
    }
    practiceInput.practiceCommand = PracticeCommand::BossBoxer;
    practice.Update(kStep, practiceInput, 0.0f, 0.0f);
    if (countPoseCues() != 0) {
        std::puts("FAIL: behavior change replayed the unchanged needle pose cue");
        return 1;
    }
    practiceInput = {};
    practiceInput.resetTarget.pressed = true;
    practice.Update(kStep, practiceInput, 0.0f, 0.0f);
    if (practice.GetBoss() || !practice.GetEnemies().empty() ||
        !practice.GetProjectiles().empty() ||
        practice.GetPhase() != BattlePhase::Ready) {
        std::puts("FAIL: practice reset retained test objects or changed flow");
        return 1;
    }
    FirstPersonGame chargePracticePlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession chargePractice(BattlePreviewKind::Practice, chargePracticePlayer,
        {}, combat);
    GameInput chargeInput{};
    chargeInput.practiceCommand = PracticeCommand::BossSpawn;
    chargePractice.Update(kStep, chargeInput, 0.0f, 0.0f);
    for (int step = 0; step < 120; ++step) {
        chargePractice.Update(kStep, {}, 0.0f, 0.0f);
    }
    const int shieldBeforeCharge =
        chargePractice.GetPlayerGame().GetVitality().GetShieldLayers();
    const int hitPointsBeforeCharge =
        chargePractice.GetPlayerGame().GetVitality().GetHitPoints();
    chargeInput.practiceCommand = PracticeCommand::BossConeCharge;
    chargePractice.Update(kStep, chargeInput, 0.0f, 0.0f);
    bool chargeReturnedToIdle = false;
    int chargeLaunchCues = 0;
    for (int step = 0; step < 240; ++step) {
        chargePractice.Update(kStep, {}, 0.0f, 0.0f);
        chargeLaunchCues += static_cast<int>(std::count(
            chargePractice.GetFeedbackEvents().begin(),
            chargePractice.GetFeedbackEvents().end(),
            BattleFeedbackEvent::BossChargeLaunched));
        if (chargePractice.GetBoss() &&
            chargePractice.GetBoss()->GetMode() == BossMode::Idle) {
            chargeReturnedToIdle = true;
            break;
        }
    }
    if (chargePractice.GetPlayerGame().GetVitality().GetShieldLayers() !=
            shieldBeforeCharge - 3 ||
        chargePractice.GetPlayerGame().GetVitality().GetHitPoints() !=
            hitPointsBeforeCharge ||
        !chargeReturnedToIdle || chargeLaunchCues != 1 ||
        !chargePractice.GetBoss() ||
        chargePractice.GetBoss()->GetMode() != BossMode::Idle) {
        std::puts("FAIL: Boss Charge tip did not hit once and end the sequence");
        return 1;
    }
    std::puts("PASS: fixed timing, Day1/Day2/Boss Clear, reset/countdown, Charge impact, Missile windup and Bomber");
    return 0;
}
