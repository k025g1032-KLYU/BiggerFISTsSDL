#include "Input/Input.h"

void Input::BeginFrame() {
    pressed_.fill(false);
    released_.fill(false);
    mouseDeltaX_ = 0.0f;
    mouseDeltaY_ = 0.0f;
}

void Input::ProcessEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_MOUSE_MOTION) {
        if (hasFocus_) {
            mouseDeltaX_ += event.motion.xrel;
            mouseDeltaY_ += event.motion.yrel;
        }
        return;
    }
    if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
        mouseDeltaX_ = 0.0f;
        mouseDeltaY_ = 0.0f;
        pressed_.fill(false);
        released_.fill(false);
        mouseButtonsDown_.fill(false);
        hasFocus_ = false;
        return;
    }
    if (event.type == SDL_EVENT_WINDOW_FOCUS_GAINED) {
        hasFocus_ = true;
        return;
    }
    if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
        event.type == SDL_EVENT_MOUSE_BUTTON_UP) {
        const Uint8 button = event.button.button;
        if (hasFocus_ && button < mouseButtonsDown_.size()) {
            mouseButtonsDown_[button] = event.type == SDL_EVENT_MOUSE_BUTTON_DOWN;
        }
        return;
    }
    if (event.type != SDL_EVENT_KEY_DOWN &&
        event.type != SDL_EVENT_KEY_UP) {
        return;
    }

    const int index = static_cast<int>(event.key.scancode);

    if (index <= SDL_SCANCODE_UNKNOWN || index >= SDL_SCANCODE_COUNT) {
        return;
    }

    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        pressed_[index] = true;
    }
    else if (event.type == SDL_EVENT_KEY_UP) {
        released_[index] = true;
    }
}

void Input::Update() {
    keyStates_ = SDL_GetKeyboardState(&keyCount_);
}

bool Input::IsKeyDown(SDL_Scancode key) const {
    const int index = static_cast<int>(key);

    return hasFocus_ && keyStates_ != nullptr &&
           index > SDL_SCANCODE_UNKNOWN &&
           index < keyCount_ &&
           keyStates_[index];
}

bool Input::IsMouseButtonDown(Uint8 button) const {
    return hasFocus_ && button < mouseButtonsDown_.size() && mouseButtonsDown_[button];
}

bool Input::IsKeyPressed(SDL_Scancode key) const {
    const int index = static_cast<int>(key);

    return hasFocus_ && index > SDL_SCANCODE_UNKNOWN &&
           index < SDL_SCANCODE_COUNT &&
           pressed_[index];
}

bool Input::IsKeyReleased(SDL_Scancode key) const {
    const int index = static_cast<int>(key);

    return hasFocus_ && index > SDL_SCANCODE_UNKNOWN &&
           index < SDL_SCANCODE_COUNT &&
           released_[index];
}
