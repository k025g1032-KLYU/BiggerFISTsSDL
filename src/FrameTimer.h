#pragma once

#include <SDL3/SDL.h>

class FrameTimer {
public:
    explicit FrameTimer(int targetFps);

    void BeginFrame();
    double GetDeltaTime() const;
    void EndFrame();

private:
    Uint64 targetFrameTimeNS_ = 0;
    Uint64 previousTimeNS_ = 0;
    Uint64 frameStartTimeNS_ = 0;

    double deltaTime_ = 0.0;
    double fpsElapsed_ = 0.0;
    int frameCount_ = 0;
};
