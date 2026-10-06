#include "Data/ObjLoader.h"
#include "Platform/FrameTimer.h"
#include "Platform/WindowSettings.h"
#include "Rendering/MeshRenderer.h"
#include "World/ModelScene.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <span>
#include <string>

namespace {
bool LoadPreviewModels(const std::filesystem::path& executableDirectory,
    std::array<MeshData, 2>& meshes, ModelScene& scene) {
    const std::array<std::filesystem::path, 2> paths{
        executableDirectory / "assets/models/Target/Target.obj",
        executableDirectory / "assets/models/LfistTEST/LfistTEST.obj"
    };
    std::string error;
    for (std::size_t i = 0; i < meshes.size(); ++i) {
        if (!LoadObjModel(paths[i], meshes[i], error)) {
            SDL_Log("%s", error.c_str());
            return false;
        }
        ModelBounds bounds;
        if (!TryCalculateModelBounds(meshes[i], bounds)) {
            SDL_Log("Could not calculate bounds for preview model %u", static_cast<Uint32>(i));
            return false;
        }
        scene.models[i].meshIndex = i;
        scene.models[i].modelCenter = bounds.center;
        const auto name = paths[i].filename().u8string();
        SDL_Log("Loaded preview model %s: %u vertices, %u triangles",
            reinterpret_cast<const char*>(name.c_str()),
            static_cast<Uint32>(meshes[i].vertices.size()),
            static_cast<Uint32>(meshes[i].indices.size() / 3));
    }

    scene.models[0].position.x = -1.6f;
    scene.models[0].initialXDegrees = 0.0f;
    scene.models[0].initialYDegrees = 0.0f;
    scene.models[0].rotationXDegreesPerSecond = 0.0f;
    scene.models[0].rotationYDegreesPerSecond = 20.0f;
    scene.models[1].position.x = 1.6f;
    scene.models[1].initialXDegrees = 15.0f;
    scene.models[1].initialYDegrees = 30.0f;
    scene.models[1].rotationXDegreesPerSecond = 0.0f;
    scene.models[1].rotationYDegreesPerSecond = -25.0f;
    scene.cameraPosition = { 0.0f, 0.0f, -6.0f };
    return true;
}

int RunPreview(MeshRenderer& renderer, WindowSettings& windowSettings,
    ModelScene& scene, bool smokeTest) {
    FrameTimer frameTimer(60);
    const Uint64 smokeStarted = SDL_GetTicks();
    int renderedFrames = 0;
    bool running = true;
    while (running) {
        frameTimer.BeginFrame();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            windowSettings.ProcessEvent(event);
            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE)) {
                running = false;
            }
        }
        if (!running) {
            break;
        }
        if (!windowSettings.ApplyPending()) {
            SDL_Log("Could not apply pending display settings");
            return 1;
        }
        scene.elapsedSeconds += frameTimer.GetDeltaTime();
        bool frameRendered = false;
        if (!renderer.Render(scene, windowSettings, &frameRendered)) {
            SDL_Log("Multi-model rendering failed");
            return 1;
        }
        if (smokeTest) {
            if (frameRendered && ++renderedFrames >= 30) {
                SDL_Log("PASS: MultiModelPreview rendered 30 frames with 2 models");
                return 0;
            }
            if (SDL_GetTicks() - smokeStarted > 10000) {
                SDL_Log("Multi-model smoke test timed out");
                return 1;
            }
        }
        frameTimer.EndFrame();
    }
    return 0;
}
}

int main(int argc, char** argv) {
    const bool smokeTest = argc > 1 && std::strcmp(argv[1], "--smoke-test") == 0;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    const DisplaySettings displaySettings = LoadDisplaySettings();
    SDL_Window* window = SDL_CreateWindow("BiggerFISTs SDL3 Multi-Model Preview",
        displaySettings.windowWidth, displaySettings.windowHeight,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    WindowSettings windowSettings(window);
    if (!windowSettings.Apply(displaySettings)) {
        SDL_Log("Using the current window after display settings could not be applied");
    }
    SDL_Log("Display shortcuts: F11 fullscreen, F1 1280x720, F2 1600x900, F8 next display");

    int exitCode = 1;
    const char* basePath = SDL_GetBasePath();
    if (basePath == nullptr) {
        SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
    } else {
        const std::filesystem::path executableDirectory =
            std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(basePath)));
        std::array<MeshData, 2> meshes;
        ModelScene scene;
        scene.models.resize(meshes.size());
        if (LoadPreviewModels(executableDirectory, meshes, scene)) {
            MeshRenderer renderer;
            if (renderer.Initialize(window, std::span<const MeshData>(meshes))) {
                exitCode = RunPreview(renderer, windowSettings, scene, smokeTest);
            }
            renderer.Shutdown();
        }
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}
