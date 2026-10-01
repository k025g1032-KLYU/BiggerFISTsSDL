#pragma once

struct GameConfig {
    int windowWidth = 1280;
    int windowHeight = 720;
    float moveSpeed = 240.0f;
    float maxMovementDeltaTime = 0.05f;
    int targetFps = 60;
};

GameConfig LoadGameConfig();
