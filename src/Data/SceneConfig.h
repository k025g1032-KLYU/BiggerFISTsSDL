#pragma once

#include "Math/MathTypes.h"

#include <filesystem>
#include <string>
#include <vector>

struct SceneModelConfig {
    std::string id;
    std::filesystem::path relativeObjPath;
    Vector3 position{};
    float scale = 1.0f;
    float initialXDegrees = 0.0f;
    float initialYDegrees = 0.0f;
    float rotationXDegreesPerSecond = 0.0f;
    float rotationYDegreesPerSecond = 0.0f;
};

struct SceneConfig {
    Vector3 cameraPosition{};
    std::vector<SceneModelConfig> models;
};

bool LoadSceneConfig(const std::filesystem::path& configPath, SceneConfig& result, std::string& error);
