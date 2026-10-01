#include "FrameTimer.h"

FrameTimer::FrameTimer(int targetFps) {
    if (targetFps <= 0) {
        SDL_Log("Invalid targetFps = %d; using 60", targetFps);
        targetFps = 60;
    }

    targetFrameTimeNS_ =
        1000000000ULL / static_cast<Uint64>(targetFps);

    previousTimeNS_ = SDL_GetTicksNS();
}

void FrameTimer::BeginFrame() {
    frameStartTimeNS_ = SDL_GetTicksNS();

    deltaTime_ =
        (frameStartTimeNS_ - previousTimeNS_) / 1000000000.0;

    previousTimeNS_ = frameStartTimeNS_;

    fpsElapsed_ += deltaTime_;
    ++frameCount_;

    if (fpsElapsed_ >= 1.0) {
        const double fps = frameCount_ / fpsElapsed_;
        SDL_Log("FPS = %.1f", fps);

        fpsElapsed_ = 0.0;
        frameCount_ = 0;
    }
}

double FrameTimer::GetDeltaTime() const {
    return deltaTime_;
}

void FrameTimer::EndFrame() {
    const Uint64 elapsedTimeNS =
        SDL_GetTicksNS() - frameStartTimeNS_;

    if (elapsedTimeNS < targetFrameTimeNS_) {
        SDL_DelayPrecise(targetFrameTimeNS_ - elapsedTimeNS);
    }
}
