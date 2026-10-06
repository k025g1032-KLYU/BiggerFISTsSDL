#include "Data/SceneLoader.h"

#include "Data/ObjLoader.h"

#include <utility>

namespace {
std::string PathToUtf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
}
}

bool LoadSceneAssets(const std::filesystem::path& modelDirectory,
    const SceneConfig& config, std::vector<MeshData>& meshes,
    ModelScene& scene, std::string& error) {
    meshes.clear();
    scene = {};
    scene.models.clear();
    error.clear();
    if (config.models.empty()) {
        error = "Scene has no models";
        return false;
    }

    std::vector<MeshData> loadedMeshes;
    std::vector<std::filesystem::path> loadedPaths;
    std::vector<ModelBounds> loadedBounds;
    ModelScene loadedScene;
    loadedScene.models.clear();
    loadedScene.models.reserve(config.models.size());
    loadedScene.cameraPosition = config.cameraPosition;

    for (const SceneModelConfig& entry : config.models) {
        if (entry.id.empty()) {
            error = "Scene model has no id: " + PathToUtf8(entry.relativeObjPath);
            return false;
        }
        if (FindModelById(loadedScene, entry.id) != nullptr) {
            error = "Scene has duplicate model id: " + entry.id;
            return false;
        }
        std::size_t meshIndex = 0;
        while (meshIndex < loadedPaths.size() && loadedPaths[meshIndex] != entry.relativeObjPath) {
            ++meshIndex;
        }
        if (meshIndex == loadedPaths.size()) {
            const std::filesystem::path path = modelDirectory / entry.relativeObjPath;
            MeshData mesh;
            if (!LoadObjModel(path, mesh, error)) {
                return false;
            }
            ModelBounds bounds;
            if (!TryCalculateModelBounds(mesh, bounds)) {
                error = "Could not calculate bounds for model: " + PathToUtf8(path);
                return false;
            }
            loadedPaths.push_back(entry.relativeObjPath);
            loadedBounds.push_back(bounds);
            loadedMeshes.push_back(std::move(mesh));
        }

        ModelInstance instance;
        instance.id = entry.id;
        instance.meshIndex = meshIndex;
        instance.modelCenter = loadedBounds[meshIndex].center;
        instance.position = entry.position;
        instance.scale = entry.scale;
        instance.initialXDegrees = entry.initialXDegrees;
        instance.initialYDegrees = entry.initialYDegrees;
        instance.initialZDegrees = entry.initialZDegrees;
        instance.rotationXDegreesPerSecond = entry.rotationXDegreesPerSecond;
        instance.rotationYDegreesPerSecond = entry.rotationYDegreesPerSecond;
        loadedScene.models.push_back(instance);
    }

    meshes = std::move(loadedMeshes);
    scene = std::move(loadedScene);
    return true;
}
