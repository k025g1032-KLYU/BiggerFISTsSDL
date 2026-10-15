#pragma once

#include <cstdint>
#include <filesystem>
#include <vector>

struct MeshVertex {
    float position[3];
    float uv[2];
    float normal[3]{0.0f, 1.0f, 0.0f};
};

struct MeshMaterial {
    float ambient[3]{0.3f, 0.3f, 0.3f};
    float diffuse[3]{0.7f, 0.7f, 0.7f};
    float specular[3]{0.5f, 0.5f, 0.5f};
    float opacity = 1.0f;
};

struct MeshSubmesh {
    // A contiguous range of triangle indices using one PNG texture.
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::filesystem::path texturePath;
    MeshMaterial material{};
};

struct MeshData {
    std::vector<MeshVertex> vertices;
    std::vector<std::uint32_t> indices;
    // Used by meshes without submeshes; OBJ loading also fills this when all groups share one PNG.
    std::filesystem::path texturePath;
    std::vector<MeshSubmesh> submeshes;
};
