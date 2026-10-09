#include "Gameplay/Cockpit/CockpitPresentation.h"

#include <algorithm>
#include <array>

namespace {
constexpr std::array kFullLines{
    "MAIN POWER ........ ONLINE", "CONTROL SYSTEM .... ONLINE",
    "ARM SYSTEM L ...... ONLINE", "ARM SYSTEM R ...... ONLINE",
    "BOOST SYSTEM ...... ONLINE", "COCKPIT LINK ...... CONNECTED"};
constexpr std::array kQuickLines{
    "MAIN POWER      OK", "ARM SYSTEM      OK", "BOOST SYSTEM    OK"};
constexpr std::array kBossLines{
    "TARGET SIGNAL ...... CONFIRMED", "COMBAT SYSTEM ...... ONLINE",
    "ARM SYSTEM ......... READY"};
float Smooth(float value) {
    const float t = std::clamp(value, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
}

void CockpitPresentation::Reset() { *this = {}; }

void CockpitPresentation::Start(CockpitMode mode) {
    if (mode != CockpitMode::FullStartup && mode != CockpitMode::QuickStartup &&
        mode != CockpitMode::BossStartup) return;
    mode_ = mode;
    elapsed_ = 0.0f;
    emergencyDuration_ = 0.0f;
    startCuePending_ = true;
}

bool CockpitPresentation::IsStartupActive() const {
    return mode_ == CockpitMode::FullStartup ||
        mode_ == CockpitMode::QuickStartup || mode_ == CockpitMode::BossStartup;
}

float CockpitPresentation::GetDuration() const {
    return mode_ == CockpitMode::FullStartup ? 4.8f : 1.5f;
}

void CockpitPresentation::Update(float seconds) {
    if (!IsStartupActive()) return;
    elapsed_ += std::max(0.0f, seconds);
    if (elapsed_ >= GetDuration()) {
        mode_ = CockpitMode::None;
        elapsed_ = 0.0f;
    }
}

void CockpitPresentation::SetEmergency(bool offline, float remaining) {
    if (!offline) {
        if (mode_ == CockpitMode::EmergencyReboot) Reset();
        return;
    }
    if (mode_ != CockpitMode::EmergencyReboot) {
        mode_ = CockpitMode::EmergencyReboot;
        emergencyDuration_ = std::max(0.01f, remaining);
    }
    elapsed_ = std::clamp(emergencyDuration_ - remaining, 0.0f,
        emergencyDuration_);
}

int CockpitPresentation::GetVisibleLineCount() const {
    if (!IsStartupActive()) return 0;
    const float interval = mode_ == CockpitMode::FullStartup ? 0.45f : 0.2f;
    const int count = mode_ == CockpitMode::FullStartup ? 6 : 3;
    return std::min(count, static_cast<int>(elapsed_ / interval) + 1);
}

const char* CockpitPresentation::GetTitle() const {
    switch (mode_) {
    case CockpitMode::FullStartup: return "COCKPIT INITIALIZATION";
    case CockpitMode::QuickStartup: return "COMBAT STARTUP";
    case CockpitMode::BossStartup: return "BOSS ENGAGEMENT";
    default: return "";
    }
}

const char* CockpitPresentation::GetLine(int index) const {
    if (index < 0 || index >= GetVisibleLineCount()) return "";
    switch (mode_) {
    case CockpitMode::FullStartup: return kFullLines[index];
    case CockpitMode::QuickStartup: return kQuickLines[index];
    case CockpitMode::BossStartup: return kBossLines[index];
    default: return "";
    }
}

bool CockpitPresentation::IsAllGreenVisible() const {
    if (mode_ == CockpitMode::EmergencyReboot)
        return elapsed_ >= emergencyDuration_ - 1.0f;
    if (!IsStartupActive()) return false;
    const float interval = mode_ == CockpitMode::FullStartup ? 0.45f : 0.2f;
    const int count = mode_ == CockpitMode::FullStartup ? 6 : 3;
    return elapsed_ >= interval * count + 0.35f;
}

float CockpitPresentation::GetOverlayAlpha() const {
    if (mode_ == CockpitMode::FullStartup) return 0.94f;
    if (mode_ == CockpitMode::EmergencyReboot)
        return 0.9f * Smooth(elapsed_ / 0.5f);
    return IsStartupActive() ? 0.78f : 0.0f;
}

float CockpitPresentation::GetCheckActivation(int index) const {
    if (!IsStartupActive()) return 1.0f;
    const float interval = mode_ == CockpitMode::FullStartup ? 0.45f : 0.2f;
    return Smooth((elapsed_ - interval * index) / 0.65f);
}

CockpitActivation CockpitPresentation::GetActivation() const {
    if (mode_ == CockpitMode::EmergencyReboot) {
        const float rebootStart = std::max(0.0f, emergencyDuration_ - 1.0f);
        const float value = elapsed_ < 0.5f
            ? 1.0f - Smooth(elapsed_ / 0.5f)
            : elapsed_ >= rebootStart
            ? Smooth((elapsed_ - rebootStart) / 1.0f) : 0.0f;
        return {value, value, value, value};
    }
    if (!IsStartupActive()) return {};
    if (mode_ == CockpitMode::FullStartup)
        return {GetCheckActivation(0), GetCheckActivation(1),
            GetCheckActivation(2), GetCheckActivation(3)};
    if (mode_ == CockpitMode::BossStartup) {
        const float arms = GetCheckActivation(2);
        return {GetCheckActivation(1), GetCheckActivation(0), arms, arms};
    }
    const float arms = GetCheckActivation(1);
    return {GetCheckActivation(0), GetCheckActivation(2), arms, arms};
}

CockpitHudVisibility CockpitPresentation::GetHudVisibility() const {
    if (!IsStartupActive()) return {};
    CockpitHudVisibility visibility{false, false, false, false,
        false, false, false};
    const int lines = GetVisibleLineCount();
    if (mode_ == CockpitMode::FullStartup) {
        visibility.base = lines >= 1;
        visibility.armor = visibility.shield = lines >= 2;
        visibility.leftArm = lines >= 3;
        visibility.rightArm = lines >= 4;
        visibility.dodge = lines >= 5;
        visibility.targets = lines >= 6;
    } else if (mode_ == CockpitMode::QuickStartup) {
        visibility.base = visibility.armor = visibility.shield = lines >= 1;
        visibility.leftArm = visibility.rightArm = lines >= 2;
        visibility.dodge = visibility.targets = lines >= 3;
    } else {
        visibility.targets = lines >= 1;
        visibility.base = visibility.armor = visibility.shield =
            visibility.dodge = lines >= 2;
        visibility.leftArm = visibility.rightArm = lines >= 3;
    }
    return visibility;
}

bool CockpitPresentation::ConsumeStartCue() {
    const bool pending = startCuePending_;
    startCuePending_ = false;
    return pending;
}
