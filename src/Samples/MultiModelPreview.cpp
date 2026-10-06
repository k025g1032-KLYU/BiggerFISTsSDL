#include "Data/SceneConfig.h"
#include "Data/SceneLoader.h"
#include "Platform/FrameTimer.h"
#include "Platform/WindowSettings.h"
#include "Rendering/MeshRenderer.h"
#include "World/ModelScene.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstring>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

namespace {
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
                SDL_Log("PASS: MultiModelPreview rendered 30 frames with %u models",
                    static_cast<Uint32>(scene.models.size()));
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
        SceneConfig config;
        std::string error;
        std::vector<MeshData> meshes;
        ModelScene scene;
        if (!LoadSceneConfig(executableDirectory / "scene.cfg", config, error)) {
            SDL_Log("%s", error.c_str());
        } else {
            SDL_Log("Loaded scene.cfg: %u model instances", static_cast<Uint32>(config.models.size()));
            if (LoadSceneAssets(executableDirectory / "assets/models", config, meshes, scene, error)) {
                SDL_Log("Loaded %u unique mesh resources", static_cast<Uint32>(meshes.size()));
                MeshRenderer renderer;
                if (renderer.Initialize(window, std::span<const MeshData>(meshes))) {
                    exitCode = RunPreview(renderer, windowSettings, scene, smokeTest);
                }
                renderer.Shutdown();
            } else {
                SDL_Log("%s", error.c_str());
            }
        }
    }
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}
