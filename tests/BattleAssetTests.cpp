#include "Data/BattleAssetCatalog.h"
#include "Data/SceneConfig.h"
#include "Data/SceneLoader.h"

#include <cstdio>
#include <filesystem>
#include <string>
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
    if (catalog.Find("missile_unit")->scaleAxes.x != 0.9f ||
        catalog.Find("bomber")->scaleAxes.x != 1.1f ||
        catalog.Find("boss_crystal_spike")->scaleAxes.x != 1.3f ||
        catalog.Find("boss_crystal_cube")->scaleAxes.x != 1.15f ||
        catalog.Find("boss_core")->modelCenter.x != 0.0f ||
        catalog.Find("boss_core")->rotationXDegreesPerSecond != 0.0f ||
        catalog.Find("boss_core")->rotationYDegreesPerSecond != 0.0f ||
        catalog.Find("missile_unit")->rotationYDegreesPerSecond != 0.0f) {
        std::puts("FAIL: original asset poses or authored origins were not loaded");
        return 1;
    }
    for (const char* key : {"body", "cloud", "tutorial_banner", "helmet",
            "shield_one", "shield_two", "shield_three", "explosion_flash",
            "explosion_fire", "explosion_smoke"}) {
        if (catalog.Find(key) == nullptr) {
            std::printf("FAIL: missing original model %s\n", key);
            return 1;
        }
    }
    const ModelInstance* frontWall = FindModelById(scene, "front_wall");
    if (!frontWall || !frontWall->transparent ||
        frontWall->position.z != -16.0f ||
        scene.models.size() < 26 || meshes.size() < 23) {
        std::printf("FAIL: battlefield or model catalog is incomplete: %zu instances, %zu meshes\n",
            scene.models.size(), meshes.size());
        return 1;
    }
    constexpr float sideWallZ[] = {-10.0f, 13.0f, 36.0f};
    for (int index = 0; index < 3; ++index) {
        const ModelInstance* leftWall = FindModelById(scene, "left_wall_" + std::to_string(index));
        const ModelInstance* rightWall = FindModelById(scene, "right_wall_" + std::to_string(index));
        if (!leftWall || !rightWall ||
            leftWall->position.x != -24.0f || rightWall->position.x != 24.0f ||
            leftWall->position.z != sideWallZ[index] || rightWall->position.z != sideWallZ[index]) {
            std::printf("FAIL: side wall tile %d does not match the original centered layout\n", index);
            return 1;
        }
    }
    std::printf("PASS: %zu battlefield instances and %zu battle meshes loaded\n",
        scene.models.size(), meshes.size());
    return 0;
}
