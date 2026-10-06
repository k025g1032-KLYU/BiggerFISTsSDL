#include "Data/ObjLoader.h"

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

    const bool loadedGameFist = LoadObjModel(OBJ_TEST_GAME_FIST_PATH, model, error);
    passed &= Expect(loadedGameFist, "load the left fist used by CombatScene");
    if (loadedGameFist) {
        passed &= Expect(model.indices.size() == 5192 * 3, "load both LfistTEST material groups");
        passed &= Expect(model.texturePath.filename() == "Lfist.png", "shared material texture is Lfist.png");
        passed &= Expect(std::filesystem::exists(model.texturePath), "game left fist PNG exists");
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
        passed &= Expect(model.indices[0] == 0 && model.indices[1] == 1 && model.indices[2] == 2 &&
            model.indices[3] == 0 && model.indices[4] == 2 && model.indices[5] == 3,
            "quad triangulation preserves face order");
    }
    if (!model.vertices.empty()) {
        passed &= Expect(std::fabs(model.vertices[0].uv[0]) < 0.0001f &&
            std::fabs(model.vertices[0].uv[1]) < 0.0001f,
            "OBJ top-left becomes GPU UV (0,0)");
    }

    // Multiple material names are valid only while they all use one texture.
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
    passed &= Expect(LoadObjModel(objPath, model, error) && model.indices.size() == 6,
        "two named materials may share one PNG");
    {
        std::ofstream mtl(mtlPath);
        mtl << "newmtl First\nmap_Kd texture.png\n"
            << "newmtl Second\nmap_Kd different.png\n";
    }
    passed &= Expect(!LoadObjModel(objPath, model, error) &&
        error.find("different PNG textures") != std::string::npos,
        "different material textures fail explicitly");
    passed &= Expect(model.vertices.empty() && model.indices.empty(),
        "unsupported materials return no partial mesh");

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
        std::cout << "PASS: original Target and fist OBJ files, shared materials and OBJ error cases\n";
    }
    return passed ? 0 : 1;
}
