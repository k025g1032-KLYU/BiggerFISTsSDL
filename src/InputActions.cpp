#include "InputActions.h"

InputActions::InputActions(const InputBindings& bindings)
    : bindings_(bindings) {
}

GameInput InputActions::Evaluate(const Input& input) const {
    GameInput result{};

    if (input.IsKeyDown(bindings_.moveLeft)) result.moveX -= 1.0f;
    if (input.IsKeyDown(bindings_.moveRight)) result.moveX += 1.0f;
    if (input.IsKeyDown(bindings_.moveUp)) result.moveY -= 1.0f;
    if (input.IsKeyDown(bindings_.moveDown)) result.moveY += 1.0f;

    result.changeBackground = {
        input.IsKeyDown(bindings_.changeBackground),
        input.IsKeyPressed(bindings_.changeBackground),
        input.IsKeyReleased(bindings_.changeBackground)
    };

    result.quit = {
        input.IsKeyDown(bindings_.quit),
        input.IsKeyPressed(bindings_.quit),
        input.IsKeyReleased(bindings_.quit)
    };

    return result;
}
