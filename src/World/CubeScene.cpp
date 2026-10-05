#include "World/CubeScene.h"

#include <cmath>
#include <numbers>

namespace {
float CalculateRotationRadians(double elapsedSeconds, float initialDegrees, float degreesPerSecond) {
    const double degrees = std::fmod(initialDegrees + elapsedSeconds * degreesPerSecond, 360.0);
    return static_cast<float>(degrees * std::numbers::pi / 180.0);
}
}

bool TryBuildCubeTransform(const CubeScene& scene, std::uint32_t outputWidth,
    std::uint32_t outputHeight, Matrix4x4& result) {
    result = {};
    if (outputWidth == 0 || outputHeight == 0 || !std::isfinite(scene.elapsedSeconds) ||
        !std::isfinite(scene.scale) || !std::isfinite(scene.position.x) ||
        !std::isfinite(scene.position.y) || !std::isfinite(scene.position.z) ||
        !std::isfinite(scene.initialXDegrees) || !std::isfinite(scene.initialYDegrees) ||
        !std::isfinite(scene.rotationXDegreesPerSecond) || !std::isfinite(scene.rotationYDegreesPerSecond)) {
        return false;
    }

    const Matrix4x4 scale = MakeScaleMatrix(scene.scale, scene.scale, scene.scale);
    const Matrix4x4 rotationX = MakeRotationXMatrix(CalculateRotationRadians(
        scene.elapsedSeconds, scene.initialXDegrees, scene.rotationXDegreesPerSecond));
    const Matrix4x4 rotationY = MakeRotationYMatrix(CalculateRotationRadians(
        scene.elapsedSeconds, scene.initialYDegrees, scene.rotationYDegreesPerSecond));
    const Matrix4x4 translation = MakeTranslationMatrix(scene.position.x, scene.position.y, scene.position.z);
    const Matrix4x4 world = MultiplyMatrices(
        MultiplyMatrices(MultiplyMatrices(scale, rotationX), rotationY), translation);

    Matrix4x4 view{};
    if (!TryMakeLookAtLHMatrix(scene.cameraPosition, scene.cameraTarget, scene.cameraUp, view)) {
        return false;
    }
    Matrix4x4 projection{};
    const float aspectRatio = static_cast<float>(outputWidth) / static_cast<float>(outputHeight);
    const float fovRadians = scene.verticalFovDegrees * std::numbers::pi_v<float> / 180.0f;
    if (!TryMakePerspectiveLHMatrix(fovRadians, aspectRatio, scene.nearPlane, scene.farPlane, projection)) {
        return false;
    }
    result = MultiplyMatrices(MultiplyMatrices(world, view), projection);
    return true;
}
