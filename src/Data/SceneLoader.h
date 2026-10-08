#pragma once

#include "Data/MeshData.h"
#include "Data/SceneConfig.h"
#include "World/ModelScene.h"

#include <filesystem>
#include <string>
#include <vector>

enum class ModelOrigin { BoundsCenter, Authored };

bool LoadSceneAssets(const std::filesystem::path& modelDirectory,
    const SceneConfig& config, std::vector<MeshData>& meshes,
    ModelScene& scene, std::string& error,
    ModelOrigin origin = ModelOrigin::BoundsCenter);
