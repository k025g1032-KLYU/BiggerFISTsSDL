#include "Rendering/FirstPersonScenePresenter.h"

#include "Data/BattleAssetCatalog.h"
#include "Gameplay/Combat/CombatSession.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <string_view>
#include <utility>

namespace {
constexpr float kDegreesPerRadian = 180.0f / std::numbers::pi_v<float>;

Vector3 Add(const Vector3& first, const Vector3& second) {
    return {first.x + second.x, first.y + second.y, first.z + second.z};
}

void Rotate(ModelInstance& model, const Vector3& radians) {
    model.initialXDegrees += radians.x * kDegreesPerRadian;
    model.initialYDegrees += radians.y * kDegreesPerRadian;
    model.initialZDegrees += radians.z * kDegreesPerRadian;
}
}

void FirstPersonScenePresenter::ClearDynamic() {
    visualEffects_.clear();
    cloudsInitialized_ = false;
    cloudRandomState_ = 0x63A9D2E1u;
    std::erase_if(scene_.models, [](const ModelInstance& model) {
        return model.id.starts_with("battle_") || model.id.starts_with("tutorial_") ||
            model.id.starts_with("fullgame_");
    });
}

void FirstPersonScenePresenter::QueueVisualEvents(
    const std::vector<BattleVisualEvent>& events) {
    for (const BattleVisualEvent& event : events) {
        visualEffects_.push_back({nextVisualEffectId_++, event.effect,
            event.position, event.scale, event.shieldLayers, visualTime_});
    }
}

void FirstPersonScenePresenter::AdvanceVisualTime(double seconds) {
    if (std::isfinite(seconds) && seconds > 0.0) visualTime_ += seconds;
}

void FirstPersonScenePresenter::SyncEnvironment(const FirstPersonGame& game,
    const BattleAssetCatalog& assets, bool tutorial) {
    std::erase_if(scene_.models, [](const ModelInstance& model) {
        return model.id == "fullgame_body" || model.id == "fullgame_banner" ||
            model.id.starts_with("fullgame_cloud_");
    });
    if (const ModelInstance* prototype = assets.Find("body")) {
        ModelInstance body = *prototype;
        body.id = "fullgame_body";
        const Vector3& position = game.GetPlayer().GetPosition();
        body.position = {position.x, position.y + 1.8f, position.z};
        Rotate(body, {0.0f, game.GetPlayer().GetBodyYaw(), 0.0f});
        scene_.models.push_back(std::move(body));
    }
    if (tutorial) {
        if (const ModelInstance* prototype = assets.Find("tutorial_banner")) {
            ModelInstance banner = *prototype;
            banner.id = "fullgame_banner";
            banner.transparent = true;
            banner.position = {0.0f, 12.0f, 41.85f};
            Rotate(banner, {0.0f, std::numbers::pi_v<float>, 0.0f});
            scene_.models.push_back(std::move(banner));
        }
    }
    if (const ModelInstance* prototype = assets.Find("cloud")) {
        const auto randomRange = [&](float minimum, float maximum) {
            cloudRandomState_ = cloudRandomState_ * 1664525u + 1013904223u;
            const float ratio = static_cast<float>((cloudRandomState_ >> 8) & 0x00ffffffu) /
                16777215.0f;
            return minimum + (maximum - minimum) * ratio;
        };
        const auto resetCloud = [&](CloudPose& cloud, bool initial) {
            cloud.position = {randomRange(-34.0f, 34.0f),
                randomRange(22.0f, 42.0f),
                randomRange(initial ? -34.0f : 60.0f,
                    initial ? 60.0f : 72.0f)};
            cloud.yaw = randomRange(-std::numbers::pi_v<float>,
                std::numbers::pi_v<float>);
            cloud.scale = {randomRange(0.75f, 2.10f),
                randomRange(0.75f, 2.10f),
                randomRange(0.75f, 2.10f)};
            cloud.speed = randomRange(0.70f, 1.25f);
        };
        if (!cloudsInitialized_ || scene_.elapsedSeconds < lastCloudTime_) {
            for (CloudPose& cloud : clouds_) resetCloud(cloud, true);
            lastCloudTime_ = scene_.elapsedSeconds;
            cloudsInitialized_ = true;
        }
        double remaining = std::max(0.0,
            scene_.elapsedSeconds - lastCloudTime_);
        while (remaining > 0.000001) {
            const float step = static_cast<float>(std::min(remaining, 1.0 / 60.0));
            for (CloudPose& cloud : clouds_) {
                cloud.position.z -= cloud.speed * step;
                if (cloud.position.z < -34.0f) resetCloud(cloud, false);
            }
            remaining -= step;
        }
        lastCloudTime_ = scene_.elapsedSeconds;
        for (std::size_t index = 0; index < clouds_.size(); ++index) {
            ModelInstance cloud = *prototype;
            cloud.id = "fullgame_cloud_" + std::to_string(index);
            cloud.transparent = true;
            cloud.position = clouds_[index].position;
            cloud.scaleAxes = clouds_[index].scale;
            Rotate(cloud, {0.0f, clouds_[index].yaw, 0.0f});
            scene_.models.push_back(std::move(cloud));
        }
    }
}

void FirstPersonScenePresenter::SyncCockpit(const FirstPersonGame& game,
    const BattleAssetCatalog& assets, bool visible,
    float startupRemainingRatio) {
    std::erase_if(scene_.models, [](const ModelInstance& model) {
        return model.id == "fullgame_cockpit" || model.id == "fullgame_helmet";
    });
    if (!visible) return;
    if (ModelInstance* body = FindModelById(scene_, "fullgame_body"))
        body->position.y -= 2.6f * startupRemainingRatio;
    for (const char* id : {"left_fist", "right_fist",
            "left_upper_arm", "right_upper_arm"}) {
        if (ModelInstance* arm = FindModelById(scene_, id))
            arm->position.y -= 2.8f * startupRemainingRatio;
    }
    const ModelInstance* prototype = assets.Find("cockpit");
    if (!prototype) return;
    ModelInstance cockpit = *prototype;
    cockpit.id = "fullgame_cockpit";
    const Vector3& player = game.GetPlayer().GetPosition();
    cockpit.position = {player.x, player.y + 4.15f -
        startupRemainingRatio * 2.9f,
        player.z};
    cockpit.scale = 1.0f;
    cockpit.opacity = 1.0f;
    cockpit.transparent = true;
    cockpit.initialXDegrees = 0.0f;
    cockpit.initialYDegrees = game.GetPlayer().GetBodyYaw() * 57.2957795f;
    cockpit.initialZDegrees = 0.0f;
    scene_.models.push_back(std::move(cockpit));
    if (const ModelInstance* helmetPrototype = assets.Find("helmet")) {
        ModelInstance helmet = *helmetPrototype;
        helmet.id = "fullgame_helmet";
        helmet.position = game.GetCamera().GetPosition();
        helmet.position.y -= 0.3f * startupRemainingRatio;
        helmet.opacity = 0.45f;
        helmet.transparent = true;
        Rotate(helmet, {game.GetCamera().GetPitch(),
            game.GetCamera().GetYaw(), 0.0f});
        scene_.models.push_back(std::move(helmet));
    }
}

void FirstPersonScenePresenter::SyncTutorial(const FirstPersonGame& game,
    const BattleAssetCatalog& assets, bool showTargets) {
    std::erase_if(scene_.models, [](const ModelInstance& model) {
        return model.id.starts_with("tutorial_") || model.id.starts_with("battle_");
    });
    SyncEnvironment(game, assets, true);
    SyncRocketFlames(game, assets);
    if (!showTargets) return;
    for (const FirstPersonTarget& target : game.GetTargets()) {
        if (target.hitPoints <= 0) continue;
        const bool missile = target.id == "target_tutorial_missile";
        const ModelInstance* prototype = assets.Find(missile ? "missile" : "tutorial_target");
        if (!prototype) continue;
        ModelInstance instance = *prototype;
        instance.id = "tutorial_" + target.id;
        instance.position = Add(target.position, prototype->position);
        instance.scale = missile ? 0.8f : 1.0f;
        scene_.models.push_back(std::move(instance));
    }
}

void FirstPersonScenePresenter::SyncRocketFlames(
    const FirstPersonGame& game, const BattleAssetCatalog& assets) {
    const ModelInstance* prototype = assets.Find("flame");
    if (!prototype) return;
    const auto addFlame = [&](const PunchArm& arm, std::string_view side) {
        if (arm.GetAttackType() != PunchArmAttackType::RocketPunch ||
            !arm.IsFistAway()) return;
        const float yaw = arm.GetRocketPoseYaw();
        const float pitch = arm.GetRocketPosePitch();
        const float cosPitch = std::cos(pitch);
        const Vector3 backward{-std::sin(yaw) * cosPitch,
            std::sin(pitch), -std::cos(yaw) * cosPitch};
        float flameScale = arm.GetState() == PunchArmState::RocketFlying
            ? 1.0f : 0.5f;
        if (arm.GetState() == PunchArmState::RocketFlying) {
            const float remaining = std::max(0.0f,
                arm.GetRocketMaximumRange() - arm.GetRocketTravelDistance());
            const float progress = std::clamp(1.0f - remaining / 2.0f,
                0.0f, 1.0f);
            flameScale += (0.15f - flameScale) *
                progress * progress * (3.0f - 2.0f * progress);
        }
        const float length = flameScale;
        const float distance = 0.55f + 1.5f + length;
        const Vector3& fist = arm.GetFistPosition();
        ModelInstance flame = *prototype;
        flame.id = "battle_" + std::string(side) + "_rocket_flame";
        flame.position = {fist.x + backward.x * distance,
            fist.y + backward.y * distance,
            fist.z + backward.z * distance};
        flame.scaleAxes = {0.45f * flameScale, length,
            0.45f * flameScale};
        Rotate(flame, {std::numbers::pi_v<float> * 0.5f - pitch,
            yaw + std::numbers::pi_v<float>, 0.0f});
        flame.colorTint = {1.0f, 0.55f, 0.15f};
        flame.transparent = true;
        scene_.models.push_back(std::move(flame));
    };
    addFlame(game.GetLeftArm(), "left");
    addFlame(game.GetRightArm(), "right");
}

void FirstPersonScenePresenter::SyncBattle(const CombatSession& battle,
    const BattleAssetCatalog& assets) {
    std::erase_if(scene_.models, [](const ModelInstance& model) {
        return model.id.starts_with("battle_") || model.id.starts_with("tutorial_");
    });
    SyncEnvironment(battle.GetPlayerGame(), assets, false);
    SyncRocketFlames(battle.GetPlayerGame(), assets);
    const auto add = [&](const std::string& id, std::string_view key,
        const Vector3& position, float scale, Vector3 tint) {
        const ModelInstance* prototype = assets.Find(key);
        if (prototype == nullptr) {
            return;
        }
        ModelInstance model = *prototype;
        model.id = "battle_" + id;
        model.position = Add(position, prototype->position);
        model.scale = scale;
        model.colorTint = tint;
        scene_.models.push_back(std::move(model));
    };
    std::erase_if(visualEffects_, [&](const ActiveVisualEffect& effect) {
        const double lifetime = effect.kind == BattleVisualEffect::ShieldHit
            ? 0.46 : 0.97;
        return visualTime_ - effect.startTime >= lifetime;
    });
    for (const ActiveVisualEffect& effect : visualEffects_) {
        const float age = static_cast<float>(std::max(0.0,
            visualTime_ - effect.startTime));
        const std::string id = "effect_" + std::to_string(effect.id);
        if (effect.kind == BattleVisualEffect::ShieldHit) {
            const std::string_view key = effect.shieldLayers >= 3
                ? "shield_three" : effect.shieldLayers == 2
                ? "shield_two" : "shield_one";
            add(id, key, effect.position,
                effect.scale * (0.25f + 0.83f *
                    std::clamp(age / 0.10f, 0.0f, 1.0f)),
                {1.0f, 1.0f, 1.0f});
            if (ModelInstance* model = FindModelById(scene_, "battle_" + id)) {
                model->opacity = 1.0f - std::clamp((age - 0.26f) /
                    0.20f, 0.0f, 1.0f);
                model->transparent = true;
            }
        } else {
            const auto component = [&](std::string_view suffix,
                std::string_view key, float duration, float delay,
                float initialScale, float finalScale, Vector3 tint,
                float maximumOpacity) {
                if (age < delay || age >= delay + duration) return;
                const float progress = (age - delay) / duration;
                add(id + std::string(suffix), key, effect.position,
                    effect.scale * (initialScale +
                        (finalScale - initialScale) * progress), tint);
                if (ModelInstance* model = FindModelById(scene_,
                        "battle_" + id + std::string(suffix))) {
                    model->opacity = maximumOpacity * (1.0f - progress);
                    model->transparent = true;
                }
            };
            component("_flash", "explosion_flash", 0.12f, 0.0f,
                0.55f, 1.35f, {1.0f, 0.78f, 0.32f}, 1.0f);
            component("_fire", "explosion_fire", 0.38f, 0.0f,
                0.45f, 1.45f, {1.0f, 0.44f, 0.08f}, 1.0f);
            component("_smoke", "explosion_smoke", 0.85f, 0.12f,
                0.55f, 1.70f, {0.42f, 0.42f, 0.42f}, 0.72f);
        }
    }
    for (const BattleEnemy& enemy : battle.GetEnemies()) {
        const std::string_view key = enemy.type == BattleEnemyType::MissileUnit
            ? "missile_unit" : enemy.type == BattleEnemyType::Bomber
            ? "bomber" : "charge_unit";
        const float scale = 1.0f;
        const bool chargeWarning = enemy.type == BattleEnemyType::ChargeUnit &&
            (enemy.chargeState == BattleEnemy::ChargeState::Preparing ||
                enemy.chargeState == BattleEnemy::ChargeState::Charging);
        add(enemy.id, key, enemy.position, scale,
            enemy.defeated ? Vector3{1.0f, 0.35f, 0.25f} :
                chargeWarning ? Vector3{1.0f, 0.55f, 0.3f} :
                Vector3{1.0f, 1.0f, 1.0f});
        if (ModelInstance* model = FindModelById(scene_, "battle_" + enemy.id)) {
            if (enemy.defeated) {
                Rotate(*model, {enemy.stateTimer * 10.0f,
                    enemy.stateTimer * 13.0f, enemy.stateTimer * 8.0f});
            } else if (enemy.type == BattleEnemyType::MissileUnit) {
                if (enemy.deploymentState == BattleEnemy::DeploymentState::Dropping ||
                    enemy.deploymentState == BattleEnemy::DeploymentState::Landed) {
                    Rotate(*model, {-std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f});
                } else if (enemy.deploymentState == BattleEnemy::DeploymentState::TakingOff) {
                    const float progress = std::clamp((enemy.position.y - 1.25f) /
                        std::max(0.001f, enemy.combatHeight - 1.25f), 0.0f, 1.0f);
                    Rotate(*model, {-std::numbers::pi_v<float> * 0.5f *
                        (1.0f - progress), 0.0f, 0.0f});
                } else {
                    Rotate(*model, {0.0f, enemy.velocity.x > 0.0f
                        ? -std::numbers::pi_v<float> * 0.5f
                        : std::numbers::pi_v<float> * 0.5f, 0.0f});
                }
            } else if (enemy.type == BattleEnemyType::Bomber) {
                Rotate(*model, {0.0f, 0.0f, enemy.visualAge * 5.0f});
            } else {
                if (enemy.deploymentState ==
                        BattleEnemy::DeploymentState::Dropping ||
                    enemy.deploymentState ==
                        BattleEnemy::DeploymentState::Landed) {
                    Rotate(*model, {0.0f, 0.0f,
                        std::numbers::pi_v<float> * 0.5f});
                } else if (enemy.deploymentState ==
                        BattleEnemy::DeploymentState::TakingOff) {
                    const float progress = std::clamp((enemy.position.y - 1.25f) /
                        std::max(0.001f, enemy.combatHeight - 1.25f),
                        0.0f, 1.0f);
                    Rotate(*model, {0.0f,
                        std::numbers::pi_v<float> * 1.5f * progress,
                        std::numbers::pi_v<float> * 0.5f * (1.0f - progress)});
                } else {
                    Rotate(*model, {0.0f, enemy.visualYaw, 0.0f});
                }
            }
        }
        if (enemy.type == BattleEnemyType::ChargeUnit &&
            enemy.chargeState == BattleEnemy::ChargeState::Preparing) {
            if (ModelInstance* model = FindModelById(scene_, "battle_" + enemy.id)) {
                const float pulse = 1.0f + std::sin(enemy.chargeTimer * 14.0f) * 0.16f;
                model->scaleAxes = {pulse, pulse, 1.0f / pulse};
            }
        }
        if (enemy.type == BattleEnemyType::ChargeUnit &&
            enemy.chargeState == BattleEnemy::ChargeState::Charging) {
            add(enemy.id + "_flame", "flame",
                {enemy.position.x - enemy.chargeDirection.x * 1.2f,
                    enemy.position.y,
                    enemy.position.z - enemy.chargeDirection.z * 1.2f},
                1.0f, {1.0f, 0.7f, 0.3f});
            if (ModelInstance* flame = FindModelById(scene_,
                    "battle_" + enemy.id + "_flame")) {
                flame->scaleAxes = {0.5f, 1.15f, 0.5f};
                flame->transparent = true;
                Rotate(*flame, {std::numbers::pi_v<float> * 0.5f,
                    std::atan2(-enemy.chargeDirection.x,
                        -enemy.chargeDirection.z), 0.0f});
            }
        }
        if (enemy.type == BattleEnemyType::Bomber && !enemy.defeated &&
            enemy.deployed) {
            add(enemy.id + "_carried_bomb", "floating_bomb",
                enemy.position, 1.0f,
                enemy.attackTimer < 1.0f ? Vector3{1.0f, 0.55f, 0.25f}
                    : Vector3{1.0f, 1.0f, 1.0f});
            if (ModelInstance* bomb = FindModelById(scene_,
                    "battle_" + enemy.id + "_carried_bomb")) {
                bomb->scale = enemy.carriedBombScale;
                Rotate(*bomb, {0.0f, enemy.visualAge * 5.0f, 0.0f});
            }
        }
    }
    for (const BattleProjectile& projectile : battle.GetProjectiles()) {
        const std::string_view key = projectile.type == BattleProjectileType::Missile
            ? "missile" : projectile.type == BattleProjectileType::FloatingBomb
            ? "floating_bomb" : projectile.type == BattleProjectileType::CrystalSpike
            ? "boss_crystal_spike" : "boss_crystal_cube";
        add(projectile.id, key, projectile.position,
            1.0f,
            {1.0f, 1.0f, 1.0f});
        if (ModelInstance* model = FindModelById(scene_, "battle_" + projectile.id)) {
            const Vector3& direction = projectile.direction;
            if (projectile.type == BattleProjectileType::Missile)
                Rotate(*model, {0.0f,
                    std::atan2(direction.x, direction.z) +
                        std::numbers::pi_v<float>, 0.0f});
            else if (projectile.type == BattleProjectileType::CrystalSpike)
                Rotate(*model, {std::numbers::pi_v<float> * 0.5f,
                    std::atan2(direction.x, direction.z), 0.0f});
            else if (projectile.type == BattleProjectileType::CrystalCube)
                Rotate(*model, {projectile.age * 4.1f,
                    projectile.age * 5.7f, projectile.age * 3.3f});
            else
                Rotate(*model, {projectile.age * 6.5f,
                    projectile.age * 5.1f, projectile.age * 3.8f});
        }
        if (projectile.type == BattleProjectileType::Missile) {
            const Vector3 tail{-projectile.direction.x,
                -projectile.direction.y, -projectile.direction.z};
            add(projectile.id + "_tail", "flame", {
                projectile.position.x + tail.x * 0.663f,
                projectile.position.y + tail.y * 0.663f,
                projectile.position.z + tail.z * 0.663f},
                1.0f, {1.0f, 0.65f, 0.25f});
            if (ModelInstance* flame = FindModelById(scene_,
                    "battle_" + projectile.id + "_tail")) {
                flame->scaleAxes = {0.12f, 0.213f, 0.12f};
                flame->transparent = true;
                Rotate(*flame, {std::numbers::pi_v<float> * 0.5f,
                    std::atan2(tail.x, tail.z), 0.0f});
            }
        }
    }
    if (const BattleBoss* boss = battle.GetBoss()) {
        const Vector3& position = boss->GetPosition();
        const BossVisualPose pose = boss->GetVisualPose();
        ModelInstance root;
        root.position = position;
        root.initialXDegrees = 0.0f;
        root.initialYDegrees = 0.0f;
        root.rotationXDegreesPerSecond = 0.0f;
        root.rotationYDegreesPerSecond = 0.0f;
        Rotate(root, pose.rootRotation);
        Matrix4x4 rootWorld{};
        const bool hasRoot = TryBuildModelWorldTransform(scene_, root, rootWorld);
        const bool warning = boss->GetMode() == BossMode::Charge ||
            boss->GetMode() == BossMode::SpreadCounter;
        const Vector3 tint = boss->GetMode() == BossMode::Defeating
            ? Vector3{1.0f, 0.35f, 0.25f}
            : boss->IsRocketDodging() ? Vector3{0.50f, 0.85f, 1.0f}
            : warning ? Vector3{1.0f, 0.55f, 0.45f}
            : Vector3{1.0f, 1.0f, 1.0f};
        if (hasRoot) {
            add("boss_core", "boss_core", {}, 1.0f, tint);
            if (ModelInstance* core = FindModelById(scene_, "battle_boss_core")) {
                core->parentWorld = rootWorld;
                Rotate(*core, pose.coreRotation);
            }
            const std::array<std::string_view, 3> keys{
                "boss_cone_left", "boss_cone_right", "boss_cone_lower"};
            const std::array<std::string_view, 3> names{
                "boss_left_cone", "boss_right_cone", "boss_lower_cone"};
            const Matrix4x4 coneParent = pose.spreadSpin == 0.0f
                ? rootWorld : MultiplyMatrices(
                    MakeRotationZMatrix(pose.spreadSpin), rootWorld);
            for (std::size_t index = 0; index < keys.size(); ++index) {
                add(std::string(names[index]), keys[index],
                    pose.cones[index].translation, 1.0f, tint);
                if (ModelInstance* cone = FindModelById(scene_,
                        "battle_" + std::string(names[index]))) {
                    const Vector3& angle = pose.cones[index].rotation;
                    const Vector3& move = pose.cones[index].rootTranslation;
                    const Matrix4x4 partRotation = MultiplyMatrices(
                        MultiplyMatrices(MakeRotationZMatrix(angle.z),
                            MakeRotationXMatrix(angle.x)),
                        MakeRotationYMatrix(angle.y));
                    cone->parentWorld = MultiplyMatrices(
                        MultiplyMatrices(partRotation,
                            MakeTranslationMatrix(move.x, move.y, move.z)),
                        coneParent);
                }
            }
        }
        const bool showHalo = boss->GetMode() != BossMode::Defeating &&
            boss->GetMode() != BossMode::Defeated;
        for (std::size_t index = 0;
                showHalo && index < boss->GetSpikeSlots().size(); ++index) {
            const BossCrystalSlot& slot = boss->GetSpikeSlots()[index];
            if (!slot.consumed && slot.reloadTime <= 0.0f) {
                add("boss_spike_slot_" + std::to_string(index),
                    "boss_crystal_spike", pose.spikes[index].translation, 1.0f,
                    slot.launchTime > 0.0f ? Vector3{1.0f, 0.8f, 0.5f}
                        : Vector3{0.65f, 0.85f, 1.0f});
                if (ModelInstance* crystal = FindModelById(scene_,
                        "battle_boss_spike_slot_" + std::to_string(index))) {
                    if (hasRoot) crystal->parentWorld = rootWorld;
                    Rotate(*crystal, pose.spikes[index].rotation);
                }
            }
        }
        for (std::size_t index = 0;
                showHalo && index < boss->GetCubeSlots().size(); ++index) {
            const BossCrystalSlot& slot = boss->GetCubeSlots()[index];
            if (!slot.consumed && slot.reloadTime <= 0.0f) {
                add("boss_cube_slot_" + std::to_string(index),
                    "boss_crystal_cube", pose.cubes[index].translation, 1.0f,
                    slot.launchTime > 0.0f ? Vector3{1.0f, 0.8f, 0.5f}
                        : Vector3{0.65f, 0.85f, 1.0f});
                if (ModelInstance* crystal = FindModelById(scene_,
                        "battle_boss_cube_slot_" + std::to_string(index))) {
                    if (hasRoot) crystal->parentWorld = rootWorld;
                    Rotate(*crystal, pose.cubes[index].rotation);
                }
            }
        }
    }
    const FirstPersonGame& player = battle.GetPlayerGame();
    if (player.GetUltimateState() == UltimateState::Forming) {
        const float ratio = player.GetUltimateFormationRatio();
        const Vector3 formation = player.GetUltimateFormationPosition();
        for (const auto& [id, sideSign] :
                {std::pair{"left_fist", -1.0f},
                    std::pair{"right_fist", 1.0f}}) {
            if (ModelInstance* fist = FindModelById(scene_, id)) {
                fist->position.x += (formation.x - fist->position.x) * ratio;
                fist->position.y += (formation.y - fist->position.y) * ratio;
                fist->position.z += (formation.z - fist->position.z) * ratio;
                fist->initialZDegrees = -sideSign * 90.0f * ratio;
            }
        }
    }
    if (player.GetUltimateState() == UltimateState::Ready ||
        player.GetUltimateState() == UltimateState::Flying) {
        for (const char* id : {"left_fist", "right_fist"}) {
            if (ModelInstance* fist = FindModelById(scene_, id)) {
                fist->scale = 0.0f;
            }
        }
    } else {
        for (const char* id : {"left_fist", "right_fist"}) {
            if (ModelInstance* fist = FindModelById(scene_, id)) {
                fist->scale = 1.0f;
            }
        }
    }
    if (player.GetUltimateState() == UltimateState::Ready ||
        player.GetUltimateState() == UltimateState::Flying) {
        const bool flying = player.GetUltimateState() == UltimateState::Flying;
        Vector3 center = flying ? player.GetUltimatePosition() :
            player.GetUltimateFormationPosition();
        if (!flying) {
            const float phase = static_cast<float>(scene_.elapsedSeconds) * 60.0f;
            center.x += std::sin(phase) * 0.08f;
            center.y += std::sin(phase * 1.71f + 0.8f) * 0.052f;
            center.z += std::sin(phase * 1.29f + 1.6f) * 0.064f;
        }
        add("ultimate", "ultimate_fist", center, 1.0f,
            {1.0f, 0.8f, 0.35f});
        if (ModelInstance* fist = FindModelById(scene_, "battle_ultimate")) {
            const Vector3& direction = player.GetUltimateDirection();
            const float yaw = flying ? std::atan2(direction.x, direction.z) :
                player.GetPlayer().GetBodyYaw();
            Rotate(*fist, {0.0f, yaw, 0.0f});
        }
        if (flying) {
            if (const ModelInstance* prototype = assets.Find("flame")) {
                ModelInstance flame = *prototype;
                flame.id = "battle_ultimate_flame";
                const Vector3& direction = player.GetUltimateDirection();
                flame.position = {center.x - direction.x * 3.5f,
                    center.y - direction.y * 3.5f,
                    center.z - direction.z * 3.5f};
                flame.scaleAxes = {1.0f, 2.0f, 1.0f};
                Rotate(flame, {std::numbers::pi_v<float> * 0.5f,
                    std::atan2(-direction.x, -direction.z), 0.0f});
                flame.colorTint = {1.0f, 0.55f, 0.15f};
                flame.transparent = true;
                scene_.models.push_back(std::move(flame));
            }
        }
    } else if (player.GetUltimateState() == UltimateState::Returning) {
        if (ModelInstance* fist = FindModelById(scene_, "left_fist")) {
            fist->position = player.GetUltimateReturnFistPosition(PunchArmSide::Left);
        }
        if (ModelInstance* fist = FindModelById(scene_, "right_fist")) {
            fist->position = player.GetUltimateReturnFistPosition(PunchArmSide::Right);
        }
    }
}
