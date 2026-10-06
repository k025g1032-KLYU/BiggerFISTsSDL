#include "Data/SceneConfig.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {
bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}

void WriteConfig(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream file(path, std::ios::binary);
    file << contents;
}
}

int main() {
    const auto configPath = std::filesystem::current_path() / "scene-config-test.cfg";
    const std::string validScene =
        "\xEF\xBB\xBF# Two original models\r\n"
        "camera 0 0 -6\r\n"
        "model Target/Target.obj\r\n"
        "id target\r\n"
        "position -1.6 0 0\r\n"
        "scale 1\r\n"
        "rotation 0 0\r\n"
        "spin 0 20\r\n"
        "model LfistTEST/LfistTEST.obj\r\n"
        "id left_fist\r\n"
        "position 1.6 0 0\r\n"
        "scale 0.8\r\n"
        "rotation 15 30 90\r\n"
        "spin 0 -25\r\n";
    SceneConfig config;
    std::string error;
    bool passed = true;

    WriteConfig(configPath, validScene);
    passed &= Expect(LoadSceneConfig(configPath, config, error), "load two model scene");
    passed &= Expect(config.models.size() == 2, "read two model instances");
    if (config.models.size() == 2) {
        passed &= Expect(config.cameraPosition.z == -6.0f, "read camera position");
        passed &= Expect(config.models[0].id == "target" &&
            config.models[0].relativeObjPath == "Target/Target.obj" &&
            config.models[0].position.x == -1.6f &&
            config.models[0].initialZDegrees == 0.0f &&
            config.models[0].rotationYDegreesPerSecond == 20.0f,
            "read first model placement and spin");
        passed &= Expect(config.models[1].id == "left_fist" &&
            config.models[1].relativeObjPath == "LfistTEST/LfistTEST.obj" &&
            config.models[1].position.x == 1.6f && config.models[1].scale == 0.8f &&
            config.models[1].initialXDegrees == 15.0f &&
            config.models[1].initialZDegrees == 90.0f,
            "read second model placement, scale and rotation");
    }

    WriteConfig(configPath,
        "camera 0 0 -6\nmodel ../outside.obj\nposition 0 0 0\nscale 1\nrotation 0 0\nspin 0 0\n");
    passed &= Expect(!LoadSceneConfig(configPath, config, error) &&
        error.find(":2:") != std::string::npos, "reject path traversal with line number");
    passed &= Expect(config.models.empty(), "invalid path leaves no partial scene");

    WriteConfig(configPath,
        "camera 0 0 -6\nmodel Target/Target.obj\nposition 0 0 0\nscale 0\nrotation 0 0\nspin 0 0\n");
    passed &= Expect(!LoadSceneConfig(configPath, config, error) &&
        error.find("positive") != std::string::npos, "reject zero scale");

    WriteConfig(configPath,
        "camera 0 0 -6\nmodel Target/Target.obj\nposition 0 0 0\nscale 1\nrotation 0 0\n");
    passed &= Expect(!LoadSceneConfig(configPath, config, error) &&
        error.find("requires id, position, scale, rotation and spin") != std::string::npos,
        "reject incomplete model block");

    WriteConfig(configPath,
        "camera 0 0 -6\nmodel Target/Target.obj\nposition 0 0 0\n"
        "scale 1\nrotation 0 0\nspin 0 0\n");
    passed &= Expect(!LoadSceneConfig(configPath, config, error) &&
        error.find(":2:") != std::string::npos &&
        error.find("requires id") != std::string::npos,
        "reject a model without an id");

    WriteConfig(configPath,
        "camera 0 0 -6\nmodel Target/Target.obj\nid target\nposition 0 0 0\n"
        "scale 1\nrotation 0 0\nspin 0 0\n"
        "model LfistTEST/LfistTEST.obj\nid target\nposition 1 0 0\n"
        "scale 1\nrotation 0 0\nspin 0 0\n");
    passed &= Expect(!LoadSceneConfig(configPath, config, error) &&
        error.find(":9:") != std::string::npos &&
        error.find("duplicate model id") != std::string::npos,
        "reject duplicate ids with the line number");

    WriteConfig(configPath,
        "camera 0 0 -6\nmodel Target/Target.obj\nid target\nposition 0 0 0\n"
        "scale 1\nrotation 0 0 90 30\nspin 0 0\n");
    passed &= Expect(!LoadSceneConfig(configPath, config, error) &&
        error.find("rotation") != std::string::npos,
        "reject more than three rotation values");

    WriteConfig(configPath,
        "camera 0 0 -6\nmodel Target/Target.obj\nid 2target\nposition 0 0 0\n"
        "scale 1\nrotation 0 0\nspin 0 0\n");
    passed &= Expect(!LoadSceneConfig(configPath, config, error) &&
        error.find(":3:") != std::string::npos,
        "reject invalid id syntax");

    WriteConfig(configPath,
        "camera 0 0 -6\nmodel Target/Target.obj\nposition 0 0 0\nposition 1 0 0\n"
        "scale 1\nrotation 0 0\nspin 0 0\n");
    passed &= Expect(!LoadSceneConfig(configPath, config, error) &&
        error.find(":4:") != std::string::npos, "reject duplicate property with line number");

    WriteConfig(configPath,
        "camera 0 0 -6\nmodel Target/Target.obj\nposition 0 0 0 extra\n"
        "scale 1\nrotation 0 0\nspin 0 0\n");
    passed &= Expect(!LoadSceneConfig(configPath, config, error), "reject extra numeric fields");

    std::filesystem::remove(configPath);
    if (passed) {
        std::cout << "PASS: two model scene and invalid config cases\n";
    }
    return passed ? 0 : 1;
}
