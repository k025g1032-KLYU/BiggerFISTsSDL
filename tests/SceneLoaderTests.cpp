#include "Data/SceneLoader.h"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

namespace {
bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}
}

int main() {
    const std::filesystem::path modelDirectory = SCENE_TEST_MODEL_DIRECTORY;
    SceneConfig config;
    config.cameraPosition = { 0.0f, 0.0f, -6.0f };
    config.models.resize(3);
    config.models[0].id = "target_1";
    config.models[0].relativeObjPath = "Target/Target.obj";
    config.models[0].position.x = -1.6f;
    config.models[1].id = "left_fist";
    config.models[1].relativeObjPath = "LfistTEST/LfistTEST.obj";
    config.models[1].position.x = 1.6f;
    config.models[1].initialZDegrees = 90.0f;
    config.models[2].id = "target_2";
    config.models[2].relativeObjPath = "Target/Target.obj";
    config.models[2].position.y = -1.4f;
    config.models[2].scale = 0.5f;

    std::vector<MeshData> meshes;
    ModelScene scene;
    std::string error;
    bool passed = true;
    passed &= Expect(LoadSceneAssets(modelDirectory, config, meshes, scene, error),
        "load three instances using two original models");
    passed &= Expect(meshes.size() == 2 && scene.models.size() == 3,
        "store two meshes and three instances");
    if (scene.models.size() == 3) {
        passed &= Expect(scene.models[0].meshIndex == 0 && scene.models[1].meshIndex == 1 &&
            scene.models[2].meshIndex == 0, "reuse the first mesh for the third instance");
        passed &= Expect(FindModelById(scene, "target_2") == &scene.models[2] &&
            FindModelById(scene, "missing") == nullptr,
            "find a scene model by stable id");
        const ModelScene& readOnlyScene = scene;
        passed &= Expect(FindModelById(readOnlyScene, "left_fist") == &scene.models[1],
            "find a scene model from a const scene");
        passed &= Expect(scene.models[0].position.x == -1.6f &&
            scene.models[2].position.y == -1.4f && scene.models[2].scale == 0.5f,
            "preserve each instance's transform");
        passed &= Expect(scene.models[1].initialZDegrees == 90.0f,
            "preserve the left fist model roll");
    }

    config.models[1].relativeObjPath = "Missing/Missing.obj";
    passed &= Expect(!LoadSceneAssets(modelDirectory, config, meshes, scene, error) &&
        !error.empty(), "report a missing OBJ");
    passed &= Expect(meshes.empty() && scene.models.empty(),
        "failure leaves no partial meshes or instances");

    config.models[1].relativeObjPath = "LfistTEST/LfistTEST.obj";
    config.models[1].id = "target_1";
    passed &= Expect(!LoadSceneAssets(modelDirectory, config, meshes, scene, error) &&
        error.find("duplicate model id") != std::string::npos,
        "reject duplicate ids from programmatically built config");

    if (passed) {
        std::cout << "PASS: shared scene loading and mesh reuse\n";
    }
    return passed ? 0 : 1;
}
