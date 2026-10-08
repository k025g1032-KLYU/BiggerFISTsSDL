#include "Data/BattleCombatConfig.h"
#include "Data/BattleAssetCatalog.h"
#include "Game/FirstPersonGame.h"
#include "Gameplay/Combat/CombatSession.h"
#include "Gameplay/Combat/EncounterConfig.h"
#include "Gameplay/Player/FirstPersonTuning.h"
#include "Platform/FixedStepClock.h"
#include "Rendering/FirstPersonScenePresenter.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>

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
    for (int frame = 0; frame < renderFps * 240 &&
            session.GetPhase() != BattlePhase::Clear &&
            session.GetPhase() != BattlePhase::GameOver; ++frame) {
        const int count = clock.Advance(1.0 / renderFps);
        for (int index = 0; index < count &&
                session.GetPhase() != BattlePhase::Clear &&
                session.GetPhase() != BattlePhase::GameOver; ++index) {
            GameInput input{};
            if (kind == BattlePreviewKind::Day2 ||
                kind == BattlePreviewKind::Boss) {
                input.moveX = ((steps / 180) % 2 == 0) ? 1.0f : -1.0f;
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
                    chargeThreat |= (boss.GetMode() == BossMode::Charge ||
                        boss.GetMode() == BossMode::Boxer ||
                        boss.GetMode() == BossMode::NeedleCombo) &&
                        std::hypot(boss.GetPosition().x - playerPosition.x,
                            boss.GetPosition().z - playerPosition.z) < 12.0f;
                }
                input.dodge.down = chargeThreat;
                input.dodge.pressed = chargeThreat && !dodgeDown;
                dodgeDown = chargeThreat;
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
    if (!sawBattleModel || (session.GetPhase() == BattlePhase::Clear &&
            (kind == BattlePreviewKind::Boss
                ? !hasEndBattleModel || hasEndHalo
                : hasEndBattleModel)) ||
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
