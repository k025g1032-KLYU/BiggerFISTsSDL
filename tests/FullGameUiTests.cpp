#include "Rendering/FullGameUi.h"

#include "Data/GameConfig.h"
#include "Gameplay/Cockpit/CockpitPresentation.h"
#include "Gameplay/Combat/CombatSession.h"
#include "Gameplay/Tutorial/TutorialSession.h"
#include "Game/FirstPersonGame.h"
#include "Rendering/UiRenderer.h"
#include "Rendering/UiLayout.h"
#include "World/ModelScene.h"

#include <cmath>
#include <cstdio>
#include <string_view>
#include <utility>

namespace {
bool Check(bool condition, const char* name) {
    if (!condition) std::printf("FAIL: %s\n", name);
    return condition;
}

bool Near(float a, float b) { return std::abs(a - b) < 0.001f; }

int FindText(const UiDrawList& list, std::string_view text, float x, float y,
    float scale = 1.0f) {
    UiDrawList expected;
    expected.Text(text, x, y, scale);
    const auto& actual = list.GetCommands();
    const auto& wanted = expected.GetCommands();
    if (wanted.empty() || wanted.size() > actual.size()) return -1;
    for (std::size_t start = 0; start + wanted.size() <= actual.size(); ++start) {
        bool matches = true;
        for (std::size_t index = 0; index < wanted.size(); ++index) {
            const UiCommand& a = actual[start + index];
            const UiCommand& b = wanted[index];
            if (a.image != b.image || !Near(a.x, b.x) || !Near(a.y, b.y) ||
                !Near(a.u, b.u) || !Near(a.v, b.v) || !Near(a.width, b.width)) {
                matches = false;
                break;
            }
        }
        if (matches) return static_cast<int>(start);
    }
    return -1;
}

int FindRectangle(const UiDrawList& list, float x, float y, float width,
    float height, float alpha = -1.0f) {
    const auto& commands = list.GetCommands();
    for (std::size_t index = 0; index < commands.size(); ++index) {
        const UiCommand& command = commands[index];
        if (command.image == "white1x1.png" && Near(command.x, x) &&
            Near(command.y, y) && Near(command.width, width) &&
            Near(command.height, height) &&
            (alpha < 0.0f || Near(command.color.a, alpha)))
            return static_cast<int>(index);
    }
    return -1;
}

int CountLockBars(const UiDrawList& list, float red) {
    int count = 0;
    for (const UiCommand& command : list.GetCommands()) {
        if (command.image == "white1x1.png" && Near(command.width, 64.0f) &&
            Near(command.height, 3.0f) && Near(command.color.r, red))
            ++count;
    }
    return count;
}

void WaitTutorial(TutorialSession& tutorial, TutorialObservation& observation) {
    for (int tick = 0; tick < 80; ++tick)
        tutorial.Update(1.0f / 60.0f, {}, observation);
}
}

int main() {
    bool pass = true;
    FirstPersonGame player(GameConfig{}, {0.0f, 2.0f, -3.0f},
        Vector3{0.0f, 2.0f, 10.0f});
    ModelScene scene;
    UiDrawList list;
    FlowScreenState flow;
    CockpitPresentation cockpit;

    UiDrawList glyphs;
    glyphs.Text("A~", 10.0f, 20.0f, 1.25f);
    const auto& fontCommands = glyphs.GetCommands();
    pass &= Check(fontCommands.size() == 2 &&
        fontCommands[0].image == "debugfont.png" &&
        Near(fontCommands[0].width, 11.25f) &&
        Near(fontCommands[1].x, 21.25f) &&
        Near(fontCommands[0].u, 5.0f * 9.0f / 128.0f) &&
        Near(fontCommands[1].u, 10.0f * 9.0f / 128.0f) &&
        Near(fontCommands[1].v, 6.0f * 18.0f / 128.0f),
        "Font commands retain the original atlas cells and character advance");

    BuildFullGameUi(flow, nullptr, nullptr, player, 0.25, cockpit,
        false, scene, list);
    pass &= Check(list.GetCommands().size() >= 2 &&
        list.GetCommands()[0].image == "title.png" &&
        list.GetCommands()[0].placement == UiPlacement::Cover &&
        list.GetCommands()[1].image == "startbar.png" &&
        list.GetCommands()[1].placement == UiPlacement::Safe,
        "Title art fills the viewport while the prompt stays in the safe area");
    for (const auto [width, height] : {
            std::pair{1280.0f, 720.0f}, std::pair{1920.0f, 1200.0f},
            std::pair{2560.0f, 1080.0f}}) {
        const UiLayout layout(width, height);
        const UiBounds title = layout.Map(list.GetCommands()[0]);
        const UiBounds prompt = layout.Map(list.GetCommands()[1]);
        pass &= Check(title.left <= 0.001f && title.top <= 0.001f &&
            title.left + title.width >= width - 0.001f &&
            title.top + title.height >= height - 0.001f &&
            prompt.left >= 0.0f && prompt.left + prompt.width <= width,
            "Title covers 16:9, 16:10 and ultrawide without clipping the prompt");
    }
    BuildFullGameUi(flow, nullptr, nullptr, player, 0.75, cockpit,
        false, scene, list);
    pass &= Check(list.GetCommands().size() == 1 &&
        list.GetCommands()[0].image == "title.png", "Title blink off");

    StoryPage page{"Story1", "story1.png", 825.0f, 904.0f,
        7, 380.0f, 133.0f, false, true};
    flow.phase = FlowPhase::Story1;
    flow.presentation = FlowPresentation::Story;
    flow.page = &page;
    flow.showPrompt = true;
    BuildFullGameUi(flow, nullptr, nullptr, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(list.GetCommands()[0].placement == UiPlacement::FullViewport &&
        Near(UiLayout(2560.0f, 1080.0f).Map(list.GetCommands()[0]).width,
            2560.0f), "Story background fills the whole ultrawide viewport");
    pass &= Check(FindText(list, "SPACE / ENTER / A: NEXT", 1059, 650) >= 0 &&
        FindText(list, "K / START: SKIP", 1095, 682) >= 0 &&
        1059.0f + 23.0f * 9.0f <= 1280.0f,
        "Story prompts remain readable and inside the design width");
    const int storyPrompt = FindText(list, "SPACE / ENTER / A: NEXT",
        1059, 650);
    pass &= Check(storyPrompt >= 0 &&
        list.GetCommands()[storyPrompt].placement == UiPlacement::RightEdge,
        "Story prompt follows the output right edge without stretching glyphs");
    flow.phase = FlowPhase::Prologue;
    BuildFullGameUi(flow, nullptr, nullptr, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(FindText(list, "SPACE / ENTER / A: FAST FORWARD",
        987, 650) >= 0 && 987.0f + 31.0f * 9.0f <= 1280.0f,
        "Prologue fast-forward prompt remains inside the design width");
    flow.phase = FlowPhase::Ending;
    flow.presentation = FlowPresentation::EndingHold;
    BuildFullGameUi(flow, nullptr, nullptr, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(FindText(list, "MISSION COMPLETE", 500, 650, 1.3f) >= 0 &&
        FindText(list, "ENTER / A / K / START: RETURN TO TITLE",
            380, 682) >= 0,
        "Ending hold keeps the original return prompt");

    flow = {};
    flow.phase = FlowPhase::Tutorial;
    TutorialSession tutorial;
    tutorial.Start({});
    cockpit.Start(CockpitMode::FullStartup);
    cockpit.Update(1.35f);
    BuildFullGameUi(flow, &tutorial, nullptr, player, 0.0, cockpit,
        false, scene, list);
    const int overlay = FindRectangle(list, 0, 0, 1280, 720, 0.94f);
    const int base = FindText(list, "BASE HP", 600, 12, 1.2f);
    pass &= Check(overlay >= 0 && base > overlay,
        "Startup blackout is drawn before the enabled HUD");
    pass &= Check(list.GetCommands()[overlay].placement ==
        UiPlacement::FullViewport,
        "Cockpit blackout covers the entire output viewport");
    pass &= Check(FindText(list, "ARMOR", 44, 628, 1.2f) < 0,
        "Tutorial HUD visibility overrides startup line count");
    pass &= Check(FindText(list, "K / START: SKIP TUTORIAL", 1020, 690, 1.0f) < 0,
        "Tutorial skip prompt stays hidden during startup");

    cockpit.Reset();
    BuildFullGameUi(flow, &tutorial, nullptr, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(FindRectangle(list, 348, 100, 560, 122, 0.72f) >= 0,
        "Tutorial panel uses original dimensions");
    pass &= Check(FindText(list, "K / START: SKIP TUTORIAL", 1020, 690, 1.0f) >= 0 &&
        1020.0f + 24.0f * 9.0f <= 1280.0f,
        "Tutorial skip prompt remains on screen");
    pass &= Check(FindRectangle(list, 31, 579, 326, 116) < 0 &&
        FindRectangle(list, 486, 8, 320, 92) < 0,
        "Gameplay HUD has no extra panel backgrounds");
    BuildFullGameUi(flow, &tutorial, nullptr, player, 0.0, cockpit,
        true, scene, list);
    pass &= Check(FindText(list, "Combat Prototype v0.4", 16, 16) >= 0 &&
        FindText(list, "SESSION: TUTORIAL", 760, 16) >= 0 &&
        FindText(list, "Camera: First Person | Control: Driving | V/R3",
            16, 352) >= 0,
        "Tutorial data is available before a practice combat session exists");

    tutorial.SkipToFreePractice();
    BuildFullGameUi(flow, &tutorial, nullptr, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(FindText(list, "SYSTEM ALL GREEN", 360, 168) >= 0 &&
        FindText(list, "FREE PRACTICE - ALL FUNCTIONS UNLOCKED", 360, 190) >= 0 &&
        FindText(list, "ENTER / START: CONTINUE", 1020, 690, 1.0f) >= 0 &&
        1020.0f + 23.0f * 9.0f <= 1280.0f,
        "Free-practice prompt remains on screen");
    CombatSession practice(BattlePreviewKind::Practice, player);
    BuildFullGameUi(flow, &tutorial, &practice, player, 0.0, cockpit,
        true, scene, list);
    pass &= Check(FindText(list, "Combat Prototype v0.4", 16, 16) >= 0 &&
        FindText(list, "SESSION: TUTORIAL", 760, 16) >= 0 &&
        FindRectangle(list, 6, 7, 468, 128) < 0,
        "Practice data follows original layout without an extra panel");
    flow.phase = FlowPhase::Day1;
    BuildFullGameUi(flow, nullptr, &practice, player, 0.0, cockpit,
        true, scene, list);
    pass &= Check(FindText(list, "SESSION: DAY1", 760, 16) >= 0 &&
        FindText(list, "Ready Debug: 0 Boss", 16, 82) < 0,
        "Formal battle shows F3 data without practice spawn instructions");
    flow.phase = FlowPhase::Tutorial;

    GameInput spawnBoss;
    spawnBoss.practiceCommand = PracticeCommand::BossSpawn;
    practice.Update(1.0f / 60.0f, spawnBoss, 0.0f, 0.0f);
    const BattleBoss* practiceBoss = practice.GetBoss();
    pass &= Check(practiceBoss != nullptr, "Practice Boss command spawns a Boss");
    if (practiceBoss) {
        char bossLabel[64]{};
        std::snprintf(bossLabel, sizeof(bossLabel), "BOSS CORE %02d / %02d",
            practiceBoss->GetHitPoints(), practiceBoss->GetMaximumHitPoints());
        BuildFullGameUi(flow, &tutorial, &practice, player, 0.0, cockpit,
            false, scene, list);
        pass &= Check(FindText(list, bossLabel, 520, 96, 1.1f) >= 0,
            "Practice Boss HP appears with F3 data hidden");
        BuildFullGameUi(flow, &tutorial, &practice, player, 0.0, cockpit,
            true, scene, list);
        pass &= Check(FindText(list, bossLabel, 520, 96, 1.1f) < 0,
            "Practice F3 data replaces the regular Boss HP label");
    }

    TutorialSession steps;
    TutorialObservation observation;
    steps.Start(observation);
    WaitTutorial(steps, observation);
    observation.cameraYaw = 1.6f;
    GameInput connect;
    connect.switchView.pressed = true;
    steps.Update(1.0f / 60.0f, connect, observation);
    WaitTutorial(steps, observation);
    BuildFullGameUi(flow, &steps, nullptr, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(steps.GetStep() == TutorialStep::Mobility &&
        FindText(list, "COCKPIT MOBILITY CONTROL TEST", 360, 140, 1.25f) >= 0 &&
        FindText(list, "ARMOR", 44, 628, 1.2f) >= 0,
        "Mobility step connects the cockpit and enables armor");
    const int armorText = FindText(list, "ARMOR", 44, 628, 1.2f);
    pass &= Check(armorText >= 0 &&
        list.GetCommands()[armorText].placement == UiPlacement::LeftEdge &&
        Near(UiLayout(2560.0f, 1080.0f).Map(
            list.GetCommands()[armorText]).left, 66.0f),
        "Armor HUD follows the actual left edge on ultrawide screens");

    observation.cameraYaw = 2.2f;
    observation.playerPosition.z = 5.1f;
    steps.Update(1.0f / 60.0f, {}, observation);
    WaitTutorial(steps, observation);
    BuildFullGameUi(flow, &steps, nullptr, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(steps.GetStep() == TutorialStep::Punch &&
        FindText(list, "SHIFT + LMB / RMB: PUNCH   L WAIT   R WAIT",
            360, 168) >= 0 &&
        FindText(list, "L ARM", 440, 570, 1.3f) >= 0,
        "Punch step enables both arm indicators and shared progress");
    observation.leftPunchHit = true;
    steps.Update(1.0f / 60.0f, {}, observation);
    BuildFullGameUi(flow, &steps, nullptr, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(FindText(list,
        "SHIFT + LMB / RMB: PUNCH   L PASS   R WAIT", 360, 168) >= 0,
        "Punch progress reports each arm independently");
    observation.rightPunchHit = true;
    steps.Update(1.0f / 60.0f, {}, observation);
    observation.leftPunchHit = observation.rightPunchHit = false;
    WaitTutorial(steps, observation);
    BuildFullGameUi(flow, &steps, nullptr, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(steps.GetStep() == TutorialStep::RocketPunch &&
        FindText(list, "LMB / RMB: ROCKET PUNCH   L WAIT   R WAIT",
            360, 168) >= 0,
        "Rocket Punch step uses original shared progress line");

    observation.leftRocketHit = observation.rightRocketHit = true;
    steps.Update(1.0f / 60.0f, {}, observation);
    observation.leftRocketHit = observation.rightRocketHit = false;
    WaitTutorial(steps, observation);
    BuildFullGameUi(flow, &steps, nullptr, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(steps.GetStep() == TutorialStep::Defense &&
        FindText(list, "INTERCEPT THE MISSILE OR LET THE SHIELD ABSORB IT",
            360, 168) >= 0 &&
        FindText(list, "SHIELD", 44, 588, 1.2f) >= 0,
        "Defense step enables shield information");
    const RocketPreviewTrajectory lockPath =
        player.GetLeftArm().GetRocketPreviewTrajectory(
            player.GetPlayer().GetPosition(),
            player.GetPlayer().GetBodyYaw());
    const Vector3 lockPosition{
        lockPath.start.x + lockPath.direction.x * 9.0f,
        lockPath.start.y + lockPath.direction.y * 9.0f,
        lockPath.start.z + lockPath.direction.z * 9.0f};
    scene.cameraPosition = {0.0f, 5.85f, 0.0f};
    scene.cameraTarget = {0.0f, 5.85f, 15.0f};
    player.SpawnTarget({"target_tutorial_hidden", lockPosition,
        1, {}, false, 1.25f});
    BuildFullGameUi(flow, &steps, nullptr, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(CountLockBars(list, 0.20f) == 0,
        "Defense does not lock onto hidden tutorial targets");
    player.RemoveTarget("target_tutorial_hidden");
    player.SpawnTarget({"target_tutorial_missile", lockPosition,
        1, {}, false, 0.45f, 1.0f, TargetDisplayKind::Projectile});
    BuildFullGameUi(flow, &steps, nullptr, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(CountLockBars(list, 0.20f) > 0,
        "Defense locks onto the visible training projectile");
    BuildFullGameUi(flow, &steps, nullptr, player, 0.0, cockpit,
        false, scene, list, 0.0f, 1920, 1200);
    pass &= Check(CountLockBars(list, 0.20f) > 0,
        "Training projectile lock remains projected at 16:10");
    player.RemoveTarget("target_tutorial_missile");
    observation.playerHit = true;
    steps.Update(1.0f / 60.0f, {}, observation);
    observation.playerHit = false;
    WaitTutorial(steps, observation);
    BuildFullGameUi(flow, &steps, nullptr, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(steps.GetStep() == TutorialStep::Dodge &&
        FindText(list, "PRESS [SPACE] / [A] TO PERFORM A DASH",
            360, 168) >= 0 &&
        FindText(list, "DODGE", 600, 610, 1.3f) >= 0,
        "Dodge step enables the dodge indicator");

    flow.phase = FlowPhase::Day1;
    BuildFullGameUi(flow, nullptr, nullptr, player, 0.0, cockpit,
        false, scene, list, 0.4f);
    pass &= Check(FindRectangle(list, 0, 0, 1280, 720, 0.4f) >= 0,
        "HP damage draws the original red overlay");
    BuildFullGameUi(flow, nullptr, nullptr, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(FindText(list, "SHIELD", 44, 588, 1.2f) >= 0 &&
        FindRectangle(list, 164, 634, 13, 10, 0.95f) >= 0,
        "Shield and armor use original coordinates");
    player.ReceivePlayerHit(PlayerHitType::Projectile);
    BuildFullGameUi(flow, nullptr, nullptr, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(FindRectangle(list, 210, 593, 34, 10, 0.80f) >= 0,
        "Empty shield segments use the original tint and spacing");
    CombatSession day1(BattlePreviewKind::Day1, player);
    BuildFullGameUi(flow, nullptr, &day1, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(FindText(list, "READY - PRESS ENTER / GAMEPAD START TO START",
        405, 300, 1.5f) >= 0,
        "Ready prompt appears before battle starts");
    cockpit.Start(CockpitMode::FullStartup);
    BuildFullGameUi(flow, nullptr, &day1, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(FindText(list, "BASE HP", 600, 12, 1.2f) >= 0 &&
        FindText(list, "ARMOR", 44, 628, 1.2f) < 0 &&
        FindText(list, "READY - PRESS ENTER / GAMEPAD START TO START",
            405, 300, 1.5f) < 0,
        "Battle startup stages HUD and hides Ready text");
    cockpit.Reset();
    cockpit.SetEmergency(true, 3.0f);
    BuildFullGameUi(flow, nullptr, &day1, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(FindText(list, "WARNING", 560, 260, 1.8f) >= 0 &&
        FindText(list, "BASE HP", 600, 12, 1.2f) < 0,
        "Emergency shutdown covers the battle HUD");
    cockpit.SetEmergency(true, 0.5f);
    BuildFullGameUi(flow, nullptr, &day1, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(FindText(list, "REBOOTING...", 525, 230, 1.6f) >= 0 &&
        FindText(list, "SYSTEM ALL GREEN", 470, 465, 1.8f) >= 0,
        "Emergency reboot uses the original cue text");
    cockpit.Reset();
    flow.presentation = FlowPresentation::Closing;
    flow.curtainAmount = 0.5f;
    BuildFullGameUi(flow, nullptr, &day1, player, 0.0, cockpit,
        false, scene, list);
    pass &= Check(FindRectangle(list, 0, -360, 1280, 720) >= 0 &&
        FindText(list, "BASE HP", 600, 12, 1.2f) < 0,
        "Cinematic curtain suppresses battle HUD");
    flow.presentation = FlowPresentation::Normal;

    PlayerCombatConfig quickDodge{};
    quickDodge.dodgeCooldown = 0.0f;
    FirstPersonGame dodging(GameConfig{}, {0.0f, 2.0f, -3.0f},
        Vector3{0.0f, 2.0f, 10.0f}, quickDodge);
    GameInput dodge;
    dodge.dodge.pressed = true;
    dodging.Update(1.0 / 60.0, dodge, 0.0f, 0.0f);
    pass &= Check(dodging.GetPlayer().GetDodgeCooldownRemaining() <= 0.0f &&
        !dodging.GetPlayer().IsDodgeReady(),
        "Dodge state can remain unavailable after cooldown reaches zero");
    BuildFullGameUi(flow, nullptr, nullptr, dodging, 0.0, cockpit,
        false, scene, list);
    pass &= Check(FindText(list, "RECHARGING", 600, 638, 1.3f) >= 0,
        "Dodge HUD follows the state, not the timer alone");

    ModelScene targetScene;
    targetScene.cameraPosition = {0.0f, 0.7f, -3.0f};
    targetScene.cameraTarget = {0.0f, 0.7f, 10.0f};
    FirstPersonGame locking(GameConfig{}, {0.0f, 2.0f, -3.0f},
        Vector3{0.0f, 0.7f, 10.0f});
    BuildFullGameUi(flow, nullptr, nullptr, locking, 0.0, cockpit,
        false, targetScene, list);
    const int readyLeftLocks = CountLockBars(list, 0.20f);
    GameInput chargePunch;
    chargePunch.leftPunchKeyDown = true;
    locking.Update(1.0 / 60.0, chargePunch, 0.0f, 0.0f);
    BuildFullGameUi(flow, nullptr, nullptr, locking, 0.0, cockpit,
        false, targetScene, list);
    if (readyLeftLocks == 0 || CountLockBars(list, 0.20f) != 0 ||
        CountLockBars(list, 1.0f) == 0)
        std::printf("Lock bars ready-left=%d charging-left=%d charging-right=%d state=%d\n",
            readyLeftLocks, CountLockBars(list, 0.20f),
            CountLockBars(list, 1.0f), static_cast<int>(locking.GetLeftArm().GetState()));
    pass &= Check(readyLeftLocks > 0 && CountLockBars(list, 0.20f) == 0 &&
        CountLockBars(list, 1.0f) > 0,
        "Punch charging hides only its own Rocket lock frames");
    return pass ? 0 : 1;
}
