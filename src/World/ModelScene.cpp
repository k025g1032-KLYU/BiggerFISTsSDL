#include "World/ModelScene.h"

#include "Data/MeshData.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace {
float CalculateRotationRadians(double elapsedSeconds, float initialDegrees, float degreesPerSecond) {
    const double degrees = std::fmod(initialDegrees + elapsedSeconds * degreesPerSecond, 360.0);
    return static_cast<float>(degrees * std::numbers::pi / 180.0);
}
}

bool TryCalculateModelBounds(const MeshData& mesh, ModelBounds& result) {
    result = {};
    if (mesh.vertices.empty()) {
        return false;
    }
    double minimum[3]{
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::infinity()
    };
    double maximum[3]{
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity(),
        -std::numeric_limits<double>::infinity()
    };
    for (const auto& vertex : mesh.vertices) {
        for (int axis = 0; axis < 3; ++axis) {
            const double value = vertex.position[axis];
            if (!std::isfinite(value)) {
                return false;
            }
            minimum[axis] = std::min(minimum[axis], value);
            maximum[axis] = std::max(maximum[axis], value);
        }
    }

    double center[3]{};
    double largestHalfExtent = 0.0;
    for (int axis = 0; axis < 3; ++axis) {
        center[axis] = minimum[axis] + (maximum[axis] - minimum[axis]) * 0.5;
        largestHalfExtent = std::max(largestHalfExtent, (maximum[axis] - minimum[axis]) * 0.5);
        if (std::abs(center[axis]) > std::numeric_limits<float>::max()) {
            return false;
        }
    }
    const double distance = std::max(3.0, largestHalfExtent * 3.0);
    if (!std::isfinite(distance) || distance > std::numeric_limits<float>::max() / 2.0) {
        return false;
    }

    result.center = {
        static_cast<float>(center[0]),
        static_cast<float>(center[1]),
        static_cast<float>(center[2])
    };
    result.largestHalfExtent = static_cast<float>(largestHalfExtent);
    return true;
}

bool TryConfigureModelPreview(const MeshData& mesh, ModelScene& scene) {
    if (scene.models.empty()) {
        return false;
    }
    ModelBounds bounds;
    if (!TryCalculateModelBounds(mesh, bounds)) {
        return false;
    }
    const float distance = std::max(3.0f, bounds.largestHalfExtent * 3.0f);
    scene.models.front().modelCenter = bounds.center;
    scene.cameraPosition = { 0.0f, 0.0f, -distance };
    scene.cameraTarget = { 0.0f, 0.0f, 0.0f };
    scene.farPlane = std::max(100.0f, distance * 2.0f);
    return true;
}

bool TryBuildModelTransform(const ModelScene& scene, const ModelInstance& model,
    std::uint32_t outputWidth,
    std::uint32_t outputHeight, Matrix4x4& result) {
    result = {};
    if (outputWidth == 0 || outputHeight == 0 || !std::isfinite(scene.elapsedSeconds) ||
        !std::isfinite(model.scale) || !std::isfinite(model.modelCenter.x) ||
        !std::isfinite(model.modelCenter.y) || !std::isfinite(model.modelCenter.z) ||
        !std::isfinite(model.position.x) ||
        !std::isfinite(model.position.y) || !std::isfinite(model.position.z) ||
        !std::isfinite(model.initialXDegrees) || !std::isfinite(model.initialYDegrees) ||
        !std::isfinite(model.rotationXDegreesPerSecond) || !std::isfinite(model.rotationYDegreesPerSecond)) {
        return false;
    }

    const Matrix4x4 scale = MakeScaleMatrix(model.scale, model.scale, model.scale);
    const Matrix4x4 center = MakeTranslationMatrix(
        -model.modelCenter.x, -model.modelCenter.y, -model.modelCenter.z);
    const Matrix4x4 rotationX = MakeRotationXMatrix(CalculateRotationRadians(
        scene.elapsedSeconds, model.initialXDegrees, model.rotationXDegreesPerSecond));
    const Matrix4x4 rotationY = MakeRotationYMatrix(CalculateRotationRadians(
        scene.elapsedSeconds, model.initialYDegrees, model.rotationYDegreesPerSecond));
    const Matrix4x4 translation = MakeTranslationMatrix(
        model.position.x, model.position.y, model.position.z);
    const Matrix4x4 world = MultiplyMatrices(
        MultiplyMatrices(MultiplyMatrices(MultiplyMatrices(center, scale), rotationX), rotationY), translation);

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

bool TryBuildModelTransform(const ModelScene& scene, std::uint32_t outputWidth,
    std::uint32_t outputHeight, Matrix4x4& result) {
    if (scene.models.empty()) {
        result = {};
        return false;
    }
    return TryBuildModelTransform(scene, scene.models.front(), outputWidth, outputHeight, result);
}
