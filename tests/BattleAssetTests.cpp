#include "Data/BattleAssetCatalog.h"
#include "Data/SceneConfig.h"
#include "Data/SceneLoader.h"

#include <cstdio>
#include <filesystem>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::puts("FAIL: expected config and model directories");
        return 1;
    }
    const std::filesystem::path configDirectory = argv[1];
    const std::filesystem::path modelDirectory = argv[2];
    SceneConfig config;
    ModelScene scene;
    std::vector<MeshData> meshes;
    BattleAssetCatalog catalog;
    std::string error;
    if (!LoadSceneConfig(configDirectory / "battlefield-scene.cfg", config, error) ||
        !LoadSceneAssets(modelDirectory, config, meshes, scene, error) ||
        !LoadBattleAssetCatalog(configDirectory / "battle-assets.cfg",
            modelDirectory, meshes, catalog, error)) {
        std::printf("FAIL: %s\n", error.c_str());
        return 1;
    }
    const char* required[] = {"missile_unit", "bomber", "charge_unit", "missile",
        "flame", "floating_bomb", "boss_core", "boss_cone_left", "boss_cone_right",
        "boss_cone_lower", "boss_crystal_spike", "boss_crystal_cube", "ultimate_fist"};
    for (const char* key : required) {
        if (catalog.Find(key) == nullptr || catalog.Find(key)->meshIndex >= meshes.size()) {
            std::printf("FAIL: missing battle asset %s\n", key);
            return 1;
        }
    }
    if (scene.models.size() < 25 || meshes.size() < 22) {
        std::printf("FAIL: battlefield or model catalog is incomplete: %zu instances, %zu meshes\n",
            scene.models.size(), meshes.size());
        return 1;
    }
    std::printf("PASS: %zu battlefield instances and %zu battle meshes loaded\n",
        scene.models.size(), meshes.size());
    return 0;
}
