#pragma once

#include "GameInput.h"
#include "Input.h"
#include "InputBindings.h"

class InputActions {
public:
    explicit InputActions(const InputBindings& bindings = {});

    GameInput Evaluate(const Input& input) const;

private:
    InputBindings bindings_;
};
