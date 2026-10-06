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
#include "Platform/CombatFeedback.h"
#include "Data/ObjLoader.h"
#include "Data/ModelConfig.h"
#include "Data/SceneConfig.h"
#include "Data/SceneLoader.h"
#include "Data/CombatFeedbackConfig.h"
#include "Data/PlayerCombatConfig.h"
#include "Data/BattleAssetCatalog.h"
#include "Data/BattleCombatConfig.h"
#include "Rendering/MeshRenderer.h"
#include "Rendering/FirstPersonScenePresenter.h"
#include "Gameplay/Combat/CombatSession.h"
#include "Gameplay/Combat/EncounterConfig.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

int Application::Run(const ApplicationOptions& options) {
    const bool battlePreview = options.battlePreview != BattlePreviewKind::None;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    const GameConfig config = LoadGameConfig();
    const InputBindings bindings = LoadInputBindings();
    const DisplaySettings displaySettings = LoadDisplaySettings();

    SDL_Window* window = SDL_CreateWindow(
        battlePreview ? "BiggerFISTs SDL3 Battle Preview" :
        (options.playerCombatPreview ? "BiggerFISTs SDL3 Player Combat Test" :
        (options.firstPersonPreview ? "BiggerFISTs SDL3 First Person" :
        (options.missileUnitModelPreview ? "BiggerFISTs SDL3 Missile Unit Model" :
        (options.scenePreview ? "BiggerFISTs SDL3 Scene Preview" : "BiggerFISTs SDL3 Model Viewer")))),
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
    BattleAssetCatalog battleAssets;
    std::string loadError;
    if (options.scenePreview || options.firstPersonPreview) {
        const char* sceneName = battlePreview ? "battlefield-scene.cfg" :
            (options.firstPersonPreview ? "first-person-scene.cfg" :
            (options.missileUnitModelPreview ? "missile-unit-scene.cfg" : "scene.cfg"));
        SceneConfig sceneConfig;
        if (!LoadSceneConfig(executableDirectory / sceneName, sceneConfig, loadError) ||
            !LoadSceneAssets(executableDirectory / "assets/models", sceneConfig,
                meshes, previewScene, loadError)) {
            SDL_Log("%s", loadError.c_str());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        if (battlePreview) {
            if (!LoadBattleAssetCatalog(executableDirectory / "battle-assets.cfg",
                    executableDirectory / "assets/models", meshes, battleAssets, loadError)) {
                SDL_Log("%s", loadError.c_str());
                SDL_DestroyWindow(window);
                SDL_Quit();
                return 1;
            }
            if (FindModelById(previewScene, "sky_dome") == nullptr ||
                FindModelById(previewScene, "research_institute") == nullptr ||
                FindModelById(previewScene, "left_fist") == nullptr ||
                FindModelById(previewScene, "right_fist") == nullptr ||
                FindModelById(previewScene, "left_upper_arm") == nullptr ||
                FindModelById(previewScene, "right_upper_arm") == nullptr) {
                SDL_Log("Battlefield scene is missing required models");
                SDL_DestroyWindow(window);
                SDL_Quit();
                return 1;
            }
            for (ModelInstance& model : previewScene.models) {
                model.modelCenter = {};
            }
            previewScene.farPlane = 1000.0f;
        } else if (options.firstPersonPreview) {
            ModelInstance* leftFist = FindModelById(previewScene, "left_fist");
            ModelInstance* rightFist = FindModelById(previewScene, "right_fist");
            ModelInstance* leftUpperArm = FindModelById(previewScene, "left_upper_arm");
            ModelInstance* rightUpperArm = FindModelById(previewScene, "right_upper_arm");
            bool validTargets = FindModelById(previewScene, "target") != nullptr;
            for (ModelInstance& model : previewScene.models) {
                if (model.id == "left_fist" || model.id == "right_fist" ||
                    model.id == "left_upper_arm" || model.id == "right_upper_arm") {
                    model.modelCenter = {};
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
            if (previewScene.models.size() != firstPersonTargets.size() + 4 ||
                !validTargets || leftFist == nullptr || rightFist == nullptr ||
                leftUpperArm == nullptr || rightUpperArm == nullptr ||
                leftFist->rotationXDegreesPerSecond != 0.0f ||
                leftFist->rotationYDegreesPerSecond != 0.0f ||
                rightFist->rotationXDegreesPerSecond != 0.0f ||
                rightFist->rotationYDegreesPerSecond != 0.0f ||
                leftUpperArm->rotationXDegreesPerSecond != 0.0f ||
                leftUpperArm->rotationYDegreesPerSecond != 0.0f ||
                rightUpperArm->rotationXDegreesPerSecond != 0.0f ||
                rightUpperArm->rotationYDegreesPerSecond != 0.0f) {
                SDL_Log("First-person scene requires Targets, fists and upper arms");
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
    std::optional<CombatFeedback> combatFeedback;
    std::optional<CombatSession> combatSession;
    if (options.firstPersonPreview) {
        PlayerCombatConfig playerCombatConfig;
        if (!LoadPlayerCombatConfig(executableDirectory / "player-combat.cfg",
                playerCombatConfig, loadError)) {
            SDL_Log("%s", loadError.c_str());
            renderer.Shutdown();
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        const Vector3 initialEyePosition = previewScene.cameraPosition;
        firstPersonGame.emplace(config, initialEyePosition, std::move(firstPersonTargets),
            playerCombatConfig);
        if (battlePreview) {
            BattleCombatConfig battleConfig;
            if (!LoadBattleCombatConfig(executableDirectory, battleConfig, loadError)) {
                SDL_Log("%s", loadError.c_str());
                renderer.Shutdown();
                SDL_DestroyWindow(window);
                SDL_Quit();
                return 1;
            }
            EncounterConfig encounter;
            if (options.battlePreview == BattlePreviewKind::Day1 ||
                options.battlePreview == BattlePreviewKind::Day2) {
                const char* name = options.battlePreview == BattlePreviewKind::Day1
                    ? "day1-encounter.cfg" : "day2-encounter.cfg";
                if (!LoadEncounterConfig(executableDirectory / name, encounter, loadError)) {
                    SDL_Log("%s", loadError.c_str());
                    renderer.Shutdown();
                    SDL_DestroyWindow(window);
                    SDL_Quit();
                    return 1;
                }
            }
            combatSession.emplace(options.battlePreview, *firstPersonGame,
                std::move(encounter), battleConfig);
            if (options.smokeTest) {
                combatSession->Start(true);
            }
        }
        firstPersonView.emplace(std::move(previewScene));
        firstPersonView->Sync(*firstPersonGame);
        combatFeedback.emplace();
        CombatFeedbackConfig feedbackConfig;
        if (!LoadCombatFeedbackConfig(executableDirectory / "combat-feedback.cfg",
                feedbackConfig, loadError) ||
            !combatFeedback->Initialize(executableDirectory / "assets/sound",
                feedbackConfig, loadError)) {
            SDL_Log("%s", loadError.c_str());
            combatFeedback->Shutdown();
            renderer.Shutdown();
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        if (!options.smokeTest && !SDL_SetWindowRelativeMouseMode(window, true)) {
            SDL_Log("Could not capture mouse for first-person view: %s", SDL_GetError());
            combatFeedback->Shutdown();
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
    if (options.playerCombatPreview) {
        SDL_Log("Player combat test: H projectile, J charge impact, K spread burst, R reset");
    }
    if (battlePreview) {
        SDL_Log("Battle preview: Enter start, R reset, WASD move, mouse look, mouse buttons attack");
    }

    bool isRunning = true;
    int exitCode = 0;
    int renderedFrames = 0;
    Uint64 smokeUpdateNanoseconds = 0;
    Uint64 smokeRenderNanoseconds = 0;
    double battleStatusElapsed = 0.0;
    const Uint64 smokeStarted = SDL_GetTicks();

    while (isRunning) {
        const Uint64 frameStarted = SDL_GetTicksNS();
        frameTimer.BeginFrame();
        const double deltaTime = frameTimer.GetDeltaTime();
        input.BeginFrame();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            input.ProcessEvent(event);
            windowSettings.ProcessEvent(event);
            if (combatFeedback) {
                combatFeedback->ProcessEvent(event);
            }
            if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST) {
                fixedStepInput.Reset();
            }

            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                isRunning = false;
            }
        }

        input.Update();
        GameInput gameInput = inputActions.Evaluate(input);
        if (!battlePreview) {
            gameInput.dodge = {};
            gameInput.battleStart = {};
        } else {
            gameInput.changeBackground = {};
        }
        if (!options.playerCombatPreview) {
            gameInput.testProjectileHit = {};
            gameInput.testChargeHit = {};
            gameInput.testBurstHit = {};
        }

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
                if (combatSession) {
                    const BattlePhase previousPhase = combatSession->GetPhase();
                    const int previousBaseHitPoints = combatSession->GetBaseHitPoints();
                    const int previousWaveIndex = combatSession->GetWaveIndex();
                    const BattleBoss* previousBoss = combatSession->GetBoss();
                    const int previousBossHitPoints = previousBoss
                        ? previousBoss->GetHitPoints() : -1;
                    combatSession->Update(static_cast<float>(FixedStepClock::kStepSeconds),
                        stepInput.game, stepInput.mouseDeltaX, stepInput.mouseDeltaY);
                    if (combatSession->GetBaseHitPoints() != previousBaseHitPoints) {
                        SDL_Log("Base hit: HP=%d", combatSession->GetBaseHitPoints());
                    }
                    if (combatSession->GetWaveIndex() != previousWaveIndex) {
                        SDL_Log("Wave %d/%d started", combatSession->GetWaveIndex() + 1,
                            combatSession->GetWaveCount());
                    }
                    if (const BattleBoss* boss = combatSession->GetBoss(); boss &&
                        previousBossHitPoints >= 0 &&
                        boss->GetHitPoints() != previousBossHitPoints) {
                        SDL_Log("Boss core hit: HP=%d/%d", boss->GetHitPoints(),
                            boss->GetMaximumHitPoints());
                    }
                    if (combatSession->GetPhase() != previousPhase) {
                        const char* label = combatSession->GetPhase() == BattlePhase::Playing
                            ? "Playing" : combatSession->GetPhase() == BattlePhase::Clear
                            ? "Clear" : combatSession->GetPhase() == BattlePhase::GameOver
                            ? "GameOver" : combatSession->GetPhase() == BattlePhase::Countdown
                            ? "Countdown" : "Ready";
                        SDL_Log("Battle %s: player HP=%d, base HP=%d, wave=%d/%d",
                            label, firstPersonGame->GetVitality().GetHitPoints(),
                            combatSession->GetBaseHitPoints(),
                            combatSession->GetWaveIndex() + 1,
                            combatSession->GetWaveCount());
                    }
                } else {
                    firstPersonGame->Update(FixedStepClock::kStepSeconds, stepInput.game,
                        stepInput.mouseDeltaX, stepInput.mouseDeltaY);
                }
                if (firstPersonGame->GetEvents().punchHit) {
                    combatFeedback->PlayPunchHit();
                }
                if (firstPersonGame->GetEvents().rocketHit) {
                    combatFeedback->PlayRocketHit();
                }
                if (firstPersonGame->GetEvents().leftPunchBegan ||
                    firstPersonGame->GetEvents().rightPunchBegan) {
                    combatFeedback->PlayPunchSwing();
                }
                if (firstPersonGame->GetEvents().leftRocketReconnected ||
                    firstPersonGame->GetEvents().rightRocketReconnected) {
                    combatFeedback->PlayRocketReconnect();
                }
                if (firstPersonGame->GetEvents().playerHit) {
                    combatFeedback->PlayPlayerHit(firstPersonGame->GetEvents().playerHitType,
                        firstPersonGame->GetEvents().playerHitResult.didDamageHitPoints);
                    SDL_Log("Player hit: HP=%d, shields=%d, lost=%d%s",
                        firstPersonGame->GetVitality().GetHitPoints(),
                        firstPersonGame->GetVitality().GetShieldLayers(),
                        firstPersonGame->GetEvents().playerHitResult.shieldLayersLost,
                        firstPersonGame->GetEvents().playerDefeated ? ", defeated" : "");
                }
                if (firstPersonGame->GetEvents().leftRocketLaunched) {
                    combatFeedback->PlayRocketLaunch();
                }
                if (firstPersonGame->GetEvents().rightRocketLaunched) {
                    combatFeedback->PlayRocketLaunch();
                }
                if (firstPersonGame->GetEvents().ultimateLaunched) {
                    combatFeedback->PlayUltimateLaunch();
                }
                if (firstPersonGame->GetEvents().dodgeStarted) {
                    combatFeedback->PlayDodge();
                }
                if (firstPersonGame->GetEvents().systemRestarted) {
                    combatFeedback->PlaySystemRestart();
                }
                if (firstPersonGame->GetEvents().targetDestroyed) {
                    if (firstPersonGame->GetEvents().punchHit) {
                        SDL_Log("Target destroyed by Punch. Press R to reset the Targets.");
                    }
                    if (firstPersonGame->GetEvents().rocketHit) {
                        SDL_Log("Target destroyed by Rocket Punch. Press R to reset the Targets.");
                    }
                }
                if (firstPersonGame->GetEvents().targetReset) {
                    SDL_Log("Player, arms and Targets reset: HP=%d, shields=%d.",
                        firstPersonGame->GetVitality().GetHitPoints(),
                        firstPersonGame->GetVitality().GetShieldLayers());
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
            if (options.dynamicTargetSmokeTest) {
                bool updated = true;
                if (renderedFrames == 5) {
                    updated = firstPersonGame->SpawnTarget(
                        {"target_dynamic", {0.0f, 4.55f, 10.0f}, 1});
                } else if (renderedFrames > 5 && renderedFrames < 25) {
                    updated = firstPersonGame->MoveTarget("target_dynamic",
                        {0.05f * static_cast<float>(renderedFrames - 5), 4.55f, 10.0f});
                } else if (renderedFrames == 25) {
                    updated = firstPersonGame->RemoveTarget("target_dynamic");
                }
                if (!updated) {
                    SDL_Log("Dynamic Target smoke test could not update the Target");
                    exitCode = 1;
                    isRunning = false;
                    break;
                }
            }
            firstPersonView->Sync(*firstPersonGame);
            if (combatSession) {
                firstPersonView->SyncBattle(*combatSession, battleAssets);
                battleStatusElapsed += deltaTime;
                if (battleStatusElapsed >= 0.25) {
                    battleStatusElapsed = 0.0;
                    const char* phase = combatSession->GetPhase() == BattlePhase::Playing
                        ? "Playing" : combatSession->GetPhase() == BattlePhase::Clear
                        ? "Clear" : combatSession->GetPhase() == BattlePhase::GameOver
                        ? "Game Over" : combatSession->GetPhase() == BattlePhase::Countdown
                        ? "Countdown" : "Ready";
                    char title[256]{};
                    if (const BattleBoss* boss = combatSession->GetBoss()) {
                        std::snprintf(title, sizeof(title),
                            "BiggerFISTs | %s | HP %d | Boss %d/%d | Base %d",
                            phase, firstPersonGame->GetVitality().GetHitPoints(),
                            boss->GetHitPoints(), boss->GetMaximumHitPoints(),
                            combatSession->GetBaseHitPoints());
                    } else {
                        std::snprintf(title, sizeof(title),
                            "BiggerFISTs | %s | HP %d | Base %d | Wave %d/%d | Enemy %u",
                            phase, firstPersonGame->GetVitality().GetHitPoints(),
                            combatSession->GetBaseHitPoints(),
                            combatSession->GetWaveIndex() + 1,
                            combatSession->GetWaveCount(),
                            static_cast<unsigned>(combatSession->GetEnemies().size()));
                    }
                    SDL_SetWindowTitle(window, title);
                }
            }
        } else {
            game.Update(deltaTime, gameInput);
        }
        const ModelScene& scene = firstPersonView ? firstPersonView->GetScene() : game.GetScene();
        if (options.dynamicTargetSmokeTest) {
            const ModelInstance* dynamicTarget = FindModelById(scene, "target_dynamic");
            const bool shouldExist = renderedFrames >= 5 && renderedFrames < 25;
            const float expectedX = 0.05f * static_cast<float>(renderedFrames - 5);
            if ((dynamicTarget != nullptr) != shouldExist ||
                scene.models.size() != (shouldExist ? 7u : 6u) ||
                (shouldExist && std::abs(dynamicTarget->position.x - expectedX) > 0.0001f)) {
                SDL_Log("Dynamic Target smoke test scene state mismatch at frame %d",
                    renderedFrames);
                exitCode = 1;
                break;
            }
        }
        bool frameRendered = false;
        const Uint64 renderStarted = SDL_GetTicksNS();
        if (options.smokeTest) {
            smokeUpdateNanoseconds += renderStarted - frameStarted;
        }
        if (!renderer.Render(scene, windowSettings, &frameRendered)) {
            SDL_Log("3D rendering failed");
            isRunning = false;
            exitCode = 1;
        }
        if (options.smokeTest) {
            smokeRenderNanoseconds += SDL_GetTicksNS() - renderStarted;
        }
        if (options.smokeTest) {
            if (frameRendered && ++renderedFrames >= 30) {
                SDL_Log("Smoke timing: update %.2f ms/frame, render %.2f ms/frame",
                    smokeUpdateNanoseconds / 30000000.0,
                    smokeRenderNanoseconds / 30000000.0);
                if (options.dynamicTargetSmokeTest) {
                    SDL_Log("PASS: Dynamic Target spawned, moved, removed and rendered across 30 GPU frames");
                } else if (battlePreview) {
                    SDL_Log("PASS: Battle preview rendered 30 GPU frames with %u models, %u enemies and %u projectiles",
                        static_cast<Uint32>(scene.models.size()),
                        static_cast<Uint32>(combatSession->GetEnemies().size()),
                        static_cast<Uint32>(combatSession->GetProjectiles().size()));
                } else if (options.firstPersonPreview) {
                    SDL_Log("PASS: BiggerFISTsSDL %s rendered 30 3D frames with Targets, fists and upper arms",
                        options.playerCombatPreview ? "player combat preview" : "first-person preview");
                } else if (options.missileUnitModelPreview) {
                    SDL_Log("PASS: Original MissileUnit model rendered 30 3D frames with two PNG materials");
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
    if (combatFeedback) {
        combatFeedback->Shutdown();
    }
    renderer.Shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}
