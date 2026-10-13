#include "Data/BattleCombatConfig.h"
#include "Data/BattleAssetCatalog.h"
#include "Game/FirstPersonGame.h"
#include "Gameplay/Combat/CombatSession.h"
#include "Gameplay/Combat/EncounterConfig.h"
#include "Gameplay/Cockpit/CockpitPresentation.h"
#include "Gameplay/Player/FirstPersonTuning.h"
#include "Platform/FixedStepClock.h"
#include "Rendering/FirstPersonScenePresenter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <string_view>

namespace {
constexpr float kStep = 1.0f / 60.0f;

struct ReplayResult {
    BattlePhase phase = BattlePhase::Ready;
    int wave = 0;
    int bossHitPoints = 0;
    int baseHitPoints = 0;
    int playerHitPoints = 0;
    int steps = 0;
    int attacksLaunched = 0;
};

bool VerifyCockpitMount() {
    FirstPersonGame player(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    BattleAssetCatalog assets;
    ModelInstance prototype{};
    assets.Add("cockpit", prototype);
    assets.Add("helmet", prototype);
    FirstPersonScenePresenter presenter(ModelScene{});
    const auto check = [&](CockpitActivation activation, bool emergency,
            float expectedHeight) {
        presenter.Sync(player);
        presenter.SyncCockpit(player, assets, true, activation, 0, emergency);
        const ModelScene& scene = presenter.GetScene();
        const ModelInstance* cockpit = FindModelById(scene, "fullgame_cockpit");
        const ModelInstance* helmet = FindModelById(scene, "fullgame_helmet");
        return cockpit && helmet &&
            std::abs(scene.cameraPosition.y - expectedHeight) < 0.0001f &&
            std::abs(cockpit->position.y - expectedHeight) < 0.0001f &&
            std::abs(helmet->position.y - expectedHeight) < 0.0001f &&
            std::abs((scene.cameraTarget.y - scene.cameraPosition.y) -
                (player.GetCamera().GetTarget().y -
                    player.GetCamera().GetPosition().y)) < 0.0001f;
    };
    return check({0.0f, 0.0f, 0.0f, 0.0f}, false, 2.95f) &&
        check({0.5f, 0.25f, 0.5f, 0.5f}, false, 4.325f) &&
        check({}, false, 5.85f) &&
        check({0.0f, 0.0f, 0.0f, 0.0f}, true, 5.20f) &&
        check({}, true, 5.85f);
}

bool VerifyTutorialProjectileModel() {
    FirstPersonGame player(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    if (!player.SpawnTarget({"target_tutorial_missile",
            {0.0f, 4.2f, 18.0f}, 1, {}, false, 0.45f,
            1.0f, TargetDisplayKind::Projectile})) return false;
    BattleAssetCatalog assets;
    ModelInstance target{};
    target.meshIndex = 41;
    ModelInstance missile{};
    missile.meshIndex = 42;
    ModelInstance flame{};
    flame.meshIndex = 43;
    assets.Add("tutorial_target", target);
    assets.Add("missile", missile);
    assets.Add("flame", flame);
    FirstPersonScenePresenter presenter(ModelScene{});
    presenter.Sync(player);
    presenter.SyncTutorial(player, assets, false);
    const ModelScene& scene = presenter.GetScene();
    const ModelInstance* body = FindModelById(scene,
        "tutorial_target_tutorial_missile");
    const ModelInstance* tail = FindModelById(scene,
        "tutorial_missile_tail");
    return body && body->meshIndex == 41 &&
        std::abs(body->scale - 0.45f / 1.25f) < 0.0001f &&
        tail && tail->meshIndex == 43 &&
        tail->renderLayer == ModelRenderLayer::AdditiveEffect;
}

bool VerifyTutorialVisualEffects() {
    FirstPersonGame player(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    BattleAssetCatalog assets;
    ModelInstance blueShield{};
    blueShield.meshIndex = 11;
    ModelInstance yellowShield{};
    yellowShield.meshIndex = 12;
    ModelInstance redShield{};
    redShield.meshIndex = 13;
    assets.Add("shield_three", blueShield);
    assets.Add("shield_two", yellowShield);
    assets.Add("shield_one", redShield);
    for (const char* key : {"explosion_flash", "explosion_fire",
            "explosion_smoke"}) assets.Add(key, ModelInstance{});
    FirstPersonScenePresenter presenter(ModelScene{});
    presenter.QueueVisualEvents({
        {BattleVisualEffect::ShieldHit, player.GetPlayer().GetPosition(), 1.0f,
            ShieldImpactState::TwoOrMoreRemaining},
        {BattleVisualEffect::ShieldHit, player.GetPlayer().GetPosition(), 1.0f,
            ShieldImpactState::OneRemaining},
        {BattleVisualEffect::ShieldHit, player.GetPlayer().GetPosition(), 1.0f,
            ShieldImpactState::Broken},
        {BattleVisualEffect::Explosion, {0.0f, 4.2f, 8.0f}, 0.55f}});
    presenter.SyncTutorial(player, assets, false);
    const ModelScene& scene = presenter.GetScene();
    const ModelInstance* blue = FindModelById(scene, "battle_effect_1");
    const ModelInstance* yellow = FindModelById(scene, "battle_effect_2");
    const ModelInstance* red = FindModelById(scene, "battle_effect_3");
    const ModelInstance* explosion = FindModelById(scene, "battle_effect_4_flash");
    if (!blue || !yellow || !red || !explosion ||
        blue->meshIndex != 11 || yellow->meshIndex != 12 ||
        red->meshIndex != 13 ||
        blue->renderLayer != ModelRenderLayer::AdditiveEffect ||
        yellow->renderLayer != ModelRenderLayer::AdditiveEffect ||
        red->renderLayer != ModelRenderLayer::AdditiveEffect ||
        explosion->renderLayer != ModelRenderLayer::AdditiveEffect) return false;
    presenter.AdvanceVisualTime(1.0);
    presenter.SyncTutorial(player, assets, false);
    return !FindModelById(presenter.GetScene(), "battle_effect_1") &&
        !FindModelById(presenter.GetScene(), "battle_effect_2") &&
        !FindModelById(presenter.GetScene(), "battle_effect_3") &&
        !FindModelById(presenter.GetScene(), "battle_effect_4_flash");
}

bool VerifyPracticeBossFinalEffect(const BattleCombatConfig& combat) {
    FirstPersonGame player(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession session(BattlePreviewKind::Practice, player, {}, combat);
    GameInput spawn{};
    spawn.practiceCommand = PracticeCommand::BossSpawn;
    session.Update(kStep, spawn, 0.0f, 0.0f);
    if (!session.GetBoss()) return false;
    BattleAssetCatalog assets;
    for (const char* key : {"boss_core", "boss_cone_left",
            "boss_cone_right", "boss_cone_lower", "explosion_flash",
            "explosion_fire", "explosion_smoke"}) {
        assets.Add(key, ModelInstance{});
    }
    FirstPersonScenePresenter presenter(ModelScene{});
    presenter.SyncBattle(session, assets);
    if (!FindModelById(presenter.GetScene(), "battle_boss_core")) {
        std::puts("FAIL: practice Boss core was not visible before defeat");
        return false;
    }
    if (!session.ApplyTargetDamage("target_boss_core",
            session.GetBoss()->GetHitPoints())) {
        std::puts("FAIL: practice Boss core rejected the first hit");
        return false;
    }
    for (int tick = 0; tick < 600; ++tick) {
        session.Update(kStep, {}, 0.0f, 0.0f);
        for (const BattleProjectile& projectile : session.GetProjectiles())
            session.ApplyTargetDamage(projectile.id, 1);
    }
    if (!session.ApplyTargetDamage("target_boss_core", 1)) return false;
    int finalExplosions = 0;
    bool sawFinalEffect = false;
    for (int tick = 0; tick < 600; ++tick) {
        session.Update(kStep, {}, 0.0f, 0.0f);
        presenter.AdvanceVisualTime(kStep);
        const auto& events = session.GetVisualEvents();
        bool finalEventThisTick = false;
        for (const BattleVisualEvent& event : events) {
            if (event.effect == BattleVisualEffect::Explosion &&
                event.scale > 3.0f) {
                ++finalExplosions;
                finalEventThisTick = true;
            }
        }
        presenter.QueueVisualEvents(events);
        presenter.SyncBattle(session, assets);
        if (session.GetBoss()->GetMode() == BossMode::Defeated) {
            sawFinalEffect |= finalEventThisTick &&
                std::any_of(presenter.GetScene().models.begin(),
                    presenter.GetScene().models.end(),
                    [](const ModelInstance& model) {
                        return model.id.starts_with("battle_effect_") &&
                            model.scale > 1.0f;
                    });
            if (FindModelById(presenter.GetScene(), "battle_boss_core") ||
                FindModelById(presenter.GetScene(), "battle_boss_left_cone") ||
                session.GetPhase() != BattlePhase::Ready) {
                std::puts("FAIL: defeated practice Boss still blocks its explosion");
                return false;
            }
        }
    }
    const bool effectsExpired = std::none_of(presenter.GetScene().models.begin(),
        presenter.GetScene().models.end(), [](const ModelInstance& model) {
            return model.id.starts_with("battle_effect_");
        });
    return finalExplosions == 1 && sawFinalEffect && effectsExpired;
}

bool VerifyEnemyInterpolation(const BattleCombatConfig& combat) {
    FirstPersonGame player(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession session(BattlePreviewKind::MissileUnit, player, {}, combat);
    session.Start(true);
    BattleAssetCatalog assets;
    ModelInstance prototype{};
    prototype.initialXDegrees = 0.0f;
    prototype.initialYDegrees = 0.0f;
    prototype.initialZDegrees = 0.0f;
    prototype.rotationXDegreesPerSecond = 0.0f;
    prototype.rotationYDegreesPerSecond = 0.0f;
    assets.Add("missile_unit", prototype);
    assets.Add("missile", prototype);
    FirstPersonScenePresenter presenter(ModelScene{});
    bool enemyChecked = false;
    for (int tick = 0; tick < 300; ++tick) {
        session.Update(kStep, {}, 0.0f, 0.0f);
        const BattleEnemy& enemy = session.GetEnemies().front();
        if (!enemy.deployed ||
            std::abs(enemy.visualYaw - enemy.previousVisualYaw) < 0.00001f ||
            std::abs(enemy.position.x - enemy.previousPosition.x) < 0.00001f)
            continue;
        for (float alpha : {0.0f, 0.5f, 1.0f}) {
            presenter.SyncBattle(session, assets, alpha);
            const ModelInstance* model = FindModelById(
                presenter.GetScene(), "battle_" + enemy.id);
            const float expectedX = enemy.previousPosition.x +
                (enemy.position.x - enemy.previousPosition.x) * alpha;
            const float expectedYaw = enemy.previousVisualYaw +
                std::remainder(enemy.visualYaw - enemy.previousVisualYaw,
                    2.0f * 3.14159265f) * alpha;
            if (!model || std::abs(model->position.x - expectedX) > 0.001f ||
                std::abs(model->initialYDegrees -
                    expectedYaw * 180.0f / 3.14159265f) > 0.02f)
                return false;
        }
        enemyChecked = true;
        break;
    }
    if (!enemyChecked) return false;
    for (int tick = 0; tick < 900; ++tick) {
        session.Update(kStep, {}, 0.0f, 0.0f);
        for (const BattleProjectile& projectile : session.GetProjectiles()) {
            if (projectile.type != BattleProjectileType::Missile ||
                std::abs(projectile.direction.x -
                    projectile.previousDirection.x) < 0.00001f) continue;
            for (float alpha : {0.0f, 0.5f, 1.0f}) {
                presenter.SyncBattle(session, assets, alpha);
                const ModelInstance* model = FindModelById(
                    presenter.GetScene(), "battle_" + projectile.id);
                const float expectedX = projectile.previousPosition.x +
                    (projectile.position.x -
                        projectile.previousPosition.x) * alpha;
                const Vector3 direction{
                    projectile.previousDirection.x +
                        (projectile.direction.x -
                            projectile.previousDirection.x) * alpha,
                    0.0f,
                    projectile.previousDirection.z +
                        (projectile.direction.z -
                            projectile.previousDirection.z) * alpha};
                const float expectedYaw = (std::atan2(direction.x,
                    direction.z) + 3.14159265f) * 180.0f / 3.14159265f;
                if (!model ||
                    std::abs(model->position.x - expectedX) > 0.001f ||
                    std::abs(model->initialYDegrees - expectedYaw) > 0.02f)
                    return false;
            }
            return true;
        }
    }
    return false;
}

bool VerifyBossHierarchy(const BattleCombatConfig& combat) {
    FirstPersonGame player(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession session(BattlePreviewKind::Practice, player, {}, combat);
    GameInput input{};
    input.practiceCommand = PracticeCommand::BossSpawn;
    session.Update(kStep, input, 0.0f, 0.0f);
    if (!session.GetBoss()) return false;
    BattleAssetCatalog assets;
    ModelInstance prototype;
    prototype.initialXDegrees = 0.0f;
    prototype.initialYDegrees = 0.0f;
    prototype.initialZDegrees = 0.0f;
    prototype.rotationXDegreesPerSecond = 0.0f;
    prototype.rotationYDegreesPerSecond = 0.0f;
    for (const char* key : {"boss_core", "boss_cone_left",
            "boss_cone_right", "boss_cone_lower",
            "boss_crystal_spike", "boss_crystal_cube"}) {
        assets.Add(key, prototype);
    }
    FirstPersonScenePresenter presenter(ModelScene{});
    const auto worldOf = [&](std::string_view id, Matrix4x4& world) {
        const ModelInstance* model = FindModelById(presenter.GetScene(), id);
        return model && TryBuildModelWorldTransform(
            presenter.GetScene(), *model, world);
    };
    const auto sameOrigin = [](const Matrix4x4& actual,
            const Matrix4x4& expected) {
        return std::abs(actual.elements[3][0] -
                expected.elements[3][0]) < 0.002f &&
            std::abs(actual.elements[3][1] -
                expected.elements[3][1]) < 0.002f &&
            std::abs(actual.elements[3][2] -
                expected.elements[3][2]) < 0.002f;
    };
    const auto checkPose = [&] {
        presenter.SyncBattle(session, assets);
        const BattleBoss& boss = *session.GetBoss();
        const BossVisualPose pose = boss.GetVisualPose();
        const Vector3& position = boss.GetPosition();
        const Matrix4x4 root = MultiplyMatrices(
            MultiplyMatrices(MultiplyMatrices(
                MakeRotationZMatrix(pose.rootRotation.z),
                MakeRotationXMatrix(pose.rootRotation.x)),
                MakeRotationYMatrix(pose.rootRotation.y)),
            MakeTranslationMatrix(position.x, position.y, position.z));
        Matrix4x4 core{};
        Matrix4x4 cone{};
        Matrix4x4 spike{};
        Matrix4x4 cube{};
        if (!worldOf("battle_boss_core", core) ||
            !worldOf("battle_boss_left_cone", cone) ||
            !worldOf("battle_boss_spike_slot_0", spike) ||
            !worldOf("battle_boss_cube_slot_0", cube)) {
            return false;
        }
        const BossPartPose& left = pose.cones[0];
        const Matrix4x4 partRotation = MultiplyMatrices(
            MultiplyMatrices(MakeRotationZMatrix(left.rotation.z),
                MakeRotationXMatrix(left.rotation.x)),
            MakeRotationYMatrix(left.rotation.y));
        const Matrix4x4 expectedCone = MultiplyMatrices(
            MultiplyMatrices(MakeTranslationMatrix(left.translation.x,
                left.translation.y, left.translation.z), partRotation),
            MultiplyMatrices(MakeTranslationMatrix(left.rootTranslation.x,
                left.rootTranslation.y, left.rootTranslation.z), root));
        const Matrix4x4 expectedSpike = MultiplyMatrices(
            MakeTranslationMatrix(pose.spikes[0].translation.x,
                pose.spikes[0].translation.y,
                pose.spikes[0].translation.z), root);
        const Matrix4x4 expectedCube = MultiplyMatrices(
            MakeTranslationMatrix(pose.cubes[0].translation.x,
                pose.cubes[0].translation.y,
                pose.cubes[0].translation.z), root);
        return sameOrigin(core, root) &&
            sameOrigin(cone, expectedCone) &&
            sameOrigin(spike, expectedSpike) &&
            sameOrigin(cube, expectedCube);
    };
    if (!checkPose()) return false;
    for (int step = 0; step < 120; ++step)
        session.Update(kStep, {}, 0.0f, 0.0f);
    input.practiceCommand = PracticeCommand::BossCycleNeedles;
    session.Update(kStep, input, 0.0f, 0.0f);
    for (int step = 0; step < 30; ++step)
        session.Update(kStep, {}, 0.0f, 0.0f);
    return session.GetBoss() && checkPose();
}

ReplayResult Play(BattlePreviewKind kind, int renderFps,
    const EncounterConfig& encounter, const BattleCombatConfig& combat) {
    FirstPersonGame player(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession session(kind, player, encounter, combat);
    FixedStepClock clock;
    GameInput start{};
    start.battleStart.pressed = true;
    session.Update(kStep, start, 0.0f, 0.0f);
    int attacks = 0;
    int steps = 0;
    bool sawBattleModel = false;
    bool sawReadyUltimateModel = false;
    bool sawFlyingUltimateFlame = false;
    BattleAssetCatalog assets;
    for (const char* key : {"missile_unit", "bomber", "charge_unit",
            "missile", "floating_bomb", "boss_core", "boss_cone_left",
            "boss_cone_right", "boss_cone_lower", "boss_crystal_spike",
            "boss_crystal_cube", "flame", "ultimate_fist"}) {
        assets.Add(key, ModelInstance{});
    }
    FirstPersonScenePresenter presenter(ModelScene{});
    std::string selectedId;
    bool dodgeDown = false;
    int boxerWindupSteps = -1;
    for (int frame = 0; frame < renderFps * 240 &&
            session.GetPhase() != BattlePhase::Clear &&
            session.GetPhase() != BattlePhase::GameOver; ++frame) {
        const int count = clock.Advance(1.0 / renderFps);
        for (int index = 0; index < count &&
                session.GetPhase() != BattlePhase::Clear &&
                session.GetPhase() != BattlePhase::GameOver; ++index) {
            GameInput input{};
            if (boxerWindupSteps >= 0) ++boxerWindupSteps;
            if (kind == BattlePreviewKind::Day2 ||
                kind == BattlePreviewKind::Boss) {
                input.moveX = ((steps / 120) % 2 == 0) ? 1.0f : -1.0f;
                bool chargeThreat = false;
                for (const BattleEnemy& enemy : session.GetEnemies()) {
                    if (!enemy.defeated && enemy.type ==
                            BattleEnemyType::ChargeUnit &&
                        enemy.chargeState == BattleEnemy::ChargeState::Charging) {
                        const Vector3& playerPosition =
                            player.GetPlayer().GetPosition();
                        chargeThreat |= std::hypot(enemy.position.x -
                            playerPosition.x, enemy.position.z -
                            playerPosition.z) < 14.0f;
                    }
                }
                if (kind == BattlePreviewKind::Boss && session.GetBoss()) {
                    const BattleBoss& boss = *session.GetBoss();
                    const Vector3& playerPosition =
                        player.GetPlayer().GetPosition();
                    if (boss.GetMode() == BossMode::Boxer ||
                        boss.GetMode() == BossMode::NeedleCombo) {
                        input.moveY = -1.0f;
                    }
                    chargeThreat |= (boss.GetMode() == BossMode::Charge ||
                        boss.GetMode() == BossMode::NeedleCombo) &&
                        std::hypot(boss.GetPosition().x - playerPosition.x,
                            boss.GetPosition().z - playerPosition.z) < 12.0f;
                    chargeThreat |= boss.GetMode() == BossMode::Boxer &&
                        boxerWindupSteps == 12;
                }
                const bool canDodge = player.GetPlayer().GetDodgeState() ==
                    PlayerDodgeState::Ready;
                input.dodge.down = chargeThreat && canDodge;
                input.dodge.pressed = input.dodge.down && !dodgeDown;
                dodgeDown = input.dodge.down;
            }
            Vector3 target{};
            bool hasTarget = false;
            if (kind == BattlePreviewKind::Boss && session.GetBoss()) {
                target = session.GetBoss()->GetPosition();
                hasTarget = true;
            } else {
                for (const BattleEnemy& enemy : session.GetEnemies()) {
                    if (!enemy.defeated && enemy.deployed &&
                        enemy.id == selectedId) {
                        target = enemy.position;
                        hasTarget = true;
                        break;
                    }
                }
                if (!hasTarget) {
                    selectedId.clear();
                    for (const BattleEnemyType type : {
                            BattleEnemyType::MissileUnit,
                            BattleEnemyType::Bomber,
                            BattleEnemyType::ChargeUnit}) {
                        for (const BattleEnemy& enemy : session.GetEnemies()) {
                            if (!enemy.defeated && enemy.deployed &&
                                enemy.type == type) {
                                target = enemy.position;
                                hasTarget = true;
                                selectedId = enemy.id;
                                break;
                            }
                        }
                        if (hasTarget) {
                            break;
                        }
                    }
                }
            }
            float mouseX = 0.0f;
            if (hasTarget) {
                const Vector3 origin = player.GetPlayer().GetPosition();
                float yaw = std::atan2(target.x - origin.x,
                    target.z - origin.z);
                if (kind != BattlePreviewKind::Boss) {
                    const float distance = std::hypot(target.x - origin.x,
                        target.z - origin.z);
                    const PunchArmState left = player.GetLeftArm().GetState();
                    const bool useLeft = left == PunchArmState::Ready ||
                        left == PunchArmState::Charging ||
                        left == PunchArmState::Charged;
                    yaw += (useLeft ? 1.0f : -1.0f) *
                        std::atan2(FirstPersonTuning::kShoulderHalfWidth,
                            std::max(1.0f, distance - 4.0f));
                }
                const float error = std::remainder(yaw - player.GetCamera().GetYaw(),
                    6.283185307f);
                mouseX = std::clamp(error /
                    FirstPersonTuning::kMouseLookSensitivity, -75.0f, 75.0f);
                const float bodyError = std::abs(std::remainder(yaw -
                    player.GetPlayer().GetBodyYaw(), 6.283185307f));
                if (kind == BattlePreviewKind::Boss) {
                    if (bodyError < 0.045f &&
                        player.GetUltimateState() == UltimateState::Idle) {
                        input.leftMouseDown = true;
                        input.rightMouseDown = true;
                    }
                    if (player.GetUltimateState() == UltimateState::Forming ||
                        player.GetUltimateState() == UltimateState::Ready) {
                        input.leftMouseDown = true;
                        input.rightMouseDown = true;
                    }
                    if (player.GetUltimateState() == UltimateState::Ready) {
                        input.leftMouseDown = false;
                        input.rightMouseDown = false;
                        ++attacks;
                    }
                } else if (bodyError < 0.055f) {
                    const PunchArmState left = player.GetLeftArm().GetState();
                    const PunchArmState right = player.GetRightArm().GetState();
                    if (left == PunchArmState::Charging ||
                        left == PunchArmState::Ready) {
                        input.leftMouseDown = true;
                    } else if (left == PunchArmState::Charged) {
                        ++attacks;
                    }
                    if (left != PunchArmState::Ready &&
                        left != PunchArmState::Charging &&
                        left != PunchArmState::Charged) {
                        if (right == PunchArmState::Charging ||
                            right == PunchArmState::Ready) {
                            input.rightMouseDown = true;
                        } else if (right == PunchArmState::Charged) {
                            ++attacks;
                        }
                    }
                }
            }
            session.Update(kStep, input, mouseX, 0.0f);
            ++steps;
            if (kind == BattlePreviewKind::Boss && session.GetBoss() &&
                session.GetBoss()->DidBeginStrikeThisStep()) {
                boxerWindupSteps = 0;
            }
        if (kind == BattlePreviewKind::Boss &&
            ((player.GetUltimateState() == UltimateState::Ready &&
                    !sawReadyUltimateModel) ||
                (player.GetUltimateState() == UltimateState::Flying &&
                    !sawFlyingUltimateFlame))) {
            presenter.SyncBattle(session, assets);
            const auto hasModel = [&](const char* id) {
                return std::any_of(presenter.GetScene().models.begin(),
                    presenter.GetScene().models.end(),
                    [&](const ModelInstance& model) { return model.id == id; });
            };
            sawReadyUltimateModel |= player.GetUltimateState() ==
                UltimateState::Ready && hasModel("battle_ultimate");
            sawFlyingUltimateFlame |= player.GetUltimateState() ==
                UltimateState::Flying && hasModel("battle_ultimate_flame");
        }
        if (steps % 120 == 0 && !sawBattleModel) {
            presenter.SyncBattle(session, assets);
            sawBattleModel = std::any_of(presenter.GetScene().models.begin(),
                presenter.GetScene().models.end(), [](const ModelInstance& model) {
                    return model.id.starts_with("battle_");
                });
        }
        }
    }
    presenter.SyncBattle(session, assets);
    const bool hasEndBattleModel = std::any_of(
        presenter.GetScene().models.begin(),
        presenter.GetScene().models.end(), [](const ModelInstance& model) {
            return model.id.starts_with("battle_");
        });
    const bool hasEndHalo = std::any_of(
        presenter.GetScene().models.begin(),
        presenter.GetScene().models.end(), [](const ModelInstance& model) {
            return model.id.starts_with("battle_boss_spike_slot_") ||
                model.id.starts_with("battle_boss_cube_slot_");
        });
    const bool hasEndBossBody = FindModelById(
        presenter.GetScene(), "battle_boss_core") != nullptr ||
        FindModelById(presenter.GetScene(), "battle_boss_left_cone") != nullptr;
    if (!sawBattleModel || (session.GetPhase() == BattlePhase::Clear &&
            (kind == BattlePreviewKind::Boss
                ? hasEndBossBody || hasEndHalo : hasEndBattleModel)) ||
        (kind == BattlePreviewKind::Boss &&
            (!sawReadyUltimateModel || !sawFlyingUltimateFlame))) {
        std::printf("FAIL: battle scene state saw=%d end=%d halo=%d ready=%d flame=%d kind=%d phase=%d\n",
            sawBattleModel, hasEndBattleModel, hasEndHalo,
            sawReadyUltimateModel, sawFlyingUltimateFlame,
            static_cast<int>(kind), static_cast<int>(session.GetPhase()));
        return {};
    }
    ReplayResult result;
    result.phase = session.GetPhase();
    result.wave = session.GetWaveIndex();
    result.bossHitPoints = session.GetBoss()
        ? session.GetBoss()->GetHitPoints() : 0;
    result.baseHitPoints = session.GetBaseHitPoints();
    result.playerHitPoints = player.GetVitality().GetHitPoints();
    result.steps = steps;
    result.attacksLaunched = attacks;
    std::printf("kind=%d fps=%d phase=%d wave=%d boss=%d base=%d player=%d steps=%d attacks=%d\n",
        static_cast<int>(kind), renderFps, static_cast<int>(result.phase),
        result.wave, result.bossHitPoints, result.baseHitPoints,
        result.playerHitPoints, steps, attacks);
    return result;
}

bool VerifyUltimateFormationPose() {
    FirstPersonGame player(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    CombatSession session(BattlePreviewKind::Practice, player);
    ModelScene scene;
    for (const char* id : {"left_fist", "right_fist",
            "left_upper_arm", "right_upper_arm"}) {
        ModelInstance model;
        model.id = id;
        scene.models.push_back(model);
    }
    FirstPersonScenePresenter presenter(std::move(scene));
    GameInput charge{};
    charge.leftMouseDown = true;
    charge.rightMouseDown = true;
    float initialArmSeparation = -1.0f;
    float middleArmSeparation = -1.0f;
    float finalArmSeparation = -1.0f;
    float middleFistSeparation = -1.0f;
    float finalFistSeparation = -1.0f;
    for (int tick = 0; tick < 400; ++tick) {
        session.Update(kStep, charge, 0.0f, 0.0f);
        if (player.GetUltimateState() != UltimateState::Forming)
            continue;
        const float ratio = player.GetUltimateFormationRatio();
        presenter.Sync(player);
        const ModelScene& pose = presenter.GetScene();
        const ModelInstance* leftArm = FindModelById(pose, "left_upper_arm");
        const ModelInstance* rightArm = FindModelById(pose, "right_upper_arm");
        const ModelInstance* leftFist = FindModelById(pose, "left_fist");
        const ModelInstance* rightFist = FindModelById(pose, "right_fist");
        if (!leftArm || !rightArm || !leftFist || !rightFist)
            return false;
        const float armSeparation = rightArm->position.x -
            leftArm->position.x;
        const float fistSeparation = rightFist->position.x -
            leftFist->position.x;
        if (ratio == 0.0f && initialArmSeparation < 0.0f)
            initialArmSeparation = armSeparation;
        if (ratio >= 0.45f && ratio < 0.55f) {
            middleArmSeparation = armSeparation;
            middleFistSeparation = fistSeparation;
        }
        if (ratio >= 0.95f) {
            finalArmSeparation = armSeparation;
            finalFistSeparation = fistSeparation;
            break;
        }
    }
    if (!(initialArmSeparation > middleArmSeparation &&
            middleArmSeparation > finalArmSeparation &&
            middleFistSeparation > finalFistSeparation))
        return false;
    GameInput cancel{};
    session.Update(kStep, cancel, 0.0f, 0.0f);
    presenter.Sync(player);
    return player.GetUltimateState() == UltimateState::Idle &&
        player.GetUltimateFormationRatio() == 0.0f;
}
}

int main(int argc, char** argv) {
    if (argc != 2) {
        return 1;
    }
    const std::filesystem::path configDirectory = argv[1];
    EncounterConfig day1;
    EncounterConfig day2;
    BattleCombatConfig combat;
    std::string error;
    if (!LoadEncounterConfig(configDirectory / "day1-encounter.cfg", day1,
            error) || !LoadEncounterConfig(configDirectory /
            "day2-encounter.cfg", day2, error) ||
        !LoadBattleCombatConfig(configDirectory, combat, error)) {
        std::printf("FAIL: %s\n", error.c_str());
        return 1;
    }
    if (!VerifyBossHierarchy(combat)) {
        std::puts("FAIL: Boss core, cone or crystal parent transform differed from the original hierarchy");
        return 1;
    }
    if (!VerifyUltimateFormationPose()) {
        std::puts("FAIL: Ultimate fists and upper arms did not converge together");
        return 1;
    }
    if (!VerifyCockpitMount()) {
        std::puts("FAIL: startup cockpit mount and rendered eye position differ");
        return 1;
    }
    if (!VerifyTutorialProjectileModel()) {
        std::puts("FAIL: training projectile must use the scaled Target model");
        return 1;
    }
    if (!VerifyTutorialVisualEffects()) {
        std::puts("FAIL: tutorial shield and explosion effects were not rendered");
        return 1;
    }
    if (!VerifyPracticeBossFinalEffect(combat)) {
        std::puts("FAIL: practice Boss final explosion or model removal");
        return 1;
    }
    if (!VerifyEnemyInterpolation(combat)) {
        std::puts("FAIL: enemy or missile facing interpolation at 0/0.5/1");
        return 1;
    }
    for (const auto [kind, encounter] : {
            std::pair{BattlePreviewKind::Day1, day1},
            std::pair{BattlePreviewKind::Day2, day2},
            std::pair{BattlePreviewKind::Boss, EncounterConfig{}}}) {
        for (const int renderFps : {30, 60, 120}) {
            const ReplayResult result = Play(kind, renderFps, encounter, combat);
            if (result.phase != BattlePhase::Clear ||
                result.attacksLaunched == 0 ||
                ((kind == BattlePreviewKind::Day1 ||
                    kind == BattlePreviewKind::Day2) && result.wave != 2) ||
                (kind == BattlePreviewKind::Boss &&
                    result.bossHitPoints != 0)) {
                std::puts("FAIL: input-driven battle did not reach Clear");
                return 1;
            }
        }
    }
    FirstPersonGame retryPlayer(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    BattleCombatConfig retryConfig = combat;
    retryConfig.enemy.baseHitPoints = 1;
    CombatSession retrySession(BattlePreviewKind::MissileUnit,
        retryPlayer, {}, retryConfig);
    GameInput enter{};
    enter.battleStart.pressed = true;
    retrySession.Update(kStep, enter, 0.0f, 0.0f);
    int landingCues = 0;
    for (int step = 0; step < 3000 &&
            retrySession.GetPhase() != BattlePhase::GameOver; ++step) {
        retrySession.Update(kStep, {}, 0.0f, 0.0f);
        const auto& cues = retrySession.GetFeedbackEvents();
        const int inStep = static_cast<int>(std::count(cues.begin(),
            cues.end(), BattleFeedbackEvent::EnemyLanded));
        if (inStep > 1) {
            std::puts("FAIL: enemy landing cue repeated in one step");
            return 1;
        }
        landingCues += inStep;
    }
    if (retrySession.GetPhase() != BattlePhase::GameOver ||
        landingCues != 1) {
        std::puts("FAIL: unattended battle did not reach GameOver");
        return 1;
    }
    GameInput reset{};
    reset.resetTarget.pressed = true;
    retrySession.Update(kStep, reset, 0.0f, 0.0f);
    if (retrySession.GetPhase() != BattlePhase::Ready ||
        retrySession.GetBaseHitPoints() != 1 ||
        !retrySession.GetEnemies().empty()) {
        std::puts("FAIL: R did not reset the battle after GameOver");
        return 1;
    }
    retrySession.Update(kStep, enter, 0.0f, 0.0f);
    for (int step = 0; step < 181; ++step) {
        retrySession.Update(kStep, {}, 0.0f, 0.0f);
    }
    if (retrySession.GetPhase() != BattlePhase::Playing ||
        retrySession.GetEnemies().empty()) {
        std::puts("FAIL: Enter did not restart the battle after R");
        return 1;
    }
    std::puts("PASS: input-driven Day1/Day2/Boss Clear at 30/60/120 FPS and GameOver retry");
    return 0;
}
