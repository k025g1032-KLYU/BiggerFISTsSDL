#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

struct MeshVertex {
    float position[3];
    float uv[2];
};

struct MeshSubmesh {
    // A contiguous range of triangle indices using one PNG texture.
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::filesystem::path texturePath;
};

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    // Used by meshes without submeshes; OBJ loading also fills this when all groups share one PNG.
    std::filesystem::path texturePath;
    std::vector<MeshSubmesh> submeshes;
};
