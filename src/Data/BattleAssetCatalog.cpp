#include "Data/BattleAssetCatalog.h"

#include "Data/ObjLoader.h"

#include <fstream>
#include <cmath>
#include <iterator>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace {
std::string ToUtf8(const std::filesystem::path& path) {
    const auto value = path.u8string();
    return {reinterpret_cast<const char*>(value.data()), value.size()};
}
}

const ModelInstance* BattleAssetCatalog::Find(std::string_view key) const {
    const auto found = models_.find(std::string(key));
    return found == models_.end() ? nullptr : &found->second;
}

void BattleAssetCatalog::Add(std::string key, ModelInstance model) {
    models_.emplace(std::move(key), std::move(model));
}

bool BattleAssetCatalog::SetPose(std::string_view key, Vector3 scale,
    Vector3 rotation, Vector3 offset) {
    const auto found = models_.find(std::string(key));
    if (found == models_.end()) return false;
    found->second.scaleAxes = scale;
    found->second.initialXDegrees = rotation.x;
    found->second.initialYDegrees = rotation.y;
    found->second.initialZDegrees = rotation.z;
    found->second.position = offset;
    return true;
}

void BattleAssetCatalog::Clear() {
    models_.clear();
}

bool LoadBattleAssetCatalog(const std::filesystem::path& configPath,
    const std::filesystem::path& modelDirectory, std::vector<MeshData>& meshes,
    BattleAssetCatalog& catalog, std::string& error) {
    error.clear();
    std::ifstream input(configPath);
    if (!input) {
        error = "Could not open battle asset catalog '" + ToUtf8(configPath) + "'";
        return false;
    }
    BattleAssetCatalog loadedCatalog;
    std::vector<MeshData> loadedMeshes;
    std::unordered_set<std::string> posedAssets;
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        std::istringstream fields(line);
        std::string command;
        std::string key;
        std::string relativeName;
        std::string extra;
        if (!(fields >> command) || command.starts_with('#')) {
            continue;
        }
        if (command == "pose") {
            Vector3 scale{}, rotation{}, offset{};
            if (!(fields >> key >> scale.x >> scale.y >> scale.z >>
                    rotation.x >> rotation.y >> rotation.z >>
                    offset.x >> offset.y >> offset.z) ||
                (fields >> extra) || posedAssets.contains(key) ||
                !std::isfinite(scale.x) || !std::isfinite(scale.y) ||
                !std::isfinite(scale.z) || scale.x <= 0.0f ||
                scale.y <= 0.0f || scale.z <= 0.0f ||
                !std::isfinite(rotation.x) || !std::isfinite(rotation.y) ||
                !std::isfinite(rotation.z) || !std::isfinite(offset.x) ||
                !std::isfinite(offset.y) || !std::isfinite(offset.z) ||
                !loadedCatalog.SetPose(key, scale, rotation, offset)) {
                error = "Invalid model pose in '" + ToUtf8(configPath) +
                    "' line " + std::to_string(lineNumber);
                return false;
            }
            posedAssets.insert(key);
            continue;
        }
        if (command != "asset" || !(fields >> key >> relativeName) ||
            (fields >> extra) || key.empty() || loadedCatalog.Find(key) != nullptr) {
            error = "Invalid battle asset in '" + ToUtf8(configPath) +
                "' line " + std::to_string(lineNumber);
            return false;
        }
        const std::filesystem::path relativePath(relativeName);
        if (relativePath.is_absolute() || relativePath.extension() != ".obj" ||
            relativePath.lexically_normal() != relativePath ||
            relativePath.string().starts_with("..")) {
            error = "Invalid battle asset path in '" + ToUtf8(configPath) +
                "' line " + std::to_string(lineNumber) + ": " + relativeName;
            return false;
        }
        MeshData mesh;
        if (!LoadObjModel(modelDirectory / relativePath, mesh, error)) {
            error = "Battle asset '" + key + "' in '" + ToUtf8(configPath) +
                "' line " + std::to_string(lineNumber) + ": " + error;
            return false;
        }
        ModelBounds bounds;
        if (!TryCalculateModelBounds(mesh, bounds)) {
            error = "Could not calculate bounds for battle asset '" + key + "'";
            return false;
        }
        ModelInstance model;
        model.id = key;
        model.meshIndex = meshes.size() + loadedMeshes.size();
        model.modelCenter = {};
        model.initialXDegrees = 0.0f;
        model.initialYDegrees = 0.0f;
        model.rotationXDegreesPerSecond = 0.0f;
        model.rotationYDegreesPerSecond = 0.0f;
        loadedCatalog.Add(key, model);
        loadedMeshes.push_back(std::move(mesh));
    }
    if (input.bad() || loadedMeshes.empty()) {
        error = "Could not read battle assets from '" + ToUtf8(configPath) + "'";
        return false;
    }
    if (posedAssets.size() != loadedMeshes.size()) {
        error = "Missing model pose in '" + ToUtf8(configPath) + "'";
        return false;
    }
    meshes.insert(meshes.end(), std::make_move_iterator(loadedMeshes.begin()),
        std::make_move_iterator(loadedMeshes.end()));
    catalog = std::move(loadedCatalog);
    return true;
}
