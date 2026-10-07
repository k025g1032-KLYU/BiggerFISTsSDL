#include "Rendering/FirstPersonScenePresenter.h"

#include "Data/BattleAssetCatalog.h"
#include "Gameplay/Combat/CombatSession.h"

#include <algorithm>
#include <cmath>
#include <string_view>
#include <utility>

void FirstPersonScenePresenter::SyncBattle(const CombatSession& battle,
    const BattleAssetCatalog& assets) {
    std::erase_if(scene_.models, [](const ModelInstance& model) {
        return model.id.starts_with("battle_");
    });
    const auto add = [&](const std::string& id, std::string_view key,
        const Vector3& position, float scale, Vector3 tint) {
        const ModelInstance* prototype = assets.Find(key);
        if (prototype == nullptr) {
            return;
        }
        ModelInstance model = *prototype;
        model.id = "battle_" + id;
        model.position = position;
        model.scale = scale;
        model.colorTint = tint;
        scene_.models.push_back(std::move(model));
    };
    for (const BattleEnemy& enemy : battle.GetEnemies()) {
        const std::string_view key = enemy.type == BattleEnemyType::MissileUnit
            ? "missile_unit" : enemy.type == BattleEnemyType::Bomber
            ? "bomber" : "charge_unit";
        const float scale = enemy.type == BattleEnemyType::MissileUnit ? 0.9f : 1.1f;
        const bool chargeWarning = enemy.type == BattleEnemyType::ChargeUnit &&
            (enemy.chargeState == BattleEnemy::ChargeState::Preparing ||
                enemy.chargeState == BattleEnemy::ChargeState::Charging);
        add(enemy.id, key, enemy.position, scale,
            enemy.defeated ? Vector3{1.0f, 0.35f, 0.25f} :
                chargeWarning ? Vector3{1.0f, 0.55f, 0.3f} :
                Vector3{1.0f, 1.0f, 1.0f});
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
        }
        if (enemy.type == BattleEnemyType::Bomber && !enemy.defeated &&
            enemy.deployed) {
            add(enemy.id + "_carried_bomb", "floating_bomb",
                {enemy.position.x, enemy.position.y - 1.25f,
                    enemy.position.z}, 0.58f,
                enemy.attackTimer < 1.0f ? Vector3{1.0f, 0.55f, 0.25f}
                    : Vector3{1.0f, 1.0f, 1.0f});
        }
    }
    for (const BattleProjectile& projectile : battle.GetProjectiles()) {
        const std::string_view key = projectile.type == BattleProjectileType::Missile
            ? "missile" : projectile.type == BattleProjectileType::FloatingBomb
            ? "floating_bomb" : projectile.type == BattleProjectileType::CrystalSpike
            ? "boss_crystal_spike" : "boss_crystal_cube";
        add(projectile.id, key, projectile.position,
            projectile.type == BattleProjectileType::FloatingBomb ? 1.4f : 1.0f,
            {1.0f, 1.0f, 1.0f});
    }
    if (const BattleBoss* boss = battle.GetBoss()) {
        const Vector3& position = boss->GetPosition();
        const bool warning = boss->GetMode() == BossMode::Charge ||
            boss->GetMode() == BossMode::SpreadCounter;
        const Vector3 tint = boss->GetMode() == BossMode::Defeating
            ? Vector3{1.0f, 0.35f, 0.25f}
            : boss->IsRocketDodging() ? Vector3{0.50f, 0.85f, 1.0f}
            : warning ? Vector3{1.0f, 0.55f, 0.45f}
            : Vector3{1.0f, 1.0f, 1.0f};
        add("boss_core", "boss_core", position, 1.0f, tint);
        add("boss_left_cone", "boss_cone_left",
            {position.x - 2.7f, position.y, position.z}, 1.0f, tint);
        add("boss_right_cone", "boss_cone_right",
            {position.x + 2.7f, position.y, position.z}, 1.0f, tint);
        add("boss_lower_cone", "boss_cone_lower",
            {position.x, position.y - 2.5f, position.z}, 1.0f, tint);
        const bool showHalo = boss->GetMode() != BossMode::Defeating &&
            boss->GetMode() != BossMode::Defeated;
        for (std::size_t index = 0;
                showHalo && index < boss->GetSpikeSlots().size(); ++index) {
            const BossCrystalSlot& slot = boss->GetSpikeSlots()[index];
            if (!slot.consumed && slot.reloadTime <= 0.0f) {
                add("boss_spike_slot_" + std::to_string(index),
                    "boss_crystal_spike", slot.position, 0.8f,
                    slot.launchTime > 0.0f ? Vector3{1.0f, 0.8f, 0.5f}
                        : Vector3{0.65f, 0.85f, 1.0f});
            }
        }
        for (std::size_t index = 0;
                showHalo && index < boss->GetCubeSlots().size(); ++index) {
            const BossCrystalSlot& slot = boss->GetCubeSlots()[index];
            if (!slot.consumed && slot.reloadTime <= 0.0f) {
                add("boss_cube_slot_" + std::to_string(index),
                    "boss_crystal_cube", slot.position, 0.7f,
                    slot.launchTime > 0.0f ? Vector3{1.0f, 0.8f, 0.5f}
                        : Vector3{0.65f, 0.85f, 1.0f});
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
    if (player.GetUltimateState() == UltimateState::Flying) {
        add("ultimate", "ultimate_fist", player.GetUltimatePosition(), 1.0f,
            {1.0f, 0.8f, 0.35f});
    } else if (player.GetUltimateState() == UltimateState::Returning) {
        if (ModelInstance* fist = FindModelById(scene_, "left_fist")) {
            fist->position = player.GetUltimateReturnFistPosition(PunchArmSide::Left);
        }
        if (ModelInstance* fist = FindModelById(scene_, "right_fist")) {
            fist->position = player.GetUltimateReturnFistPosition(PunchArmSide::Right);
        }
    }
}
