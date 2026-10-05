#pragma once

#include "Input/GameInput.h"
#include "Input/Input.h"
#include "Input/InputBindings.h"

class InputActions {
public:
    explicit InputActions(const InputBindings& bindings = {});

    GameInput Evaluate(const Input& input) const;

private:
    InputBindings bindings_;
};
