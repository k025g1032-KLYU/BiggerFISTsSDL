#include "Data/ObjLoader.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
bool Expect(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
    }
    return condition;
}
}

int main() {
    bool passed = true;
    MeshData model;
    std::string error;
    passed &= Expect(LoadObjModel(OBJ_TEST_MODEL_PATH, model, error), "load original Target OBJ");
    if (!passed) {
        std::cerr << error << '\n';
        return 1;
    }
    passed &= Expect(model.indices.size() == 960 * 3, "triangulate original triangles and quads");
    passed &= Expect(model.vertices.size() > 482, "split OBJ position/UV/normal combinations");
    passed &= Expect(model.texturePath.filename() == "target.png", "resolve map_Kd PNG");
    passed &= Expect(std::filesystem::exists(model.texturePath), "original PNG exists");
    passed &= Expect(!model.submeshes.empty() &&
        model.submeshes.front().material.ambient[0] == 1.0f &&
        model.submeshes.front().material.opacity == 1.0f,
        "preserve original ambient material and opacity");
    passed &= Expect(std::all_of(model.vertices.begin(), model.vertices.end(),
        [](const MeshVertex& vertex) {
            const float length = std::hypot(vertex.normal[0],
                vertex.normal[1], vertex.normal[2]);
            return std::isfinite(length) && length > 0.5f;
        }), "preserve valid OBJ vertex normals");
    for (const auto index : model.indices) {
        passed &= Expect(index < model.vertices.size(), "every index refers to a vertex");
        if (!passed) {
            break;
        }
    }

    passed &= Expect(LoadObjModel(OBJ_TEST_FIST_PATH, model, error), "load original LFist OBJ");
    if (passed) {
        passed &= Expect(!model.indices.empty() && model.indices.size() % 3 == 0,
            "LFist has drawable triangles");
        passed &= Expect(model.texturePath.filename() == "Lfist.png", "resolve LFist map_Kd PNG");
        passed &= Expect(std::filesystem::exists(model.texturePath), "original LFist PNG exists");
    } else {
        std::cerr << error << '\n';
    }

    const bool loadedTutorial = LoadObjModel(OBJ_TEST_TUTORIAL_PATH,
        model, error);
    passed &= Expect(loadedTutorial, "load original tutorial banner OBJ");
    if (loadedTutorial) {
        float leftUWorldX = 0.0f;
        float rightUWorldX = 0.0f;
        int leftCount = 0;
        int rightCount = 0;
        for (const MeshVertex& vertex : model.vertices) {
            // The original scene rotates this banner 180 degrees around Y.
            const float worldX = -vertex.position[0];
            if (vertex.uv[0] < 0.1f) {
                leftUWorldX += worldX;
                ++leftCount;
            } else if (vertex.uv[0] > 0.9f) {
                rightUWorldX += worldX;
                ++rightCount;
            }
        }
        passed &= Expect(leftCount > 0 && rightCount > 0 &&
            leftUWorldX / leftCount < rightUWorldX / rightCount,
            "tutorial banner keeps its texture left-to-right after the original Y rotation");
    }

    const bool loadedGameFist = LoadObjModel(OBJ_TEST_GAME_FIST_PATH, model, error);
    passed &= Expect(loadedGameFist, "load the left fist used by CombatScene");
    if (loadedGameFist) {
        passed &= Expect(model.indices.size() == 5192 * 3, "load both LfistTEST material groups");
        passed &= Expect(model.texturePath.filename() == "Lfist.png", "shared material texture is Lfist.png");
        passed &= Expect(std::filesystem::exists(model.texturePath), "game left fist PNG exists");
    } else {
        std::cerr << error << '\n';
    }

    const bool loadedRightFist = LoadObjModel(OBJ_TEST_RIGHT_FIST_PATH, model, error);
    passed &= Expect(loadedRightFist, "load the right fist used by CombatScene");
    if (loadedRightFist) {
        passed &= Expect(!model.indices.empty() && model.indices.size() % 3 == 0,
            "right fist has drawable triangles");
        passed &= Expect(model.texturePath.filename() == "Lfist.png" &&
            std::filesystem::exists(model.texturePath),
            "right fist materials share a readable PNG");
    } else {
        std::cerr << error << '\n';
    }

    const bool loadedMissileUnit = LoadObjModel(OBJ_TEST_MISSILE_UNIT_PATH, model, error);
    passed &= Expect(loadedMissileUnit, "load original MissileUnit OBJ");
    if (loadedMissileUnit) {
        passed &= Expect(model.texturePath.empty() && model.submeshes.size() == 2,
            "MissileUnit keeps its two distinct material groups");
        passed &= Expect(model.submeshes.size() == 2 &&
            model.submeshes[0].texturePath.filename() == "Launcherv2.png" &&
            model.submeshes[1].texturePath.filename() == "Launcher.png" &&
            std::filesystem::exists(model.submeshes[0].texturePath) &&
            std::filesystem::exists(model.submeshes[1].texturePath),
            "MissileUnit resolves both original PNG files");
        passed &= Expect(model.submeshes.size() == 2 &&
            model.submeshes[0].firstIndex == 0 &&
            model.submeshes[0].indexCount > 0 &&
            model.submeshes[1].firstIndex == model.submeshes[0].indexCount &&
            model.submeshes[0].indexCount + model.submeshes[1].indexCount ==
                model.indices.size(),
            "MissileUnit material ranges cover its complete index buffer");
    } else {
        std::cerr << error << '\n';
    }

    // Use a tiny quad to check winding and the OBJ bottom-left UV convention.
    const auto directory = std::filesystem::current_path() / "obj-loader-test-data";
    std::filesystem::create_directories(directory);
    const auto objPath = directory / "quad.obj";
    const auto mtlPath = directory / "quad.mtl";
    {
        std::ofstream mtl(mtlPath);
        mtl << "newmtl Test\nmap_Kd texture.png\n";
        std::ofstream obj(objPath);
        obj << "mtllib quad.mtl\n"
            << "v -1 1 0\nv 1 1 0\nv 1 -1 0\nv -1 -1 0\n"
            << "vt 0 1\nvt 1 1\nvt 1 0\nvt 0 0\n"
            << "vn 0 0 -1\nusemtl Test\nf 1/1/1 2/2/1 3/3/1 4/4/1\n";
    }
    passed &= Expect(LoadObjModel(objPath, model, error), "load a four-corner face");
    passed &= Expect(model.vertices.size() == 4 && model.indices.size() == 6,
        "quad becomes four vertices and two triangles");
    if (model.indices.size() == 6) {
        passed &= Expect(model.indices[0] == 0 && model.indices[1] == 2 && model.indices[2] == 1 &&
            model.indices[3] == 0 && model.indices[4] == 3 && model.indices[5] == 2,
            "left-handed OBJ conversion reverses face winding");
    }
    if (!model.vertices.empty()) {
        passed &= Expect(std::fabs(model.vertices[0].uv[0]) < 0.0001f &&
            std::fabs(model.vertices[0].uv[1]) < 0.0001f,
            "OBJ top-left becomes GPU UV (0,0)");
        passed &= Expect(std::fabs(model.vertices[0].position[0] - 1.0f) < 0.0001f &&
            std::fabs(model.vertices[1].position[0] + 1.0f) < 0.0001f,
            "OBJ X coordinates convert to the game's left-handed basis");
    }

    // Each material group keeps its own texture and index range.
    {
        std::ofstream mtl(mtlPath);
        mtl << "newmtl First\nmap_Kd texture.png\n"
            << "newmtl Second\nmap_Kd texture.png\n";
        std::ofstream obj(objPath);
        obj << "mtllib quad.mtl\n"
            << "v 0 0 0\nv 1 0 0\nv 0 1 0\n"
            << "vt 0 0\nvt 1 0\nvt 0 1\n"
            << "vn 0 0 1\n"
            << "usemtl First\nf 1/1/1 2/2/1 3/3/1\n"
            << "usemtl Second\nf 3/3/1 2/2/1 1/1/1\n";
    }
    passed &= Expect(LoadObjModel(objPath, model, error) && model.indices.size() == 6 &&
        model.submeshes.size() == 2 && !model.texturePath.empty(),
        "two named materials may share one PNG");
    {
        std::ofstream mtl(mtlPath);
        mtl << "newmtl First\nmap_Kd texture.png\n"
            << "newmtl Second\nmap_Kd different.png\n";
    }
    passed &= Expect(LoadObjModel(objPath, model, error) &&
        model.texturePath.empty() && model.submeshes.size() == 2 &&
        model.submeshes[0].firstIndex == 0 && model.submeshes[0].indexCount == 3 &&
        model.submeshes[1].firstIndex == 3 && model.submeshes[1].indexCount == 3 &&
        model.submeshes[1].texturePath.filename() == "different.png",
        "different material textures produce separate draw ranges");
    {
        std::ofstream mtl(mtlPath);
        mtl << "newmtl First\nmap_Kd texture.png\nnewmtl Second\n";
    }
    passed &= Expect(!LoadObjModel(objPath, model, error) &&
        error.find("has no map_Kd texture") != std::string::npos,
        "missing material texture fails explicitly");
    passed &= Expect(model.vertices.empty() && model.indices.empty() &&
        model.submeshes.empty(), "missing texture returns no partial mesh");

    // A bad face must fail with its line number instead of accessing past a vector.
    {
        std::ofstream obj(objPath);
        obj << "mtllib quad.mtl\nv 0 0 0\nvt 0 0\nvn 0 0 1\n"
            << "usemtl Test\nf 1/1/1 1/1/1 99/1/1\n";
    }
    passed &= Expect(!LoadObjModel(objPath, model, error) && error.find(":6:") != std::string::npos,
        "out-of-range face index reports the line");
    passed &= Expect(model.vertices.empty() && model.indices.empty(), "failure does not return partial mesh");

    std::filesystem::remove(objPath);
    std::filesystem::remove(mtlPath);
    std::filesystem::remove(directory);
    if (passed) {
        std::cout << "PASS: original Target, fist and MissileUnit OBJ files, material ranges and errors\n";
    }
    return passed ? 0 : 1;
}
