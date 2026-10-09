#include "Gameplay/Tutorial/TutorialSession.h"

#include <algorithm>
#include <cmath>

void TutorialSession::Start(const TutorialObservation& player) {
    *this = {};
    previousYaw_ = player.cameraYaw;
    previousPitch_ = player.cameraPitch;
    previousPosition_ = player.playerPosition;
    gate_ = 0.5f;
    cue_ = "personnel.wav";
}

GameInput TutorialSession::FilterInput(GameInput input) const {
    if (IsComplete()) return input;
    if (step_ == TutorialStep::VisualLink) {
        input.moveX = input.moveY = 0.0f;
        input.leftPunchKeyDown = input.rightPunchKeyDown = false;
        input.leftMouseDown = input.rightMouseDown = false;
        input.dodge = {};
    } else if (step_ == TutorialStep::Mobility) {
        input.leftPunchKeyDown = input.rightPunchKeyDown = false;
        input.leftMouseDown = input.rightMouseDown = false;
        input.dodge = {};
    } else if (step_ == TutorialStep::Punch) {
        input.leftMouseDown = input.rightMouseDown = false;
        input.dodge = {};
    } else if (step_ == TutorialStep::RocketPunch || step_ == TutorialStep::Defense) {
        input.dodge = {};
    }
    return input;
}

void TutorialSession::CompleteStep() {
    if (confirmation_ > 0.0f) return;
    confirmation_ = 0.6f;
    switch (step_) {
    case TutorialStep::VisualLink: cue_ = "system/system_visual_link_pass.wav"; break;
    case TutorialStep::Mobility: cue_ = "system/system_mobility_pass.wav"; break;
    case TutorialStep::Punch: cue_ = "system/system_punch_pass.wav"; break;
    case TutorialStep::RocketPunch: cue_ = "system/system_rocket_pass.wav"; break;
    case TutorialStep::Defense: cue_ = "system/system_defense_pass.wav"; break;
    case TutorialStep::Dodge: cue_ = "system/system_dodge_pass.wav"; break;
    case TutorialStep::Complete: break;
    }
}

void TutorialSession::Advance() {
    switch (step_) {
    case TutorialStep::VisualLink: step_ = TutorialStep::Mobility; break;
    case TutorialStep::Mobility: step_ = TutorialStep::Punch; break;
    case TutorialStep::Punch: step_ = TutorialStep::RocketPunch; break;
    case TutorialStep::RocketPunch: step_ = TutorialStep::Defense; break;
    case TutorialStep::Defense: step_ = TutorialStep::Dodge; break;
    case TutorialStep::Dodge: step_ = TutorialStep::Complete;
        freePracticeCaptionRemaining_ = 10.0f;
        cue_ = "system/system_all_green.wav"; break;
    case TutorialStep::Complete: break;
    }
    gate_ = 0.5f;
    confirmation_ = 0.0f;
    lookAngle_ = moveDistance_ = 0.0f;
    leftHit_ = rightHit_ = false;
    if (step_ != TutorialStep::Complete) cue_ = "personnel.wav";
}

void TutorialSession::Update(float seconds, const GameInput& input,
    const TutorialObservation& player) {
    if (IsComplete()) freePracticeCaptionRemaining_ =
        std::max(0.0f, freePracticeCaptionRemaining_ - seconds);
    gate_ = std::max(0.0f, gate_ - seconds);
    const float yaw = player.cameraYaw;
    const float pitch = player.cameraPitch;
    const Vector3 position = player.playerPosition;
    lookAngle_ += std::abs(std::remainder(yaw - previousYaw_, 6.2831853f)) +
        std::abs(pitch - previousPitch_);
    moveDistance_ += std::hypot(position.x - previousPosition_.x,
        position.z - previousPosition_.z);
    previousYaw_ = yaw;
    previousPitch_ = pitch;
    previousPosition_ = position;
    if (confirmation_ > 0.0f) {
        confirmation_ -= seconds;
        if (confirmation_ <= 0.0f) Advance();
        return;
    }
    if (gate_ > 0.0f || IsComplete()) return;
    switch (step_) {
    case TutorialStep::VisualLink:
        if (lookAngle_ >= 1.5707963f && input.switchView.pressed) {
            cockpitConnected_ = true;
            CompleteStep();
        }
        break;
    case TutorialStep::Mobility:
        if (moveDistance_ >= 5.0f && lookAngle_ >= 0.5235988f) CompleteStep();
        break;
    case TutorialStep::Punch:
    case TutorialStep::RocketPunch:
        leftHit_ |= step_ == TutorialStep::Punch ? player.leftPunchHit : player.leftRocketHit;
        rightHit_ |= step_ == TutorialStep::Punch ? player.rightPunchHit : player.rightRocketHit;
        if (leftHit_ && rightHit_ && (step_ == TutorialStep::Punch ||
            (!player.leftFistAway && !player.rightFistAway))) {
            CompleteStep();
        }
        break;
    case TutorialStep::Defense:
        if (player.playerHit || player.defenseIntercepted) CompleteStep();
        break;
    case TutorialStep::Dodge:
        if (player.dodgeStarted) CompleteStep();
        break;
    case TutorialStep::Complete: break;
    }
}

void TutorialSession::SkipToFreePractice() {
    step_ = TutorialStep::Complete;
    confirmation_ = gate_ = 0.0f;
    cockpitConnected_ = true;
    freePracticeCaptionRemaining_ = 10.0f;
    cue_ = "system/system_all_green.wav";
}

bool TutorialSession::ShouldShowTargets() const {
    return step_ == TutorialStep::Punch || step_ == TutorialStep::RocketPunch ||
        IsComplete();
}

int TutorialSession::GetStepNumber() const {
    return std::min(6, static_cast<int>(step_) + 1);
}

const char* TutorialSession::GetTitle() const {
    switch (step_) {
    case TutorialStep::VisualLink: return "VISUAL LINK CHECK";
    case TutorialStep::Mobility: return "COCKPIT MOBILITY CONTROL TEST";
    case TutorialStep::Punch: return "LEFT / RIGHT PUNCH TEST";
    case TutorialStep::RocketPunch: return "LEFT / RIGHT ROCKET ARM TEST";
    case TutorialStep::Defense: return "DEFENSE RESPONSE TEST";
    case TutorialStep::Dodge: return "EVASION THRUSTER TEST";
    case TutorialStep::Complete: return "VERIFICATION COMPLETE";
    }
    return "";
}

const char* TutorialSession::GetInstruction() const {
    if (confirmation_ > 0.0f) return "RESPONSE CONFIRMED / CHECK PASSED";
    switch (step_) {
    case TutorialStep::VisualLink: return lookAngle_ >= 1.5707963f
        ? "PRESS [V] / [R3] TO CONNECT COCKPIT VIEW" : "MOVE MOUSE / RIGHT STICK TO SCAN";
    case TutorialStep::Mobility: return "WASD / LEFT STICK: MOVE; MOUSE / RIGHT STICK: LOOK";
    case TutorialStep::Punch: return "SHIFT + LMB / RMB OR TRIGGERS: HIT BOTH TARGETS";
    case TutorialStep::RocketPunch: return "LMB / RMB OR SHOULDERS: HIT BOTH TARGETS";
    case TutorialStep::Defense: return "SHIELD DEFENSE: INCOMING PROJECTILE";
    case TutorialStep::Dodge: return "SPACE / A: DODGE";
    case TutorialStep::Complete: return "SYSTEM ALL GREEN - ENTER / START TO CONTINUE";
    }
    return "";
}

const char* TutorialSession::ConsumeVoiceCue() {
    const char* result = cue_;
    cue_ = nullptr;
    return result;
}
