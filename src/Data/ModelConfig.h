#pragma once

#include <filesystem>
#include <string>

struct ModelConfig {
    std::filesystem::path relativeObjPath;
};

bool LoadModelConfig(const std::filesystem::path& configPath, ModelConfig& result, std::string& error);
