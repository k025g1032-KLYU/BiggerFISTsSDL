#pragma once

#include "Data/StoryConfig.h"
#include "Input/GameInput.h"

#include <cstddef>

enum class FlowPhase {
    Title, Prologue, Story0, Tutorial, Story1, Day1, Story2, Day2,
    Story3, Boss, Ending
};
enum class FlowPresentation { Normal, Closing, Story, Opening, EndingHold };
enum class FlowBattleResult { None, Ready, Playing, Clear, GameOver };

struct FlowScreenState {
    FlowPhase phase = FlowPhase::Title;
    FlowPresentation presentation = FlowPresentation::Normal;
    const StoryPage* page = nullptr;
    float pageY = 0.0f;
    float curtainAmount = 0.0f;
    int dialogueIndex = 0;
    bool showPrompt = false;
};

class GameFlow {
public:
    explicit GameFlow(const StoryConfig& config);
    void Update(float seconds, const GameInput& input,
        FlowBattleResult battleResult, bool tutorialComplete);
    FlowPhase GetPhase() const { return phase_; }
    FlowScreenState GetScreenState() const;
    bool IsBattlePhase() const;
    bool ConsumePhaseChanged();

private:
    void BeginTransition(FlowPhase next);
    void EnterPhase(FlowPhase next);
    void StartStory();
    void NextStoryPage();
    void FinishStory(bool skipped);
    const StoryPage* CurrentPage() const;
    std::size_t FirstPageIndex(FlowPhase phase) const;
    bool HasNextPage() const;

    StoryConfig config_;
    FlowPhase phase_ = FlowPhase::Title;
    FlowPhase pendingPhase_ = FlowPhase::Title;
    FlowPresentation presentation_ = FlowPresentation::Normal;
    std::size_t pageIndex_ = 0;
    int dialogueIndex_ = 0;
    float pageY_ = 0.0f;
    float targetY_ = 0.0f;
    float curtainAmount_ = 0.0f;
    bool skipping_ = false;
    bool exiting_ = false;
    bool fastPrologue_ = false;
    bool phaseChanged_ = true;
};
