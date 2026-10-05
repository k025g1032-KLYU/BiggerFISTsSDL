#pragma once

#include "Data/GameConfig.h"
#include "Input/GameInput.h"
#include "World/CubeScene.h"

class Game {
public:
    explicit Game(const GameConfig& config);

    void Update(double deltaTime, const GameInput& input);
    const CubeScene& GetScene() const;

private:
    float moveSpeed_;
    float maxMovementDeltaTime_;

    CubeScene scene_{};
};
