#include "GameConfig.h"

#include <SDL3/SDL.h>
#include <cmath>
#include <sstream>
#include <string>

GameConfig LoadGameConfig() {
    GameConfig config{};

    const char* basePath = SDL_GetBasePath();
    if (basePath == nullptr) {
        SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
        return config;
    }

    const std::string path = std::string(basePath) + "gameplay.cfg";
    size_t fileSize = 0;
    void* fileData = SDL_LoadFile(path.c_str(), &fileSize);
    if (fileData == nullptr) {
        SDL_Log("Could not load %s: %s", path.c_str(), SDL_GetError());
        return config;
    }

    std::string text(static_cast<const char*>(fileData), fileSize);
    SDL_free(fileData);
    if (text.starts_with("\xEF\xBB\xBF")) {
        text.erase(0, 3);
    }

    std::istringstream input(text);
    std::string line;
    int lineNumber = 0;

    while (std::getline(input, line)) {
        ++lineNumber;

        std::istringstream lineInput(line);
        std::string key;

        if (!(lineInput >> key)) {
            continue;
        }

        float value = 0.0f;
        std::string extra;

        if (!(lineInput >> value) ||
            (lineInput >> extra) ||
            !std::isfinite(value)) {
            SDL_Log("Invalid format on line %d", lineNumber);
            continue;
        }

        if (key == "moveSpeed" &&
            value > 0.0f && value <= 2000.0f) {
            config.moveSpeed = value;
        }
        else if (key == "maxMovementDeltaTime" &&
            value > 0.0f && value <= 0.1f) {
            config.maxMovementDeltaTime = value;
        }
        else if (key == "targetFps" &&
            value >= 1.0f && value <= 240.0f &&
            std::floor(value) == value) {
            config.targetFps = static_cast<int>(value);
        }
        else {
            SDL_Log(
                "Unknown key or value out of range on line %d: %s",
                lineNumber,
                key.c_str()
            );
        }
    }

    SDL_Log(
        "Config: moveSpeed = %.1f, maxMovementDeltaTime = %.3f, targetFps = %d",
        config.moveSpeed,
        config.maxMovementDeltaTime,
        config.targetFps
    );

    return config;
}
