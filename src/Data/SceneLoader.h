#pragma once

#include "Data/MeshData.h"
#include "Data/SceneConfig.h"
#include "World/ModelScene.h"

#include <filesystem>
#include <string>
#include <vector>

bool LoadSceneAssets(const std::filesystem::path& modelDirectory,
    const SceneConfig& config, std::vector<MeshData>& meshes,
    ModelScene& scene, std::string& error);
