#include "Data/StoryConfig.h"
#include "Game/GameFlow.h"
#include "Platform/FixedStepClock.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

namespace {
constexpr float kStep = 1.0f / 60.0f;
bool Check(bool condition, const char* description) {
    if (!condition) std::printf("FAIL: %s\n", description);
    return condition;
}
void Tick(GameFlow& flow, GameInput input = {},
    FlowBattleResult result = FlowBattleResult::None, bool tutorialComplete = false) {
    flow.Update(kStep, input, result, tutorialComplete);
}
bool WaitFor(GameFlow& flow, FlowPhase phase, FlowPresentation presentation,
    int limit = 2000) {
    for (int step = 0; step < limit; ++step) {
        if (flow.GetPhase() == phase &&
            flow.GetScreenState().presentation == presentation) return true;
        Tick(flow);
    }
    return false;
}
GameInput Confirm() { GameInput input; input.confirm.pressed = true; return input; }
GameInput Skip() { GameInput input; input.skip.pressed = true; return input; }

struct Replay { FlowPhase phase; FlowPresentation presentation; int updates; };
Replay ReplayIntro(const StoryConfig& config, int renderFps) {
    GameFlow flow(config);
    FixedStepClock clock;
    int updates = 0;
    for (int frame = 0; frame < renderFps * 4; ++frame) {
        const int count = clock.Advance(1.0 / renderFps);
        for (int step = 0; step < count; ++step) {
            GameInput input;
            input.confirm.pressed = updates == 0;
            flow.Update(kStep, input, FlowBattleResult::None, false);
            ++updates;
        }
    }
    return {flow.GetPhase(), flow.GetScreenState().presentation, updates};
}
}

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    StoryConfig config;
    std::string error;
    bool success = Check(LoadStoryConfig(std::filesystem::path(argv[1]) /
        "story-pages.cfg", config, error), "story config loads");
    if (!success) { std::printf("%s\n", error.c_str()); return 1; }
    const Replay a = ReplayIntro(config, 30);
    const Replay b = ReplayIntro(config, 60);
    const Replay c = ReplayIntro(config, 120);
    success &= Check(a.updates == 240 && b.updates == 240 && c.updates == 240 &&
        a.phase == b.phase && b.phase == c.phase &&
        a.presentation == b.presentation && b.presentation == c.presentation,
        "30/60/120 FPS have the same fixed update flow");

    GameFlow flow(config);
    success &= Check(flow.GetPhase() == FlowPhase::Title, "start at Title");
    Tick(flow, Confirm());
    success &= Check(WaitFor(flow, FlowPhase::Prologue, FlowPresentation::Story),
        "Title enters Prologue");
    success &= Check(flow.GetScreenState().showPrompt,
        "Prologue displays fast-forward prompt while scrolling");
    Tick(flow, Skip());
    success &= Check(WaitFor(flow, FlowPhase::Tutorial, FlowPresentation::Normal),
        "skipped Prologue enters Tutorial");
    GameInput f4; f4.skipPhase.pressed = true;
    Tick(flow, f4, FlowBattleResult::None, false);
    success &= Check(flow.GetPhase() == FlowPhase::Tutorial,
        "first F4 in Tutorial does not bypass free practice");
    Tick(flow, f4, FlowBattleResult::None, true);
    success &= Check(WaitFor(flow, FlowPhase::Story1, FlowPresentation::Story),
        "completed Tutorial enters Story1");
    Tick(flow, Skip());
    success &= Check(WaitFor(flow, FlowPhase::Day1, FlowPresentation::Normal),
        "Story1 skip enters Day1");
    Tick(flow, {}, FlowBattleResult::GameOver);
    success &= Check(flow.GetPhase() == FlowPhase::Day1,
        "GameOver stays in battle for retry");
    Tick(flow, {}, FlowBattleResult::Clear);
    success &= Check(WaitFor(flow, FlowPhase::Story2, FlowPresentation::Story),
        "Day1 Clear enters Story2");
    Tick(flow, Skip());
    success &= Check(WaitFor(flow, FlowPhase::Day2, FlowPresentation::Normal),
        "Story2 skip enters Day2");
    Tick(flow, {}, FlowBattleResult::Clear);
    success &= Check(WaitFor(flow, FlowPhase::Story3, FlowPresentation::Story),
        "Day2 Clear enters Story3");
    Tick(flow, Skip());
    success &= Check(WaitFor(flow, FlowPhase::Boss, FlowPresentation::Normal),
        "Story3 goes directly to Boss");
    Tick(flow, {}, FlowBattleResult::Clear);
    success &= Check(WaitFor(flow, FlowPhase::Ending, FlowPresentation::Story),
        "Boss Clear enters Ending");
    Tick(flow, Skip());
    success &= Check(WaitFor(flow, FlowPhase::Ending, FlowPresentation::EndingHold),
        "Ending skip settles on last image");
    Tick(flow, Confirm());
    success &= Check(WaitFor(flow, FlowPhase::Title, FlowPresentation::Normal),
        "Ending confirmation returns to Title");

    GameFlow normal(config);
    Tick(normal, Confirm());
    success &= Check(WaitFor(normal, FlowPhase::Prologue, FlowPresentation::Story),
        "normal path begins Prologue");
    GameInput fast = Confirm();
    Tick(normal, fast);
    success &= Check(WaitFor(normal, FlowPhase::Story0, FlowPresentation::Story),
        "normal Prologue enters Story0");
    success &= Check(normal.GetScreenState().showPrompt,
        "Story prompt remains visible while page enters");
    success &= Check(normal.GetScreenState().page &&
        normal.GetScreenState().page->image == "story0.png",
        "Story0 uses original long image");
    for (int step = 0; step < 100; ++step) Tick(normal);
    const float previousY = normal.GetScreenState().pageY;
    Tick(normal, Confirm());
    success &= Check(normal.GetScreenState().dialogueIndex == 1 &&
        normal.GetScreenState().pageY == previousY,
        "confirm advances the dialogue target before scrolling");
    Tick(normal, Skip());
    success &= Check(WaitFor(normal, FlowPhase::Tutorial, FlowPresentation::Normal),
        "Story0 skip enters Tutorial");
    return success ? 0 : 1;
}
