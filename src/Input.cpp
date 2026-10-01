#include "Input.h"

void Input::BeginFrame() {
    pressed_.fill(false);
    released_.fill(false);
}

void Input::ProcessEvent(const SDL_Event& event) {
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

    return keyStates_ != nullptr &&
           index > SDL_SCANCODE_UNKNOWN &&
           index < keyCount_ &&
           keyStates_[index];
}

bool Input::IsKeyPressed(SDL_Scancode key) const {
    const int index = static_cast<int>(key);

    return index > SDL_SCANCODE_UNKNOWN &&
           index < SDL_SCANCODE_COUNT &&
           pressed_[index];
}

bool Input::IsKeyReleased(SDL_Scancode key) const {
    const int index = static_cast<int>(key);

    return index > SDL_SCANCODE_UNKNOWN &&
           index < SDL_SCANCODE_COUNT &&
           released_[index];
}
