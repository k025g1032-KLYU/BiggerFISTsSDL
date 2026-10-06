#include "App/Application.h"

#include "Platform/FrameTimer.h"
#include "Game/Game.h"
#include "Data/GameConfig.h"
#include "Input/Input.h"
#include "Input/InputActions.h"
#include "Input/InputBindings.h"
#include "Platform/WindowSettings.h"
#include "Data/ObjLoader.h"
#include "Data/ModelConfig.h"
#include "Rendering/MeshRenderer.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <string>

int Application::Run(bool smokeTest) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    const GameConfig config = LoadGameConfig();
    const InputBindings bindings = LoadInputBindings();
    const DisplaySettings displaySettings = LoadDisplaySettings();

    SDL_Window* window = SDL_CreateWindow(
        "BiggerFISTs SDL3 Model Viewer",
        displaySettings.windowWidth,
        displaySettings.windowHeight,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
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

    const char* basePath = SDL_GetBasePath();
    if (basePath == nullptr) {
        SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    const std::filesystem::path executableDirectory =
        std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(basePath)));
    ModelConfig modelConfig;
    std::string loadError;
    if (!LoadModelConfig(executableDirectory / "model.cfg", modelConfig, loadError)) {
        SDL_Log("%s", loadError.c_str());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    const std::filesystem::path modelPath =
        executableDirectory / "assets/models" / modelConfig.relativeObjPath;
    MeshData model;
    if (!LoadObjModel(modelPath, model, loadError)) {
        SDL_Log("%s", loadError.c_str());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    const auto modelName = modelConfig.relativeObjPath.u8string();
    SDL_Log("Loaded model %s: %u vertices, %u triangles",
        reinterpret_cast<const char*>(modelName.c_str()),
        static_cast<Uint32>(model.vertices.size()), static_cast<Uint32>(model.indices.size() / 3));

    ModelScene previewScene;
    if (!TryConfigureModelPreview(model, previewScene)) {
        SDL_Log("Could not calculate preview bounds for the selected model");
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    MeshRenderer renderer;
    if (!renderer.Initialize(window, model)) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    Game game(config, previewScene);
    FrameTimer frameTimer(config.targetFps);
    Input input;
    InputActions inputActions(bindings);

    bool isRunning = true;
    int exitCode = 0;
    int renderedFrames = 0;
    const Uint64 smokeStarted = SDL_GetTicks();

    while (isRunning) {
        frameTimer.BeginFrame();
        const double deltaTime = frameTimer.GetDeltaTime();
        input.BeginFrame();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            input.ProcessEvent(event);
            windowSettings.ProcessEvent(event);

            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                isRunning = false;
            }
        }

        input.Update();
        const GameInput gameInput = inputActions.Evaluate(input);

        if (gameInput.quit.pressed) {
            isRunning = false;
        }

        if (!isRunning) {
            break;
        }

        windowSettings.ApplyPending();
        game.Update(deltaTime, gameInput);
        bool frameRendered = false;
        if (!renderer.Render(game.GetScene(), windowSettings, &frameRendered)) {
            SDL_Log("3D rendering failed");
            isRunning = false;
            exitCode = 1;
        }
        if (smokeTest) {
            if (frameRendered && ++renderedFrames >= 30) {
                SDL_Log("PASS: BiggerFISTsSDL rendered 30 3D frames");
                isRunning = false;
            }
            else if (SDL_GetTicks() - smokeStarted > 10000) {
                SDL_Log("3D smoke test timed out");
                isRunning = false;
                exitCode = 1;
            }
        }

        frameTimer.EndFrame();
    }

    renderer.Shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}
