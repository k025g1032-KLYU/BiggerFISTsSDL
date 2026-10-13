#include "Rendering/FirstPersonScenePresenter.h"

#include "Data/BattleAssetCatalog.h"
#include "Gameplay/Combat/CombatSession.h"
#include "Gameplay/Cockpit/CockpitPresentation.h"
#include "Gameplay/Player/FirstPersonTuning.h"

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

Vector3 Interpolate(const Vector3& previous, const Vector3& current,
    float alpha) {
    return {previous.x + (current.x - previous.x) * alpha,
        previous.y + (current.y - previous.y) * alpha,
        previous.z + (current.z - previous.z) * alpha};
}

float InterpolateAngle(float previous, float current, float alpha) {
    return previous + std::remainder(current - previous,
        2.0f * std::numbers::pi_v<float>) * alpha;
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
            event.position, event.scale, event.shieldImpact, visualTime_});
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
        const Vector3 position = Interpolate(
            game.GetPlayer().GetPreviousPosition(),
            game.GetPlayer().GetPosition(), interpolationAlpha_);
        body.position = {position.x, position.y + 1.8f, position.z};
        Rotate(body, {0.0f, game.GetPlayer().GetBodyYaw(), 0.0f});
        scene_.models.push_back(std::move(body));
    }
    if (tutorial) {
        if (const ModelInstance* prototype = assets.Find("tutorial_banner")) {
            ModelInstance banner = *prototype;
            banner.id = "fullgame_banner";
            banner.transparent = true;
            banner.renderLayer = ModelRenderLayer::TransparentBackground;
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
            cloud.renderLayer = ModelRenderLayer::TransparentBackground;
            cloud.position = clouds_[index].position;
            cloud.scaleAxes = clouds_[index].scale;
            Rotate(cloud, {0.0f, clouds_[index].yaw, 0.0f});
            scene_.models.push_back(std::move(cloud));
        }
    }
}

void FirstPersonScenePresenter::SyncCockpit(const FirstPersonGame& game,
    const BattleAssetCatalog& assets, bool visible,
    const CockpitActivation& activation, int practiceModelMode,
    bool emergency) {
    std::erase_if(scene_.models, [](const ModelInstance& model) {
        return model.id == "fullgame_cockpit" || model.id == "fullgame_helmet";
    });
    const float bodySinkDistance = emergency
        ? FirstPersonTuning::kSystemOfflineBodySinkDistance
        : FirstPersonTuning::kCockpitStartupBodySinkDistance;
    const float cameraSinkDistance = emergency
        ? FirstPersonTuning::kSystemOfflineCameraSinkDistance
        : FirstPersonTuning::kCockpitStartupCameraSinkDistance;
    const float armSinkDistance = emergency
        ? FirstPersonTuning::kSystemOfflineArmSinkDistance
        : FirstPersonTuning::kCockpitStartupArmSinkDistance;
    const float bodySink = bodySinkDistance * (1.0f - activation.body);
    const float cameraSink = cameraSinkDistance * (1.0f - activation.camera);
    const float mountSink = bodySink + cameraSink;
    scene_.cameraPosition.y -= mountSink;
    scene_.cameraTarget.y -= mountSink;
    if (ModelInstance* body = FindModelById(scene_, "fullgame_body"))
        body->position.y -= bodySink;
    for (const char* id : {"left_fist", "left_upper_arm"})
        if (ModelInstance* arm = FindModelById(scene_, id))
            arm->position.y -= armSinkDistance * (1.0f - activation.leftArm);
    for (const char* id : {"right_fist", "right_upper_arm"})
        if (ModelInstance* arm = FindModelById(scene_, id))
            arm->position.y -= armSinkDistance * (1.0f - activation.rightArm);
    if (!visible || practiceModelMode == 3) return;
    const ModelInstance* prototype = assets.Find("cockpit");
    if (!prototype) return;
    ModelInstance cockpit = *prototype;
    cockpit.id = "fullgame_cockpit";
    const Vector3 player = Interpolate(
        game.GetPlayer().GetPreviousPosition(),
        game.GetPlayer().GetPosition(), interpolationAlpha_);
    cockpit.position = {player.x, player.y + FirstPersonTuning::kEyeHeight -
        mountSink,
        player.z};
    cockpit.scale = 1.0f;
    cockpit.opacity = 1.0f;
    cockpit.transparent = true;
    cockpit.renderLayer = ModelRenderLayer::FirstPerson;
    cockpit.initialXDegrees = 0.0f;
    cockpit.initialYDegrees = game.GetPlayer().GetBodyYaw() * 57.2957795f;
    cockpit.initialZDegrees = 0.0f;
    if (practiceModelMode != 1) scene_.models.push_back(std::move(cockpit));
    if (practiceModelMode != 2) if (const ModelInstance* helmetPrototype = assets.Find("helmet")) {
        ModelInstance helmet = *helmetPrototype;
        helmet.id = "fullgame_helmet";
        helmet.position = scene_.cameraPosition;
        helmet.opacity = 0.45f;
        helmet.transparent = true;
        helmet.renderLayer = ModelRenderLayer::FirstPerson;
        Rotate(helmet, {game.GetCamera().GetPitch(),
            game.GetCamera().GetYaw(), 0.0f});
        scene_.models.push_back(std::move(helmet));
    }
}

void FirstPersonScenePresenter::SetCombatThirdPerson(
    const FirstPersonGame& game, bool enabled) {
    if (!enabled) return;
    const float yaw = game.GetCamera().GetYaw();
    const float pitch = std::clamp(game.GetCamera().GetPitch() + 0.28f,
        0.12f, 0.75f);
    const float horizontalDistance = std::cos(pitch) * 10.0f;
    const Vector3 player = game.GetPlayer().GetPosition();
    scene_.cameraTarget = {player.x, player.y + 1.8f, player.z};
    scene_.cameraPosition = {player.x - std::sin(yaw) * horizontalDistance,
        player.y + 1.8f + std::sin(pitch) * 10.0f,
        player.z - std::cos(yaw) * horizontalDistance};
}

void FirstPersonScenePresenter::SyncTutorial(const FirstPersonGame& game,
    const BattleAssetCatalog& assets, bool showTargets) {
    std::erase_if(scene_.models, [](const ModelInstance& model) {
        return model.id.starts_with("tutorial_") || model.id.starts_with("battle_");
    });
    SyncEnvironment(game, assets, true);
    SyncRocketFlames(game, assets);
    SyncVisualEffects(game, assets);
    for (const FirstPersonTarget& target : game.GetTargets()) {
        if (target.hitPoints <= 0) continue;
        const bool missile = target.id == "target_tutorial_missile";
        if (!missile && !showTargets) continue;
        const ModelInstance* prototype = assets.Find("tutorial_target");
        if (!prototype) continue;
        ModelInstance instance = *prototype;
        instance.id = "tutorial_" + target.id;
        instance.position = Add(target.position, prototype->position);
        instance.scale = missile ? target.radius / 1.25f : 1.0f;
        scene_.models.push_back(std::move(instance));
        if (missile) {
            const ModelInstance* flamePrototype = assets.Find("flame");
            if (!flamePrototype) continue;
            const Vector3 movement{
                target.position.x - target.previousPosition.x,
                0.0f, target.position.z - target.previousPosition.z};
            const float length = std::hypot(movement.x, movement.z);
            const Vector3 tailDirection = length > 0.00001f
                ? Vector3{-movement.x / length, 0.0f,
                    -movement.z / length} : Vector3{0.0f, 0.0f, 1.0f};
            ModelInstance flame = *flamePrototype;
            flame.id = "tutorial_missile_tail";
            flame.position = Add(target.position, {
                tailDirection.x * (target.radius + 0.213f),
                0.0f,
                tailDirection.z * (target.radius + 0.213f)});
            flame.scaleAxes = {0.12f, 0.213f, 0.12f};
            flame.transparent = true;
            flame.renderLayer = ModelRenderLayer::AdditiveEffect;
            Rotate(flame, {std::numbers::pi_v<float> * 0.5f,
                std::atan2(tailDirection.x, tailDirection.z), 0.0f});
            scene_.models.push_back(std::move(flame));
        }
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
        const float flameScale = arm.GetRocketFlameScale();
        const float length = flameScale;
        const float distance = 0.55f + 1.5f + length;
        const Vector3 fist = Interpolate(arm.GetPreviousFistPosition(),
            arm.GetFistPosition(), interpolationAlpha_);
        ModelInstance flame = *prototype;
        flame.id = "battle_" + std::string(side) + "_rocket_flame";
        flame.position = {fist.x + backward.x * distance,
            fist.y + backward.y * distance,
            fist.z + backward.z * distance};
        flame.scaleAxes = {0.45f * flameScale, length,
            0.45f * flameScale};
        Rotate(flame, arm.IsUltimateDocking()
            ? Vector3{std::numbers::pi_v<float>, 0.0f, 0.0f}
            : Vector3{std::numbers::pi_v<float> * 0.5f - pitch,
                yaw + std::numbers::pi_v<float>, 0.0f});
        flame.colorTint = {1.0f, 1.0f, 1.0f};
        flame.transparent = true;
        flame.renderLayer = ModelRenderLayer::AdditiveEffect;
        scene_.models.push_back(std::move(flame));
    };
    addFlame(game.GetLeftArm(), "left");
    addFlame(game.GetRightArm(), "right");
}

void FirstPersonScenePresenter::SyncVisualEffects(
    const FirstPersonGame& game, const BattleAssetCatalog& assets) {
    const auto add = [&](const std::string& id, std::string_view key,
        const Vector3& position, float scale, Vector3 tint) {
        const ModelInstance* prototype = assets.Find(key);
        if (!prototype) return;
        ModelInstance model = *prototype;
        model.id = "battle_" + id;
        model.position = Add(position, prototype->position);
        model.scale = scale;
        model.colorTint = tint;
        scene_.models.push_back(std::move(model));
    };
    std::erase_if(visualEffects_, [&](const ActiveVisualEffect& effect) {
        return visualTime_ - effect.startTime >=
            BattleVisualEffectDuration(effect.kind);
    });
    for (const ActiveVisualEffect& effect : visualEffects_) {
        const float age = static_cast<float>(std::max(0.0,
            visualTime_ - effect.startTime));
        const std::string id = "effect_" + std::to_string(effect.id);
        if (effect.kind == BattleVisualEffect::ShieldHit) {
            const std::string_view key =
                effect.shieldImpact == ShieldImpactState::TwoOrMoreRemaining
                    ? "shield_three" :
                effect.shieldImpact == ShieldImpactState::OneRemaining
                    ? "shield_two" : "shield_one";
            Vector3 shieldCenter = game.GetPlayer().GetPosition();
            shieldCenter.y += 4.15f;
            add(id, key, shieldCenter,
                effect.scale * (0.25f + 0.83f *
                    std::clamp(age / 0.10f, 0.0f, 1.0f)),
                {1.0f, 1.0f, 1.0f});
            if (ModelInstance* model = FindModelById(scene_, "battle_" + id)) {
                model->opacity = 1.0f - std::clamp((age - 0.26f) /
                    0.20f, 0.0f, 1.0f);
                model->transparent = true;
                model->renderLayer = ModelRenderLayer::AdditiveEffect;
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
                    model->renderLayer = ModelRenderLayer::AdditiveEffect;
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
}

void FirstPersonScenePresenter::SyncBattle(const CombatSession& battle,
    const BattleAssetCatalog& assets, float interpolationAlpha) {
    interpolationAlpha = std::clamp(interpolationAlpha, 0.0f, 1.0f);
    std::erase_if(scene_.models, [](const ModelInstance& model) {
        return model.id.starts_with("battle_") || model.id.starts_with("tutorial_");
    });
    SyncEnvironment(battle.GetPlayerGame(), assets, battle.IsPractice());
    SyncRocketFlames(battle.GetPlayerGame(), assets);
    SyncVisualEffects(battle.GetPlayerGame(), assets);
    if (battle.IsPractice()) {
        if (const ModelInstance* prototype = assets.Find("tutorial_target")) {
            for (const FirstPersonTarget& target : battle.GetPlayerGame().GetTargets()) {
                if (!target.id.starts_with("target_tutorial_") ||
                    target.hitPoints <= 0) continue;
                ModelInstance instance = *prototype;
                instance.id = "tutorial_" + target.id;
                instance.position = Add(target.position, prototype->position);
                scene_.models.push_back(std::move(instance));
            }
        }
    }
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
    for (const BattleEnemy& enemy : battle.GetEnemies()) {
        const Vector3 displayPosition = Interpolate(enemy.previousPosition,
            enemy.position, interpolationAlpha);
        const std::string_view key = enemy.type == BattleEnemyType::MissileUnit
            ? "missile_unit" : enemy.type == BattleEnemyType::Bomber
            ? "bomber" : "charge_unit";
        const float scale = 1.0f;
        add(enemy.id, key, displayPosition, scale,
            {1.0f, 1.0f, 1.0f});
        if (ModelInstance* model = FindModelById(scene_, "battle_" + enemy.id)) {
            if (enemy.defeated) {
                Rotate(*model, Interpolate(enemy.previousDefeatRotation,
                    enemy.defeatRotation, interpolationAlpha));
            } else if (enemy.type == BattleEnemyType::MissileUnit) {
                if (enemy.deploymentState == BattleEnemy::DeploymentState::Dropping ||
                    enemy.deploymentState == BattleEnemy::DeploymentState::Landed) {
                    Rotate(*model, {-std::numbers::pi_v<float> * 0.5f, 0.0f, 0.0f});
                } else if (enemy.deploymentState == BattleEnemy::DeploymentState::TakingOff) {
                    const float progress = std::clamp((displayPosition.y - 1.25f) /
                        std::max(0.001f, enemy.combatHeight - 1.25f), 0.0f, 1.0f);
                    Rotate(*model, {-std::numbers::pi_v<float> * 0.5f *
                        (1.0f - progress), 0.0f, 0.0f});
                } else {
                    Rotate(*model, {0.0f, InterpolateAngle(
                        enemy.previousVisualYaw, enemy.visualYaw,
                        interpolationAlpha), 0.0f});
                }
            } else if (enemy.type == BattleEnemyType::Bomber) {
                const float age = enemy.previousVisualAge +
                    (enemy.visualAge - enemy.previousVisualAge) *
                    interpolationAlpha;
                Rotate(*model, {0.0f, 0.0f, age * 5.0f});
            } else {
                if (enemy.deploymentState ==
                        BattleEnemy::DeploymentState::Dropping ||
                    enemy.deploymentState ==
                        BattleEnemy::DeploymentState::Landed) {
                    Rotate(*model, {0.0f, 0.0f,
                        std::numbers::pi_v<float> * 0.5f});
                } else if (enemy.deploymentState ==
                        BattleEnemy::DeploymentState::TakingOff) {
                    const float progress = std::clamp((displayPosition.y - 1.25f) /
                        std::max(0.001f, enemy.combatHeight - 1.25f),
                        0.0f, 1.0f);
                    Rotate(*model, {0.0f,
                        std::numbers::pi_v<float> * 1.5f * progress,
                        std::numbers::pi_v<float> * 0.5f * (1.0f - progress)});
                } else {
                    Rotate(*model, {0.0f, InterpolateAngle(
                        enemy.previousVisualYaw, enemy.visualYaw,
                        interpolationAlpha), 0.0f});
                }
            }
        }
        if (enemy.type == BattleEnemyType::ChargeUnit &&
            enemy.chargeState == BattleEnemy::ChargeState::Preparing) {
            if (ModelInstance* model = FindModelById(scene_, "battle_" + enemy.id)) {
                const float pulse = 1.0f + std::sin(enemy.chargeTimer * 14.0f) * 0.16f;
                model->scaleAxes = {model->scaleAxes.x * pulse,
                    model->scaleAxes.y * pulse,
                    model->scaleAxes.z / pulse};
            }
        }
        if (enemy.type == BattleEnemyType::ChargeUnit && !enemy.defeated &&
            enemy.chargeState == BattleEnemy::ChargeState::Impact) {
            if (ModelInstance* model = FindModelById(scene_,
                    "battle_" + enemy.id)) {
                const auto& tuning = battle.GetConfig().enemy;
                model->scaleAxes = {
                    model->scaleAxes.x * tuning.chargeImpactWidthScale,
                    model->scaleAxes.y * tuning.chargeImpactHeightScale,
                    model->scaleAxes.z * tuning.chargeImpactDepthScale};
            }
        }
        if (enemy.type == BattleEnemyType::ChargeUnit &&
            enemy.chargeState == BattleEnemy::ChargeState::Charging) {
            add(enemy.id + "_flame", "flame",
                {displayPosition.x - enemy.chargeDirection.x * 1.2f,
                    displayPosition.y,
                    displayPosition.z - enemy.chargeDirection.z * 1.2f},
                1.0f, {1.0f, 1.0f, 1.0f});
            if (ModelInstance* flame = FindModelById(scene_,
                    "battle_" + enemy.id + "_flame")) {
                flame->scaleAxes = {0.5f, 1.15f, 0.5f};
                flame->transparent = true;
                flame->renderLayer = ModelRenderLayer::AdditiveEffect;
                Rotate(*flame, {std::numbers::pi_v<float> * 0.5f,
                    std::atan2(-enemy.chargeDirection.x,
                        -enemy.chargeDirection.z), 0.0f});
            }
        }
        if (enemy.type == BattleEnemyType::Bomber && !enemy.defeated &&
            enemy.deployed) {
            add(enemy.id + "_carried_bomb", "floating_bomb",
                displayPosition, 1.0f,
                {1.0f, 1.0f, 1.0f});
            if (ModelInstance* bomb = FindModelById(scene_,
                    "battle_" + enemy.id + "_carried_bomb")) {
                bomb->scale = enemy.carriedBombScale;
                const float age = enemy.previousVisualAge +
                    (enemy.visualAge - enemy.previousVisualAge) *
                    interpolationAlpha;
                Rotate(*bomb, {0.0f, age * 5.0f, 0.0f});
            }
        }
    }
    for (const BattleProjectile& projectile : battle.GetProjectiles()) {
        const Vector3 displayPosition = Interpolate(
            projectile.previousPosition, projectile.position,
            interpolationAlpha);
        const std::string_view key = projectile.type == BattleProjectileType::Missile
            ? "missile" : projectile.type == BattleProjectileType::FloatingBomb
            ? "floating_bomb" : projectile.type == BattleProjectileType::CrystalSpike
            ? "boss_crystal_spike" : "boss_crystal_cube";
        add(projectile.id, key, displayPosition,
            1.0f,
            {1.0f, 1.0f, 1.0f});
        if (ModelInstance* model = FindModelById(scene_, "battle_" + projectile.id)) {
            const Vector3 direction = Interpolate(
                projectile.previousDirection, projectile.direction,
                interpolationAlpha);
            const float age = projectile.previousAge +
                (projectile.age - projectile.previousAge) * interpolationAlpha;
            if (projectile.type == BattleProjectileType::Missile)
                Rotate(*model, {0.0f,
                    std::atan2(direction.x, direction.z) +
                        std::numbers::pi_v<float>, 0.0f});
            else if (projectile.type == BattleProjectileType::CrystalSpike)
                Rotate(*model, {std::numbers::pi_v<float> * 0.5f,
                    std::atan2(direction.x, direction.z), 0.0f});
            else if (projectile.type == BattleProjectileType::CrystalCube)
                Rotate(*model, {age * 4.1f,
                    age * 5.7f, age * 3.3f});
            else
                Rotate(*model, {age * 6.5f,
                    age * 5.1f, age * 3.8f});
        }
        if (projectile.type == BattleProjectileType::Missile) {
            const Vector3 direction = Interpolate(projectile.previousDirection,
                projectile.direction, interpolationAlpha);
            const Vector3 tail{-direction.x, -direction.y, -direction.z};
            add(projectile.id + "_tail", "flame", {
                displayPosition.x + tail.x * 0.663f,
                displayPosition.y + tail.y * 0.663f,
                displayPosition.z + tail.z * 0.663f},
                1.0f, {1.0f, 1.0f, 1.0f});
            if (ModelInstance* flame = FindModelById(scene_,
                    "battle_" + projectile.id + "_tail")) {
                flame->scaleAxes = {0.12f, 0.213f, 0.12f};
                flame->transparent = true;
                flame->renderLayer = ModelRenderLayer::AdditiveEffect;
                Rotate(*flame, {std::numbers::pi_v<float> * 0.5f,
                    std::atan2(tail.x, tail.z), 0.0f});
            }
        }
    }
    if (const BattleBoss* boss = battle.GetBoss();
        boss && boss->GetMode() != BossMode::Defeated) {
        const Vector3 position{
            boss->GetPreviousPosition().x +
                (boss->GetPosition().x - boss->GetPreviousPosition().x) *
                interpolationAlpha,
            boss->GetPreviousPosition().y +
                (boss->GetPosition().y - boss->GetPreviousPosition().y) *
                interpolationAlpha,
            boss->GetPreviousPosition().z +
                (boss->GetPosition().z - boss->GetPreviousPosition().z) *
                interpolationAlpha};
        const BossVisualPose pose = boss->GetInterpolatedVisualPose(
            interpolationAlpha);
        ModelInstance root;
        root.position = position;
        root.initialXDegrees = 0.0f;
        root.initialYDegrees = 0.0f;
        root.rotationXDegreesPerSecond = 0.0f;
        root.rotationYDegreesPerSecond = 0.0f;
        Rotate(root, pose.rootRotation);
        Matrix4x4 rootWorld{};
        const bool hasRoot = TryBuildModelWorldTransform(scene_, root, rootWorld);
        if (hasRoot) {
            add("boss_core", "boss_core", {}, 1.0f,
                boss->GetMode() == BossMode::Defeating
                    ? Vector3{1.0f, 0.35f, 0.35f}
                    : boss->GetCoreHitFlashRemaining() > 0.0f
                    ? Vector3{1.0f, 0.15f, 0.15f}
                    : Vector3{1.0f, 1.0f, 1.0f});
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
                    pose.cones[index].translation, 1.0f,
                    {1.0f, 1.0f, 1.0f});
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
                    {1.0f, 1.0f, 1.0f});
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
                    {1.0f, 1.0f, 1.0f});
                if (ModelInstance* crystal = FindModelById(scene_,
                        "battle_boss_cube_slot_" + std::to_string(index))) {
                    if (hasRoot) crystal->parentWorld = rootWorld;
                    Rotate(*crystal, pose.cubes[index].rotation);
                }
            }
        }
    }
    const FirstPersonGame& player = battle.GetPlayerGame();
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
            {1.0f, 1.0f, 1.0f});
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
                flame.colorTint = {1.0f, 1.0f, 1.0f};
                flame.transparent = true;
                flame.renderLayer = ModelRenderLayer::AdditiveEffect;
                scene_.models.push_back(std::move(flame));
            }
        }
    }
}
