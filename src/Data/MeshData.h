#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

struct MeshVertex {
    float position[3];
    float uv[2];
};

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    std::filesystem::path texturePath;
};
