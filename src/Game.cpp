#include "Game.h"

#include <algorithm>
#include <cmath>

Game::Game(const GameConfig& config)
    : moveSpeed_(config.moveSpeed),
      maxMovementDeltaTime_(config.maxMovementDeltaTime) {
}

void Game::SetBackgroundColor(SDL_Color color) {
    backgroundColor_ = color;
}

void Game::Update(
    double deltaTime,
    const GameInput& input,
    int outputWidth,
    int outputHeight
) {
    if (input.changeBackground.pressed) {
        SetBackgroundColor({ 180, 80, 40, 255 });
        SDL_Log("ChangeBackground pressed");
    }

    if (input.changeBackground.released) {
        SDL_Log("ChangeBackground released");
    }

    const float length = std::sqrt(
        input.moveX * input.moveX + input.moveY * input.moveY
    );

    if (length > 0.0f) {
        const float movementDeltaTime = std::min(
            static_cast<float>(deltaTime),
            maxMovementDeltaTime_
        );

        const float distance = moveSpeed_ * movementDeltaTime;

        square_.x += (input.moveX / length) * distance;
        square_.y += (input.moveY / length) * distance;
    }

    const float maxX = std::max(
        0.0f,
        static_cast<float>(outputWidth) - square_.w
    );

    const float maxY = std::max(
        0.0f,
        static_cast<float>(outputHeight) - square_.h
    );

    square_.x = std::clamp(square_.x, 0.0f, maxX);
    square_.y = std::clamp(square_.y, 0.0f, maxY);
}

bool Game::Draw(SDL_Renderer* renderer) const {
    return SDL_SetRenderDrawColor(
               renderer,
               backgroundColor_.r,
               backgroundColor_.g,
               backgroundColor_.b,
               backgroundColor_.a
           ) &&
           SDL_RenderClear(renderer) &&
           SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255) &&
           SDL_RenderFillRect(renderer, &square_);
}
