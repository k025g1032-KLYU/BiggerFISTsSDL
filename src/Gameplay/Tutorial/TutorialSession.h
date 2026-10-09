#pragma once

#include "Input/GameInput.h"
#include "Math/MathTypes.h"

struct TutorialObservation {
    float cameraYaw = 0.0f;
    float cameraPitch = 0.0f;
    Vector3 playerPosition{};
    bool leftPunchHit = false;
    bool rightPunchHit = false;
    bool leftRocketHit = false;
    bool rightRocketHit = false;
    bool leftFistAway = false;
    bool rightFistAway = false;
    bool playerHit = false;
    bool defenseIntercepted = false;
    bool dodgeStarted = false;
};

enum class TutorialStep { VisualLink, Mobility, Punch, RocketPunch, Defense, Dodge, Complete };

class TutorialSession {
public:
    void Start(const TutorialObservation& player);
    GameInput FilterInput(GameInput input) const;
    void Update(float seconds, const GameInput& input,
        const TutorialObservation& player);
    void SkipToFreePractice();
    TutorialStep GetStep() const { return step_; }
    bool IsComplete() const { return step_ == TutorialStep::Complete; }
    bool IsCockpitConnected() const { return cockpitConnected_; }
    bool ShouldShowTargets() const;
    int GetStepNumber() const;
    const char* GetTitle() const;
    const char* GetInstruction() const;
    float GetLookAngle() const { return lookAngle_; }
    float GetMoveDistance() const { return moveDistance_; }
    bool HasLeftHit() const { return leftHit_; }
    bool HasRightHit() const { return rightHit_; }
    bool IsConfirming() const { return confirmation_ > 0.0f; }
    bool ShowFreePracticeCaption() const {
        return IsComplete() && freePracticeCaptionRemaining_ > 0.0f;
    }
    const char* ConsumeVoiceCue();
private:
    void Advance();
    void CompleteStep();
    TutorialStep step_ = TutorialStep::VisualLink;
    float gate_ = 0.0f;
    float confirmation_ = 0.0f;
    float lookAngle_ = 0.0f;
    float moveDistance_ = 0.0f;
    float freePracticeCaptionRemaining_ = 0.0f;
    float previousYaw_ = 0.0f;
    float previousPitch_ = 0.0f;
    Vector3 previousPosition_{};
    bool cockpitConnected_ = false;
    bool leftHit_ = false;
    bool rightHit_ = false;
    mutable const char* cue_ = nullptr;
};
