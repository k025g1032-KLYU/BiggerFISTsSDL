#pragma once

#include "Data/MeshData.h"

#include <filesystem>
#include <string>

// Loads one textured OBJ mesh. Supported faces are triangles and quads with
// positive position/UV/normal indices; the material must name one PNG via map_Kd.
bool LoadObjModel(const std::filesystem::path& objPath, MeshData& result, std::string& error);
