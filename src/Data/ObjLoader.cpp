#include "Data/ObjLoader.h"

#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <limits>
#include <functional>
#include <sstream>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {
struct Position { float x, y, z; };
struct Texcoord { float u, v; };
struct FaceKey {
    int position;
    int uv;
    int normal;

    bool operator==(const FaceKey&) const = default;
};
struct FaceKeyHash {
    size_t operator()(const FaceKey& key) const {
        size_t hash = std::hash<int>{}(key.position);
        hash ^= std::hash<int>{}(key.uv) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        hash ^= std::hash<int>{}(key.normal) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        return hash;
    }
};

bool ParsePositiveIndex(std::string_view text, int& value) {
    if (text.empty()) {
        return false;
    }
    const auto* first = text.data();
    const auto* last = first + text.size();
    const auto [end, code] = std::from_chars(first, last, value);
    return code == std::errc{} && end == last && value > 0;
}

bool ParseFaceKey(std::string_view text, FaceKey& key) {
    const auto firstSlash = text.find('/');
    if (firstSlash == std::string_view::npos) {
        return false;
    }
    const auto secondSlash = text.find('/', firstSlash + 1);
    if (secondSlash == std::string_view::npos || text.find('/', secondSlash + 1) != std::string_view::npos) {
        return false;
    }
    return ParsePositiveIndex(text.substr(0, firstSlash), key.position) &&
        ParsePositiveIndex(text.substr(firstSlash + 1, secondSlash - firstSlash - 1), key.uv) &&
        ParsePositiveIndex(text.substr(secondSlash + 1), key.normal);
}

std::string TrimWhitespace(std::string text) {
    const auto first = text.find_first_not_of(" \t\r");
    if (first == std::string::npos) {
        return {};
    }
    const auto last = text.find_last_not_of(" \t\r");
    return text.substr(first, last - first + 1);
}

std::filesystem::path PathFromUtf8(const std::string& text) {
    const auto* bytes = reinterpret_cast<const char8_t*>(text.data());
    return std::filesystem::path(std::u8string(bytes, bytes + text.size()));
}

std::string PathToUtf8(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return { reinterpret_cast<const char*>(bytes.data()), bytes.size() };
}

bool LoadMaterialTexture(const std::filesystem::path& mtlPath,
    const std::unordered_set<std::string>& materialNames,
    std::filesystem::path& texturePath, std::string& error) {
    std::ifstream file(mtlPath);
    if (!file) {
        error = "Could not open material file: " + PathToUtf8(mtlPath);
        return false;
    }
    std::string line;
    std::string currentMaterial;
    std::unordered_map<std::string, std::filesystem::path> materialTextures;
    while (std::getline(file, line)) {
        std::istringstream input(line);
        std::string command;
        input >> command;
        if (command == "newmtl") {
            std::getline(input, currentMaterial);
            currentMaterial = TrimWhitespace(currentMaterial);
        } else if (command == "map_Kd" && materialNames.contains(currentMaterial)) {
            std::string filename;
            std::getline(input, filename);
            filename = TrimWhitespace(filename);
            if (filename.empty()) {
                error = "Material '" + currentMaterial + "' has an empty map_Kd in " + PathToUtf8(mtlPath);
                return false;
            }
            const auto path = (mtlPath.parent_path() / PathFromUtf8(filename)).lexically_normal();
            const auto [found, inserted] = materialTextures.emplace(currentMaterial, path);
            if (!inserted && found->second != path) {
                error = "Material '" + currentMaterial + "' has multiple map_Kd textures in " +
                    PathToUtf8(mtlPath);
                return false;
            }
        }
    }
    if (!file.eof() && file.fail()) {
        error = "Could not read material file: " + PathToUtf8(mtlPath);
        return false;
    }
    for (const auto& name : materialNames) {
        const auto found = materialTextures.find(name);
        if (found == materialTextures.end()) {
            error = "Material '" + name + "' has no map_Kd texture in " + PathToUtf8(mtlPath);
            return false;
        }
        if (texturePath.empty()) {
            texturePath = found->second;
        } else if (texturePath != found->second) {
            error = "Materials use different PNG textures; multiple textures are not supported: " +
                PathToUtf8(mtlPath);
            return false;
        }
    }
    return true;
}
}

bool LoadObjModel(const std::filesystem::path& objPath, MeshData& result, std::string& error) {
    result = {};
    error.clear();
    std::ifstream file(objPath);
    if (!file) {
        error = "Could not open OBJ file: " + PathToUtf8(objPath);
        return false;
    }

    MeshData mesh;
    std::vector<Position> positions;
    std::vector<Texcoord> texcoords;
    size_t normalCount = 0;
    std::unordered_map<FaceKey, std::uint32_t, FaceKeyHash> vertexLookup;
    std::string mtlFilename;
    std::string materialName;
    std::unordered_set<std::string> usedMaterials;
    std::string line;
    size_t lineNumber = 0;
    while (std::getline(file, line)) {
        ++lineNumber;
        std::istringstream input(line);
        std::string command;
        input >> command;
        if (command == "v") {
            Position value{};
            if (!(input >> value.x >> value.y >> value.z) ||
                !std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) {
                error = "Invalid position";
                break;
            }
            positions.push_back(value);
        } else if (command == "vt") {
            Texcoord value{};
            if (!(input >> value.u >> value.v) || !std::isfinite(value.u) || !std::isfinite(value.v)) {
                error = "Invalid UV coordinate";
                break;
            }
            texcoords.push_back(value);
        } else if (command == "vn") {
            Position value{};
            if (!(input >> value.x >> value.y >> value.z) ||
                !std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(value.z)) {
                error = "Invalid normal";
                break;
            }
            ++normalCount;
        } else if (command == "mtllib") {
            if (!mtlFilename.empty()) {
                error = "Multiple material libraries are not supported";
                break;
            }
            std::getline(input, mtlFilename);
            mtlFilename = TrimWhitespace(mtlFilename);
        } else if (command == "usemtl") {
            std::string name;
            std::getline(input, name);
            name = TrimWhitespace(name);
            if (name.empty()) {
                error = "Material name is empty";
                break;
            }
            materialName = name;
        } else if (command == "f") {
            if (materialName.empty()) {
                error = "Face has no material";
                break;
            }
            usedMaterials.insert(materialName);
            std::array<std::uint32_t, 4> face{};
            size_t corners = 0;
            std::string text;
            while (input >> text) {
                if (text[0] == '#') {
                    break;
                }
                FaceKey key{};
                if (corners >= face.size() || !ParseFaceKey(text, key) ||
                    static_cast<size_t>(key.position) > positions.size() ||
                    static_cast<size_t>(key.uv) > texcoords.size() ||
                    static_cast<size_t>(key.normal) > normalCount) {
                    error = "Unsupported face or invalid vertex index";
                    break;
                }
                const auto found = vertexLookup.find(key);
                if (found != vertexLookup.end()) {
                    face[corners++] = found->second;
                } else {
                    if (mesh.vertices.size() >= std::numeric_limits<std::uint32_t>::max()) {
                        error = "Too many OBJ vertices";
                        break;
                    }
                    const auto& position = positions[static_cast<size_t>(key.position - 1)];
                    const auto& uv = texcoords[static_cast<size_t>(key.uv - 1)];
                    const std::uint32_t index = static_cast<std::uint32_t>(mesh.vertices.size());
                    mesh.vertices.push_back({ { position.x, position.y, position.z }, { uv.u, 1.0f - uv.v } });
                    vertexLookup.emplace(key, index);
                    face[corners++] = index;
                }
            }
            if (!error.empty()) {
                break;
            }
            if (corners < 3) {
                error = "Face has fewer than three corners";
                break;
            }
            // Fan triangulation preserves the OBJ face order for triangles and quads.
            for (size_t i = 1; i + 1 < corners; ++i) {
                mesh.indices.insert(mesh.indices.end(), { face[0], face[i], face[i + 1] });
            }
        }
    }
    if (!error.empty()) {
        error = PathToUtf8(objPath) + ":" + std::to_string(lineNumber) + ": " + error;
        return false;
    }
    if (!file.eof() && file.fail()) {
        error = "Could not read OBJ file: " + PathToUtf8(objPath);
        return false;
    }
    if (mesh.indices.empty() || mtlFilename.empty() || usedMaterials.empty()) {
        error = "OBJ needs faces, mtllib and usemtl: " + PathToUtf8(objPath);
        return false;
    }
    if (!LoadMaterialTexture(objPath.parent_path() / PathFromUtf8(mtlFilename),
            usedMaterials, mesh.texturePath, error)) {
        return false;
    }
    result = std::move(mesh);
    return true;
}
