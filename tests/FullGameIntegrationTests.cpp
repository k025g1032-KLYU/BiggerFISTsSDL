#include "Data/BattleCombatConfig.h"
#include "Data/StoryConfig.h"
#include "Game/FirstPersonGame.h"
#include "Game/GameFlow.h"
#include "Gameplay/Combat/CombatSession.h"
#include "Gameplay/Combat/EncounterConfig.h"
#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace {
constexpr float kStep = 1.0f / 60.0f;

void AimAtBattle(CombatSession& session, FirstPersonGame& player,
    GameInput& input, float& mouseX, std::string& selectedId,
    bool& dodgeWasDown, int& attacks, int steps) {
    const BattleBoss* boss = session.GetBoss();
    if (session.GetPhase() != BattlePhase::Playing) return;
    if (boss || session.GetWaveCount() > 0) {
        input.moveX = (steps / 180) % 2 == 0 ? 1.0f : -1.0f;
        bool threat = false;
        for (const BattleEnemy& enemy : session.GetEnemies()) {
            if (!enemy.defeated && enemy.type == BattleEnemyType::ChargeUnit &&
                enemy.chargeState == BattleEnemy::ChargeState::Charging) {
                const Vector3& position = player.GetPlayer().GetPosition();
                threat |= std::hypot(enemy.position.x - position.x,
                    enemy.position.z - position.z) < 14.0f;
            }
        }
        if (boss) {
            const Vector3& position = player.GetPlayer().GetPosition();
            threat |= (boss->GetMode() == BossMode::Charge ||
                boss->GetMode() == BossMode::Boxer ||
                boss->GetMode() == BossMode::NeedleCombo) &&
                std::hypot(boss->GetPosition().x - position.x,
                    boss->GetPosition().z - position.z) < 12.0f;
        }
        input.dodge.down = threat;
        input.dodge.pressed = threat && !dodgeWasDown;
        dodgeWasDown = threat;
    }
    Vector3 target{};
    bool found = false;
    if (boss) {
        target = boss->GetPosition();
        found = true;
    } else {
        for (const BattleEnemy& enemy : session.GetEnemies()) {
            if (!enemy.defeated && enemy.deployed && enemy.id == selectedId) {
                target = enemy.position;
                found = true;
                break;
            }
        }
        if (!found) {
            selectedId.clear();
            for (const BattleEnemyType type : {BattleEnemyType::MissileUnit,
                    BattleEnemyType::Bomber, BattleEnemyType::ChargeUnit}) {
                for (const BattleEnemy& enemy : session.GetEnemies()) {
                    if (!enemy.defeated && enemy.deployed && enemy.type == type) {
                        target = enemy.position;
                        selectedId = enemy.id;
                        found = true;
                        break;
                    }
                }
                if (found) break;
            }
        }
    }
    if (!found) return;
    const Vector3 origin = player.GetPlayer().GetPosition();
    float yaw = std::atan2(target.x - origin.x, target.z - origin.z);
    if (!boss) {
        const float distance = std::hypot(target.x - origin.x, target.z - origin.z);
        const PunchArmState left = player.GetLeftArm().GetState();
        const bool useLeft = left == PunchArmState::Ready ||
            left == PunchArmState::Charging || left == PunchArmState::Charged;
        yaw += (useLeft ? 1.0f : -1.0f) *
            std::atan2(FirstPersonTuning::kShoulderHalfWidth,
                std::max(1.0f, distance - 4.0f));
    }
    mouseX = std::clamp(std::remainder(yaw - player.GetCamera().GetYaw(),
        6.283185307f) / FirstPersonTuning::kMouseLookSensitivity,
        -75.0f, 75.0f);
    const float bodyError = std::abs(std::remainder(yaw -
        player.GetPlayer().GetBodyYaw(), 6.283185307f));
    if (boss) {
        if ((bodyError < 0.045f && player.GetUltimateState() == UltimateState::Idle) ||
            player.GetUltimateState() == UltimateState::Forming ||
            player.GetUltimateState() == UltimateState::Ready) {
            input.leftMouseDown = input.rightMouseDown = true;
        }
        if (player.GetUltimateState() == UltimateState::Ready) {
            input.leftMouseDown = input.rightMouseDown = false;
            ++attacks;
        }
    } else if (bodyError < 0.055f) {
        const PunchArmState left = player.GetLeftArm().GetState();
        const PunchArmState right = player.GetRightArm().GetState();
        if (left == PunchArmState::Ready || left == PunchArmState::Charging)
            input.leftMouseDown = true;
        else if (left == PunchArmState::Charged) ++attacks;
        if (left != PunchArmState::Ready && left != PunchArmState::Charging &&
            left != PunchArmState::Charged) {
            if (right == PunchArmState::Ready || right == PunchArmState::Charging)
                input.rightMouseDown = true;
            else if (right == PunchArmState::Charged) ++attacks;
        }
    }
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    const std::filesystem::path configDirectory(argv[1]);
    StoryConfig stories;
    BattleCombatConfig combatConfig;
    EncounterConfig day1;
    EncounterConfig day2;
    std::string error;
    if (!LoadStoryConfig(configDirectory / "story-pages.cfg", stories, error) ||
        !LoadBattleCombatConfig(configDirectory, combatConfig, error) ||
        !LoadEncounterConfig(configDirectory / "day1-encounter.cfg", day1, error) ||
        !LoadEncounterConfig(configDirectory / "day2-encounter.cfg", day2, error)) {
        std::printf("FAIL: %s\n", error.c_str());
        return 1;
    }
    FirstPersonGame player(GameConfig{}, {0.0f, 5.85f, 0.0f},
        std::vector<FirstPersonTarget>{});
    GameFlow flow(stories);
    std::optional<CombatSession> battle;
    std::string selectedId;
    bool dodgeWasDown = false;
    int attacks[3]{};
    bool cleared[3]{};
    bool sawStory0 = false;
    bool sawEnding = false;
    bool returnedTitle = false;
    for (int step = 0; step < 60000; ++step) {
        const FlowScreenState screen = flow.GetScreenState();
        GameInput input{};
        if (screen.phase == FlowPhase::Title && step == 0)
            input.confirm.pressed = true;
        if (screen.presentation == FlowPresentation::Story) {
            if (screen.phase == FlowPhase::Prologue) {
                input.confirm.pressed = true;
            } else {
                input.skip.pressed = true;
            }
        }
        if (screen.phase == FlowPhase::Story0) sawStory0 = true;
        if (screen.phase == FlowPhase::Ending) sawEnding = true;
        if (screen.phase == FlowPhase::Tutorial &&
            screen.presentation == FlowPresentation::Normal)
            input.skipPhase.pressed = true;
        if (screen.presentation == FlowPresentation::EndingHold)
            input.confirm.pressed = true;
        FlowBattleResult result = FlowBattleResult::None;
        if (battle) {
            result = battle->GetPhase() == BattlePhase::Clear
                ? FlowBattleResult::Clear : battle->GetPhase() == BattlePhase::GameOver
                ? FlowBattleResult::GameOver : FlowBattleResult::Playing;
        }
        flow.Update(kStep, input, result, true);
        if (flow.ConsumePhaseChanged()) {
            battle.reset();
            const FlowPhase phase = flow.GetPhase();
            if (phase == FlowPhase::Day1 || phase == FlowPhase::Day2 ||
                phase == FlowPhase::Boss) {
                const BattlePreviewKind kind = phase == FlowPhase::Day1
                    ? BattlePreviewKind::Day1 : phase == FlowPhase::Day2
                    ? BattlePreviewKind::Day2 : BattlePreviewKind::Boss;
                battle.emplace(kind, player,
                    phase == FlowPhase::Day1 ? day1 : phase == FlowPhase::Day2
                    ? day2 : EncounterConfig{}, combatConfig);
                selectedId.clear();
                dodgeWasDown = false;
            }
        }
        if (battle && flow.GetScreenState().presentation == FlowPresentation::Normal) {
            GameInput attackInput{};
            if (battle->GetPhase() == BattlePhase::Ready) attackInput.battleStart.pressed = true;
            const int index = flow.GetPhase() == FlowPhase::Day1 ? 0 :
                flow.GetPhase() == FlowPhase::Day2 ? 1 : 2;
            float mouseX = 0.0f;
            AimAtBattle(*battle, player, attackInput, mouseX,
                selectedId, dodgeWasDown, attacks[index], step);
            battle->Update(kStep, attackInput, mouseX, 0.0f);
            if (battle->GetPhase() == BattlePhase::GameOver) {
                std::printf("FAIL: GameOver in phase %d at step %d\n", index, step);
                return 1;
            }
            if (battle->GetPhase() == BattlePhase::Clear) cleared[index] = true;
        }
        if (step > 0 && screen.phase == FlowPhase::Ending &&
            flow.GetPhase() == FlowPhase::Title) {
            returnedTitle = true;
            break;
        }
    }
    if (!sawStory0 || !sawEnding || !returnedTitle ||
        !cleared[0] || !cleared[1] || !cleared[2] ||
        attacks[0] <= 0 || attacks[1] <= 0 || attacks[2] <= 0) {
        std::printf("FAIL: flow story0=%d ending=%d title=%d clear=%d%d%d attacks=%d,%d,%d\n",
            sawStory0, sawEnding, returnedTitle, cleared[0], cleared[1],
            cleared[2], attacks[0], attacks[1], attacks[2]);
        return 1;
    }
    std::printf("PASS: Title to Ending to Title with input-driven Day1, Day2 and Boss clears\n");
    return 0;
}
