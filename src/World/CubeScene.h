#pragma once

#include "Math/MathTypes.h"

#include <cstdint>

struct CubeScene {
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    double elapsedSeconds = 0.0;
    bool warmBackground = false;
    float scale = 1.0f;
    float initialXDegrees = 20.0f;
    float initialYDegrees = 30.0f;
    float rotationXDegreesPerSecond = 25.0f;
    float rotationYDegreesPerSecond = 40.0f;
    Vector3 cameraPosition{ 0.0f, 0.0f, -3.0f };
    Vector3 cameraTarget{ 0.0f, 0.0f, 0.0f };
    Vector3 cameraUp{ 0.0f, 1.0f, 0.0f };
    float verticalFovDegrees = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};

bool TryBuildCubeTransform(const CubeScene& scene, std::uint32_t outputWidth,
    std::uint32_t outputHeight, Matrix4x4& result);
