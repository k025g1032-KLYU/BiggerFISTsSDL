#include "Game/Game.h"

#include <algorithm>
#include <cmath>

Game::Game(const GameConfig& config, ModelScene initialScene)
    : moveSpeed_(config.moveSpeed),
      maxMovementDeltaTime_(config.maxMovementDeltaTime),
      scene_(initialScene) {
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
    if (length <= 0.0f || scene_.models.empty()) {
        return;
    }

    ModelInstance& model = scene_.models.front();
    const float movementDeltaTime = std::min(static_cast<float>(deltaTime), maxMovementDeltaTime_);
    const float distance = moveSpeed_ * movementDeltaTime;
    model.position.x += (input.moveX / length) * distance;
    model.position.y -= (input.moveY / length) * distance;

    // Keep the selected model near the fixed camera's view.
    model.position.x = std::clamp(model.position.x, -1.2f, 1.2f);
    model.position.y = std::clamp(model.position.y, -0.4f, 0.4f);
}

const ModelScene& Game::GetScene() const {
    return scene_;
}
