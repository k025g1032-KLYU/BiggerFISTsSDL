#pragma once

#include "Math/MathTypes.h"

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

struct MeshData;

struct ModelInstance {
    std::string id;
    std::size_t meshIndex = 0;
    Vector3 position{ 0.0f, 0.0f, 0.0f };
    Vector3 modelCenter{ 0.0f, 0.0f, 0.0f };
    float scale = 1.0f;
    float initialXDegrees = 20.0f;
    float initialYDegrees = 30.0f;
    float initialZDegrees = 0.0f;
    float rotationXDegreesPerSecond = 25.0f;
    float rotationYDegreesPerSecond = 40.0f;
};

struct ModelBounds {
    Vector3 center{ 0.0f, 0.0f, 0.0f };
    float largestHalfExtent = 0.0f;
};

struct ModelScene {
    std::vector<ModelInstance> models{ ModelInstance{} };
    double elapsedSeconds = 0.0;
    bool warmBackground = false;
    Vector3 cameraPosition{ 0.0f, 0.0f, -3.0f };
    Vector3 cameraTarget{ 0.0f, 0.0f, 0.0f };
    Vector3 cameraUp{ 0.0f, 1.0f, 0.0f };
    float verticalFovDegrees = 60.0f;
    float nearPlane = 0.1f;
    float farPlane = 100.0f;
};

bool TryCalculateModelBounds(const MeshData& mesh, ModelBounds& result);
bool TryConfigureModelPreview(const MeshData& mesh, ModelScene& scene);
ModelInstance* FindModelById(ModelScene& scene, std::string_view id);
const ModelInstance* FindModelById(const ModelScene& scene, std::string_view id);
bool TryBuildModelTransform(const ModelScene& scene, const ModelInstance& model,
    std::uint32_t outputWidth, std::uint32_t outputHeight, Matrix4x4& result);
bool TryBuildModelTransform(const ModelScene& scene, std::uint32_t outputWidth,
    std::uint32_t outputHeight, Matrix4x4& result);
