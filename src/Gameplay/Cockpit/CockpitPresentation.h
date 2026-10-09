#pragma once

enum class CockpitMode { None, FullStartup, QuickStartup, BossStartup, EmergencyReboot };

struct CockpitActivation {
    float body = 1.0f;
    float camera = 1.0f;
    float leftArm = 1.0f;
    float rightArm = 1.0f;
};

struct CockpitHudVisibility {
    bool base = true;
    bool armor = true;
    bool shield = true;
    bool leftArm = true;
    bool rightArm = true;
    bool dodge = true;
    bool targets = true;
};

class CockpitPresentation {
public:
    void Start(CockpitMode mode);
    void Update(float seconds);
    void SetEmergency(bool offline, float remaining);
    void Reset();
    CockpitMode GetMode() const { return mode_; }
    bool IsStartupActive() const;
    bool IsActive() const { return mode_ != CockpitMode::None; }
    float GetElapsed() const { return elapsed_; }
    float GetDuration() const;
    int GetVisibleLineCount() const;
    const char* GetTitle() const;
    const char* GetLine(int index) const;
    bool IsAllGreenVisible() const;
    float GetOverlayAlpha() const;
    CockpitActivation GetActivation() const;
    CockpitHudVisibility GetHudVisibility() const;
    bool ConsumeStartCue();

private:
    float GetCheckActivation(int index) const;
    CockpitMode mode_ = CockpitMode::None;
    float elapsed_ = 0.0f;
    float emergencyDuration_ = 0.0f;
    bool startCuePending_ = false;
};
