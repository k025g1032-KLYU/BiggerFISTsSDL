#include "Data/ModelConfig.h"

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
}

int main() {
    const auto configPath = std::filesystem::current_path() / "model-config-test.cfg";
    ModelConfig config;
    std::string error;
    bool passed = true;

    {
        std::ofstream file(configPath, std::ios::binary);
        file << "# Select an original asset\nmodel Target/Target.obj\n";
    }
    passed &= Expect(LoadModelConfig(configPath, config, error), "load Target selection");
    passed &= Expect(config.relativeObjPath == "Target/Target.obj", "resolve Target relative path");

    {
        std::ofstream file(configPath, std::ios::binary);
        file << "\xEF\xBB\xBFmodel LFist/Lfist.obj\r\n";
    }
    passed &= Expect(LoadModelConfig(configPath, config, error), "load LFist selection with UTF-8 BOM");
    passed &= Expect(config.relativeObjPath == "LFist/Lfist.obj", "resolve LFist relative path");

    {
        std::ofstream file(configPath);
        file << "model ../outside.obj\n";
    }
    passed &= Expect(!LoadModelConfig(configPath, config, error) &&
        error.find("cannot contain") != std::string::npos, "reject parent directory traversal");
    passed &= Expect(config.relativeObjPath.empty(), "invalid selection leaves no partial config");

    {
        std::ofstream file(configPath);
        file << "model C:/outside.obj\n";
    }
    passed &= Expect(!LoadModelConfig(configPath, config, error) &&
        error.find("relative .obj") != std::string::npos, "reject an absolute path");

    {
        std::ofstream file(configPath);
        file << "model Target/Target.obj\nmodel LFist/Lfist.obj\n";
    }
    passed &= Expect(!LoadModelConfig(configPath, config, error) &&
        error.find(":2:") != std::string::npos, "reject duplicate model entries with line number");
    passed &= Expect(config.relativeObjPath.empty(), "duplicate selection leaves no partial config");

    std::filesystem::remove(configPath);
    if (passed) {
        std::cout << "PASS: model selection and invalid config cases\n";
    }
    return passed ? 0 : 1;
}
