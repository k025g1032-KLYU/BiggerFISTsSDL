#pragma once

#include "Data/GameConfig.h"
#include "Input/GameInput.h"
#include "World/ModelScene.h"

class Game {
public:
    explicit Game(const GameConfig& config, ModelScene initialScene = {});

    void Update(double deltaTime, const GameInput& input);
    const ModelScene& GetScene() const;

private:
    float moveSpeed_;
    float maxMovementDeltaTime_;

    ModelScene scene_{};
};
