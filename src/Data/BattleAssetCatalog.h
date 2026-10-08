#pragma once

#include "Data/MeshData.h"
#include "World/ModelScene.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

class BattleAssetCatalog {
public:
    const ModelInstance* Find(std::string_view key) const;
    void Add(std::string key, ModelInstance model);
    bool SetPose(std::string_view key, Vector3 scale, Vector3 rotation,
        Vector3 offset);
    void Clear();

private:
    std::unordered_map<std::string, ModelInstance> models_;
};

bool LoadBattleAssetCatalog(const std::filesystem::path& configPath,
    const std::filesystem::path& modelDirectory, std::vector<MeshData>& meshes,
    BattleAssetCatalog& catalog, std::string& error);
