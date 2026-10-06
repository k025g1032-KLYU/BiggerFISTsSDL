#pragma once

#include <SDL3/SDL.h>
#include <array>

class Input {
public:
    void BeginFrame();
    void ProcessEvent(const SDL_Event& event);
    void Update();
    bool IsKeyDown(SDL_Scancode key) const;
    bool IsKeyPressed(SDL_Scancode key) const;
    bool IsKeyReleased(SDL_Scancode key) const;
    bool IsMouseButtonDown(Uint8 button) const;
    float GetMouseDeltaX() const { return mouseDeltaX_; }
    float GetMouseDeltaY() const { return mouseDeltaY_; }

private:
    const bool* keyStates_ = nullptr;
    int keyCount_ = 0;

    std::array<bool, SDL_SCANCODE_COUNT> pressed_{};
    std::array<bool, SDL_SCANCODE_COUNT> released_{};
    std::array<bool, 8> mouseButtonsDown_{};
    bool hasFocus_ = true;
    float mouseDeltaX_ = 0.0f;
    float mouseDeltaY_ = 0.0f;
};
