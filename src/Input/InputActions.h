#pragma once

#include "Input/GameInput.h"
#include "Input/Input.h"
#include "Input/InputBindings.h"

bool IsGamepadTriggerPressed(Sint16 value);

class InputActions {
public:
    explicit InputActions(const InputBindings& bindings = {});

    GameInput Evaluate(const Input& input, SDL_Gamepad* gamepad = nullptr,
        double deltaSeconds = 0.0);

private:
    InputBindings bindings_;
    std::array<bool, SDL_GAMEPAD_BUTTON_COUNT> previousButtons_{};
    bool previousLeftTrigger_ = false;
    bool previousRightTrigger_ = false;
    SDL_Gamepad* previousGamepad_ = nullptr;
};
