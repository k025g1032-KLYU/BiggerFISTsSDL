#include "Game/Game.h"

#include <cmath>
#include <cstdio>

namespace {
bool Expect(bool condition, const char* description) {
    if (!condition) {
        std::printf("FAIL: %s\n", description);
    }
    return condition;
}

bool NearlyEqual(float a, float b) {
    return std::abs(a - b) < 0.0001f;
}
}

int main() {
    Game game(GameConfig{});
    GameInput input{};
    bool success = true;

    input.moveX = 1.0f;
    game.Update(0.5, input);
    success &= Expect(NearlyEqual(game.GetScene().position.x, 0.12f),
        "Movement uses world units and caps a long frame");
    success &= Expect(game.GetScene().elapsedSeconds == 0.5,
        "Animation uses real elapsed time rather than capped movement time");

    input.moveX = 0.0f;
    input.moveY = -1.0f;
    game.Update(0.05, input);
    success &= Expect(NearlyEqual(game.GetScene().position.y, 0.12f),
        "Move-up input increases world Y");

    input.moveY = 0.0f;
    input.changeBackground.pressed = true;
    game.Update(0.0, input);
    success &= Expect(game.GetScene().warmBackground,
        "Background changes on a press");
    input.changeBackground.pressed = false;
    game.Update(0.0, input);
    success &= Expect(game.GetScene().warmBackground,
        "A held or released key does not toggle again");
    input.changeBackground.pressed = true;
    game.Update(0.0, input);
    success &= Expect(!game.GetScene().warmBackground,
        "A second press restores the original background");

    Matrix4x4 transform{};
    success &= Expect(TryBuildCubeTransform(game.GetScene(), 1280, 720, transform),
        "Game state produces a valid 3D transform");
    success &= Expect(!TryBuildCubeTransform(game.GetScene(), 0, 720, transform),
        "An unavailable GPU output size is rejected");

    if (success) {
        std::puts("PASS: 3D game input, scene state and transform");
    }
    return success ? 0 : 1;
}
