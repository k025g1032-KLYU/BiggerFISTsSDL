#include "Game/GameFlow.h"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace {
std::string_view PhaseName(FlowPhase phase) {
    switch (phase) {
    case FlowPhase::Prologue: return "Prologue";
    case FlowPhase::Story0: return "Story0";
    case FlowPhase::Story1: return "Story1";
    case FlowPhase::Story2: return "Story2";
    case FlowPhase::Story3: return "Story3";
    case FlowPhase::Ending: return "Ending";
    default: return {};
    }
}
float Approach(float value, float target, float distance) {
    return value < target ? std::min(value + distance, target) :
        std::max(value - distance, target);
}
}

GameFlow::GameFlow(const StoryConfig& config) : config_(config) {}

bool GameFlow::IsBattlePhase() const {
    return phase_ == FlowPhase::Day1 || phase_ == FlowPhase::Day2 || phase_ == FlowPhase::Boss;
}

bool GameFlow::ConsumePhaseChanged() {
    const bool changed = phaseChanged_;
    phaseChanged_ = false;
    return changed;
}

std::size_t GameFlow::FirstPageIndex(FlowPhase phase) const {
    const auto name = PhaseName(phase);
    for (std::size_t index = 0; index < config_.pages.size(); ++index) {
        if (config_.pages[index].phase == name) return index;
    }
    return config_.pages.size();
}

const StoryPage* GameFlow::CurrentPage() const {
    if (pageIndex_ >= config_.pages.size() ||
        config_.pages[pageIndex_].phase != PhaseName(phase_)) return nullptr;
    return &config_.pages[pageIndex_];
}

bool GameFlow::HasNextPage() const {
    return pageIndex_ + 1 < config_.pages.size() &&
        config_.pages[pageIndex_ + 1].phase == PhaseName(phase_);
}

void GameFlow::BeginTransition(FlowPhase next) {
    pendingPhase_ = next;
    presentation_ = FlowPresentation::Closing;
    curtainAmount_ = 0.0f;
}

void GameFlow::EnterPhase(FlowPhase next) {
    phase_ = next;
    phaseChanged_ = true;
}

void GameFlow::StartStory() {
    pageIndex_ = FirstPageIndex(phase_);
    dialogueIndex_ = 0;
    skipping_ = false;
    exiting_ = false;
    fastPrologue_ = false;
    pageY_ = 720.0f;
    if (const StoryPage* page = CurrentPage()) {
        targetY_ = page->autoScroll ? 720.0f : page->firstY;
    }
    presentation_ = FlowPresentation::Story;
}

void GameFlow::NextStoryPage() {
    ++pageIndex_;
    dialogueIndex_ = 0;
    pageY_ = 720.0f;
    targetY_ = CurrentPage() ? CurrentPage()->firstY : 720.0f;
    exiting_ = false;
}

void GameFlow::FinishStory(bool skipped) {
    switch (phase_) {
    case FlowPhase::Prologue:
        EnterPhase(skipped ? FlowPhase::Tutorial : FlowPhase::Story0);
        if (skipped) presentation_ = FlowPresentation::Opening;
        else StartStory();
        break;
    case FlowPhase::Story0: EnterPhase(FlowPhase::Tutorial); presentation_ = FlowPresentation::Opening; break;
    case FlowPhase::Story1: EnterPhase(FlowPhase::Day1); presentation_ = FlowPresentation::Opening; break;
    case FlowPhase::Story2: EnterPhase(FlowPhase::Day2); presentation_ = FlowPresentation::Opening; break;
    case FlowPhase::Story3: EnterPhase(FlowPhase::Boss); presentation_ = FlowPresentation::Opening; break;
    case FlowPhase::Ending:
        pageIndex_ = config_.pages.size() - 1;
        pageY_ = (720.0f - config_.pages[pageIndex_].height) * 0.5f;
        presentation_ = FlowPresentation::EndingHold;
        break;
    default: break;
    }
}

void GameFlow::Update(float seconds, const GameInput& input,
    FlowBattleResult battleResult, bool tutorialComplete) {
    if (presentation_ == FlowPresentation::Closing) {
        curtainAmount_ = std::min(1.0f, curtainAmount_ + seconds / config_.curtainSeconds);
        if (curtainAmount_ >= 1.0f) {
            EnterPhase(pendingPhase_);
            if (CurrentPage() || FirstPageIndex(phase_) < config_.pages.size()) StartStory();
            else presentation_ = FlowPresentation::Opening;
        }
        return;
    }
    if (presentation_ == FlowPresentation::Opening) {
        curtainAmount_ = std::max(0.0f, curtainAmount_ - seconds / config_.curtainSeconds);
        if (curtainAmount_ <= 0.0f) presentation_ = FlowPresentation::Normal;
        return;
    }
    if (presentation_ == FlowPresentation::EndingHold) {
        if (input.confirm.pressed || input.skip.pressed) {
            EnterPhase(FlowPhase::Title);
            presentation_ = FlowPresentation::Opening;
        }
        return;
    }
    if (presentation_ == FlowPresentation::Story) {
        const StoryPage* page = CurrentPage();
        if (!page) return;
        if (input.skip.pressed) { skipping_ = true; exiting_ = false; }
        if (skipping_) {
            if (phase_ == FlowPhase::Ending && !HasNextPage()) {
                pageY_ = Approach(pageY_, targetY_, config_.skipSpeed * seconds);
                if (std::abs(pageY_ - targetY_) < 0.01f) FinishStory(true);
            } else {
                pageY_ = Approach(pageY_, -page->height, config_.skipSpeed * seconds);
                if (pageY_ <= -page->height) {
                    if (phase_ == FlowPhase::Ending && HasNextPage()) NextStoryPage();
                    else FinishStory(true);
                }
            }
            return;
        }
        if (page->autoScroll) {
            if (input.confirm.pressed) fastPrologue_ = true;
            pageY_ -= (fastPrologue_ ? config_.prologueFastSpeed : config_.prologueSpeed) * seconds;
            if (pageY_ + page->height <= 0.0f) FinishStory(false);
            return;
        }
        if (exiting_) {
            pageY_ -= config_.exitSpeed * seconds;
            if (pageY_ + page->height <= 0.0f) {
                if (HasNextPage()) NextStoryPage();
                else FinishStory(false);
            }
            return;
        }
        pageY_ = Approach(pageY_, targetY_, config_.slideSpeed * seconds);
        if (input.confirm.pressed && std::abs(pageY_ - targetY_) < 0.01f) {
            if (dialogueIndex_ + 1 < page->dialogueCount) {
                ++dialogueIndex_;
                targetY_ = page->firstY - page->stepY * dialogueIndex_;
            } else if (phase_ == FlowPhase::Ending && !HasNextPage()) {
                FinishStory(false);
            } else {
                exiting_ = true;
            }
        }
        return;
    }
    if (input.skipPhase.pressed) {
        if (phase_ == FlowPhase::Tutorial && tutorialComplete)
            BeginTransition(FlowPhase::Story1);
        else if (phase_ == FlowPhase::Day1) BeginTransition(FlowPhase::Story2);
        else if (phase_ == FlowPhase::Day2) BeginTransition(FlowPhase::Story3);
        else if (phase_ == FlowPhase::Boss) BeginTransition(FlowPhase::Ending);
        return;
    }
    switch (phase_) {
    case FlowPhase::Title:
        if (input.confirm.pressed || input.battleStart.pressed) BeginTransition(FlowPhase::Prologue);
        break;
    case FlowPhase::Tutorial:
        if (tutorialComplete && input.battleStart.pressed) BeginTransition(FlowPhase::Story1);
        break;
    case FlowPhase::Day1:
        if (battleResult == FlowBattleResult::Clear) BeginTransition(FlowPhase::Story2);
        break;
    case FlowPhase::Day2:
        if (battleResult == FlowBattleResult::Clear) BeginTransition(FlowPhase::Story3);
        break;
    case FlowPhase::Boss:
        if (battleResult == FlowBattleResult::Clear) BeginTransition(FlowPhase::Ending);
        break;
    default: break;
    }
}

FlowScreenState GameFlow::GetScreenState() const {
    FlowScreenState state{};
    state.phase = phase_;
    state.presentation = presentation_;
    state.page = (presentation_ == FlowPresentation::Story ||
        presentation_ == FlowPresentation::EndingHold) ? CurrentPage() : nullptr;
    state.pageY = pageY_;
    state.curtainAmount = curtainAmount_;
    state.dialogueIndex = dialogueIndex_;
    state.showPrompt = presentation_ == FlowPresentation::Story &&
        state.page != nullptr;
    return state;
}
