#include "App/Application.h"

#include "Platform/FrameTimer.h"
#include "Platform/FixedStepClock.h"
#include "Game/Game.h"
#include "Game/FirstPersonGame.h"
#include "Data/GameConfig.h"
#include "Input/Input.h"
#include "Input/InputActions.h"
#include "Input/InputBindings.h"
#include "Input/FixedStepInputBuffer.h"
#include "Platform/WindowSettings.h"
#include "Data/ObjLoader.h"
#include "Data/ModelConfig.h"
#include "Data/SceneConfig.h"
#include "Data/SceneLoader.h"
#include "Rendering/MeshRenderer.h"
#include "Rendering/FirstPersonScenePresenter.h"

#include <SDL3/SDL.h>

#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

int Application::Run(const ApplicationOptions& options) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    const GameConfig config = LoadGameConfig();
    const InputBindings bindings = LoadInputBindings();
    const DisplaySettings displaySettings = LoadDisplaySettings();

    SDL_Window* window = SDL_CreateWindow(
        options.firstPersonPreview ? "BiggerFISTs SDL3 First Person" :
        (options.scenePreview ? "BiggerFISTs SDL3 Scene Preview" : "BiggerFISTs SDL3 Model Viewer"),
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
    std::vector<MeshData> meshes;
    ModelScene previewScene;
    std::vector<FirstPersonTarget> firstPersonTargets;
    std::string loadError;
    if (options.scenePreview || options.firstPersonPreview) {
        const char* sceneName = options.firstPersonPreview ? "first-person-scene.cfg" : "scene.cfg";
        SceneConfig sceneConfig;
        if (!LoadSceneConfig(executableDirectory / sceneName, sceneConfig, loadError) ||
            !LoadSceneAssets(executableDirectory / "assets/models", sceneConfig,
                meshes, previewScene, loadError)) {
            SDL_Log("%s", loadError.c_str());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        if (options.firstPersonPreview) {
            ModelInstance* leftFist = FindModelById(previewScene, "left_fist");
            ModelInstance* rightFist = FindModelById(previewScene, "right_fist");
            bool validTargets = FindModelById(previewScene, "target") != nullptr;
            for (ModelInstance& model : previewScene.models) {
                if (model.id == "left_fist" || model.id == "right_fist") {
                    continue;
                }
                if ((model.id != "target" && !model.id.starts_with("target_")) ||
                    model.rotationXDegreesPerSecond != 0.0f ||
                    model.rotationYDegreesPerSecond != 0.0f) {
                    validTargets = false;
                    break;
                }
                model.modelCenter = {};
                firstPersonTargets.push_back({model.id, model.position, 1});
            }
            if (previewScene.models.size() != firstPersonTargets.size() + 2 ||
                !validTargets || leftFist == nullptr || rightFist == nullptr ||
                leftFist->rotationXDegreesPerSecond != 0.0f ||
                leftFist->rotationYDegreesPerSecond != 0.0f ||
                rightFist->rotationXDegreesPerSecond != 0.0f ||
                rightFist->rotationYDegreesPerSecond != 0.0f) {
                SDL_Log("First-person scene requires static target IDs, left_fist and right_fist");
                SDL_DestroyWindow(window);
                SDL_Quit();
                return 1;
            }
            leftFist->modelCenter = {};
            rightFist->modelCenter = {};
        }
        SDL_Log("Loaded %s: %u model instances, %u unique mesh resources",
            sceneName,
            static_cast<Uint32>(previewScene.models.size()), static_cast<Uint32>(meshes.size()));
    } else {
        ModelConfig modelConfig;
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
        if (!TryConfigureModelPreview(model, previewScene)) {
            SDL_Log("Could not calculate preview bounds for the selected model");
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        meshes.push_back(std::move(model));
    }
    MeshRenderer renderer;
    if (!renderer.Initialize(window, std::span<const MeshData>(meshes))) {
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    Game game(config, previewScene);
    std::optional<FirstPersonGame> firstPersonGame;
    std::optional<FirstPersonScenePresenter> firstPersonView;
    if (options.firstPersonPreview) {
        const Vector3 initialEyePosition = previewScene.cameraPosition;
        firstPersonGame.emplace(config, initialEyePosition, std::move(firstPersonTargets));
        firstPersonView.emplace(std::move(previewScene));
        firstPersonView->Sync(*firstPersonGame);
        if (!options.smokeTest && !SDL_SetWindowRelativeMouseMode(window, true)) {
            SDL_Log("Could not capture mouse for first-person view: %s", SDL_GetError());
            renderer.Shutdown();
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
    }
    FrameTimer frameTimer(config.targetFps);
    FixedStepClock fixedStepClock;
    FixedStepInputBuffer fixedStepInput;
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
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                fixedStepInput.Reset();
            }

            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                isRunning = false;
            }
        }

        input.Update();
        GameInput gameInput = inputActions.Evaluate(input);

        if (gameInput.quit.pressed) {
            isRunning = false;
        }

        if (!isRunning) {
            break;
        }

        windowSettings.ApplyPending();
        if (options.scenePreview) {
            gameInput.moveX = 0.0f;
            gameInput.moveY = 0.0f;
        }
        if (firstPersonGame) {
            fixedStepInput.QueueFrame(gameInput,
                input.GetMouseDeltaX(), input.GetMouseDeltaY());
            const int simulationSteps = fixedStepClock.Advance(deltaTime);
            for (int step = 0; step < simulationSteps; ++step) {
                const SimulationStepInput stepInput =
                    fixedStepInput.ConsumeStep(simulationSteps - step);
                firstPersonGame->Update(FixedStepClock::kStepSeconds, stepInput.game,
                    stepInput.mouseDeltaX, stepInput.mouseDeltaY);
                if (firstPersonGame->GetEvents().targetDestroyed) {
                    SDL_Log("Target destroyed by fist impact. Press R to reset the Target.");
                }
                if (firstPersonGame->GetEvents().targetReset) {
                    SDL_Log("Target and punch states reset.");
                }
                if (firstPersonGame->GetEvents().leftRocketLaunched) {
                    SDL_Log("Left Rocket Punch launched");
                }
                if (firstPersonGame->GetEvents().rightRocketLaunched) {
                    SDL_Log("Right Rocket Punch launched");
                }
                if (firstPersonGame->GetEvents().leftRocketReconnected) {
                    SDL_Log("Left Rocket Punch reconnected");
                }
                if (firstPersonGame->GetEvents().rightRocketReconnected) {
                    SDL_Log("Right Rocket Punch reconnected");
                }
            }
            firstPersonView->Sync(*firstPersonGame);
        } else {
            game.Update(deltaTime, gameInput);
        }
        const ModelScene& scene = firstPersonView ? firstPersonView->GetScene() : game.GetScene();
        bool frameRendered = false;
        if (!renderer.Render(scene, windowSettings, &frameRendered)) {
            SDL_Log("3D rendering failed");
            isRunning = false;
            exitCode = 1;
        }
        if (options.smokeTest) {
            if (frameRendered && ++renderedFrames >= 30) {
                if (options.firstPersonPreview) {
                    SDL_Log("PASS: BiggerFISTsSDL first-person preview rendered 30 3D frames with static Targets and two player fists");
                } else if (options.scenePreview) {
                    SDL_Log("PASS: BiggerFISTsSDL scene preview rendered 30 3D frames with %u models",
                        static_cast<Uint32>(scene.models.size()));
                } else {
                    SDL_Log("PASS: BiggerFISTsSDL rendered 30 3D frames");
                }
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

    if (options.firstPersonPreview && !options.smokeTest) {
        SDL_SetWindowRelativeMouseMode(window, false);
    }
    renderer.Shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}
