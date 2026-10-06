#include "Game/Game.h"
#include "Data/MeshData.h"

#include <cmath>
#include <cstdio>

namespace {
bool Expect(bool condition, const char* description) {
    if (!condition) {
        std::printf("FAIL: %s\n", description);
    }
    return condition;
}

bool NearlyEqual(float a, float b) {
    return std::abs(a - b) < 0.0001f;
}
}

int main() {
    Game game(GameConfig{});
    GameInput input{};
    bool success = true;

    input.moveX = 1.0f;
    game.Update(0.5, input);
    success &= Expect(NearlyEqual(game.GetScene().models.front().position.x, 0.12f),
        "Movement uses world units and caps a long frame");
    success &= Expect(game.GetScene().elapsedSeconds == 0.5,
        "Animation uses real elapsed time rather than capped movement time");

    input.moveX = 0.0f;
    input.moveY = -1.0f;
    game.Update(0.05, input);
    success &= Expect(NearlyEqual(game.GetScene().models.front().position.y, 0.12f),
        "Move-up input increases world Y");

    input.moveY = 0.0f;
    input.changeBackground.pressed = true;
    game.Update(0.0, input);
    success &= Expect(game.GetScene().warmBackground,
        "Background changes on a press");
    input.changeBackground.pressed = false;
    game.Update(0.0, input);
    success &= Expect(game.GetScene().warmBackground,
        "A held or released key does not toggle again");
    input.changeBackground.pressed = true;
    game.Update(0.0, input);
    success &= Expect(!game.GetScene().warmBackground,
        "A second press restores the original background");

    Matrix4x4 transform{};
    success &= Expect(TryBuildModelTransform(game.GetScene(), 1280, 720, transform),
        "Game state produces a valid 3D transform");
    success &= Expect(!TryBuildModelTransform(game.GetScene(), 0, 720, transform),
        "An unavailable GPU output size is rejected");

    MeshData previewMesh;
    previewMesh.vertices = {
        { { -1.0f, -2.0f, -3.0f }, { 0.0f, 0.0f } },
        { { 3.0f, 2.0f, 5.0f }, { 1.0f, 1.0f } }
    };
    ModelScene previewScene;
    success &= Expect(TryConfigureModelPreview(previewMesh, previewScene),
        "Calculate a preview view from mesh positions");
    success &= Expect(NearlyEqual(previewScene.models.front().modelCenter.x, 1.0f) &&
        NearlyEqual(previewScene.models.front().modelCenter.y, 0.0f) &&
        NearlyEqual(previewScene.models.front().modelCenter.z, 1.0f) &&
        NearlyEqual(previewScene.cameraPosition.z, -12.0f),
        "Preview centers an offset model and moves the camera back");
    Game previewGame(GameConfig{}, previewScene);
    success &= Expect(NearlyEqual(previewGame.GetScene().models.front().modelCenter.z, 1.0f),
        "Game preserves the configured preview scene");
    previewMesh.vertices.clear();
    success &= Expect(!TryConfigureModelPreview(previewMesh, previewScene),
        "An empty mesh cannot configure the preview");

    ModelScene multiScene;
    multiScene.models.resize(2);
    multiScene.models[0].position.x = -1.5f;
    multiScene.models[1].meshIndex = 1;
    multiScene.models[1].position.x = 1.5f;
    multiScene.models[0].initialXDegrees = 0.0f;
    multiScene.models[0].initialYDegrees = 0.0f;
    multiScene.models[1].initialXDegrees = 0.0f;
    multiScene.models[1].initialYDegrees = 0.0f;
    Matrix4x4 leftTransform{};
    Matrix4x4 rightTransform{};
    success &= Expect(TryBuildModelTransform(multiScene, multiScene.models[0],
        1280, 720, leftTransform) &&
        TryBuildModelTransform(multiScene, multiScene.models[1],
            1280, 720, rightTransform),
        "Build independent transforms for two scene models");
    success &= Expect(leftTransform.elements[3][0] < 0.0f &&
        rightTransform.elements[3][0] > 0.0f,
        "Left and right model positions remain independent");

    if (success) {
        std::puts("PASS: 3D game input, scene state and transform");
    }
    return success ? 0 : 1;
}
