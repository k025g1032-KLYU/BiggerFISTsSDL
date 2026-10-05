#include "Game/Game.h"

#include <algorithm>
#include <cmath>

Game::Game(const GameConfig& config)
    : moveSpeed_(config.moveSpeed),
      maxMovementDeltaTime_(config.maxMovementDeltaTime) {
}

void Game::Update(double deltaTime, const GameInput& input) {
    if (!std::isfinite(deltaTime) || deltaTime < 0.0) {
        return;
    }

    scene_.elapsedSeconds += deltaTime;

    if (input.changeBackground.pressed) {
        scene_.warmBackground = !scene_.warmBackground;
    }

    const float length = std::sqrt(input.moveX * input.moveX + input.moveY * input.moveY);
    if (length <= 0.0f) {
        return;
    }

    const float movementDeltaTime = std::min(static_cast<float>(deltaTime), maxMovementDeltaTime_);
    const float distance = moveSpeed_ * movementDeltaTime;
    scene_.position.x += (input.moveX / length) * distance;
    scene_.position.y -= (input.moveY / length) * distance;

    // Keep the sample cube visible while the camera remains fixed.
    scene_.position.x = std::clamp(scene_.position.x, -1.2f, 1.2f);
    scene_.position.y = std::clamp(scene_.position.y, -0.8f, 0.8f);
}

const CubeScene& Game::GetScene() const {
    return scene_;
}
