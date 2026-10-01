#pragma once

#include "GameConfig.h"
#include "GameInput.h"

#include <SDL3/SDL.h>

class Game {
public:
    explicit Game(const GameConfig& config);

    void SetBackgroundColor(SDL_Color color);

    void Update(
        double deltaTime,
        const GameInput& input,
        int outputWidth,
        int outputHeight
    );

    bool Draw(SDL_Renderer* renderer) const;

private:
    float moveSpeed_;
    float maxMovementDeltaTime_;

    SDL_Color backgroundColor_{ 32, 48, 68, 255 };
    SDL_FRect square_{ 600.0f, 320.0f, 80.0f, 80.0f };
};
