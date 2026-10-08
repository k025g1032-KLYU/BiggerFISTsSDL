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
#include "Data/StoryConfig.h"
#include "Game/GameFlow.h"
#include "Gameplay/Tutorial/TutorialSession.h"
#include "Rendering/FullGameUi.h"
#include "Rendering/UiRenderer.h"
#include "Collision/Collision.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace {
TutorialObservation ObserveTutorial(const FirstPersonGame& player) {
    TutorialObservation observation{};
    observation.cameraYaw = player.GetCamera().GetYaw();
    observation.cameraPitch = player.GetCamera().GetPitch();
    observation.playerPosition = player.GetPlayer().GetPosition();
    observation.leftFistAway = player.GetLeftArm().IsFistAway();
    observation.rightFistAway = player.GetRightArm().IsFistAway();
    observation.playerHit = player.GetEvents().playerHit &&
        player.GetEvents().playerHitResult.wasApplied;
    observation.dodgeStarted = player.GetEvents().dodgeStarted;
    for (const CombatHitEvent& hit : player.GetEvents().targetHits) {
        const bool left = hit.armSide == PunchArmSide::Left;
        const bool punch = hit.attackType == PunchArmAttackType::Punch;
        const bool rocket = hit.attackType == PunchArmAttackType::RocketPunch;
        if (left && punch) observation.leftPunchHit = true;
        if (!left && punch) observation.rightPunchHit = true;
        if (left && rocket) observation.leftRocketHit = true;
        if (!left && rocket) observation.rightRocketHit = true;
        if (hit.targetId == "target_tutorial_missile")
            observation.defenseIntercepted = true;
    }
    return observation;
}
}

int Application::Run(const ApplicationOptions& options) {
    const bool battlePreview = options.battlePreview != BattlePreviewKind::None;
    const bool fullGame = !options.scenePreview && !options.firstPersonPreview;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    const GameConfig config = LoadGameConfig();
    const InputBindings bindings = LoadInputBindings();
    const DisplaySettings displaySettings = LoadDisplaySettings();

    SDL_Window* window = SDL_CreateWindow(
        fullGame ? "BiggerFISTs" :
        (battlePreview ? "BiggerFISTs SDL3 Battle Preview" :
        (options.playerCombatPreview ? "BiggerFISTs SDL3 Player Combat Test" :
        (options.firstPersonPreview ? "BiggerFISTs SDL3 First Person" :
        (options.missileUnitModelPreview ? "BiggerFISTs SDL3 Missile Unit Model" :
        (options.scenePreview ? "BiggerFISTs SDL3 Scene Preview" : "BiggerFISTs SDL3 Model Viewer"))))),
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
    if (options.scenePreview || options.firstPersonPreview || fullGame) {
        const char* sceneName = (battlePreview || fullGame) ? "battlefield-scene.cfg" :
            (options.firstPersonPreview ? "first-person-scene.cfg" :
            (options.missileUnitModelPreview ? "missile-unit-scene.cfg" : "scene.cfg"));
        SceneConfig sceneConfig;
        if (!LoadSceneConfig(executableDirectory / sceneName, sceneConfig, loadError) ||
            !LoadSceneAssets(executableDirectory / "assets/models", sceneConfig,
                meshes, previewScene, loadError,
                (battlePreview || fullGame || options.firstPersonPreview)
                    ? ModelOrigin::Authored : ModelOrigin::BoundsCenter)) {
            SDL_Log("%s", loadError.c_str());
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        if (battlePreview || fullGame) {
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
    UiRenderer uiRenderer;
    UiDrawList uiDrawList;
    StoryConfig storyConfig;
    std::optional<GameFlow> gameFlow;
    std::optional<TutorialSession> tutorial;
    if (fullGame) {
        if (!LoadStoryConfig(executableDirectory / "story-pages.cfg", storyConfig, loadError) ||
            !uiRenderer.Initialize(renderer.GetDevice(), window,
                executableDirectory / "assets", storyConfig, loadError)) {
            SDL_Log("%s", loadError.c_str());
            uiRenderer.Shutdown();
            renderer.Shutdown();
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        gameFlow.emplace(storyConfig);
        tutorial.emplace();
    }
    Game game(config, previewScene);
    std::optional<FirstPersonGame> firstPersonGame;
    std::optional<FirstPersonScenePresenter> firstPersonView;
    std::optional<CombatFeedback> combatFeedback;
    std::optional<CombatSession> combatSession;
    if (options.firstPersonPreview || fullGame) {
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
                feedbackConfig, loadError) ||
            (fullGame && !combatFeedback->InitializeFullGameAudio(
                executableDirectory / "assets/sound", loadError))) {
            SDL_Log("%s", loadError.c_str());
            combatFeedback->Shutdown();
            renderer.Shutdown();
            SDL_DestroyWindow(window);
            SDL_Quit();
            return 1;
        }
        if (!fullGame && !options.smokeTest && !SDL_SetWindowRelativeMouseMode(window, true)) {
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
    float cockpitStartupRemaining = 0.0f;
    bool cockpitVisible = true;
    float tutorialDefenseElapsed = 0.0f;
    bool tutorialMissileActive = false;
    Vector3 tutorialMissilePosition{};
    double fullGameElapsed = 0.0;
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
        GameInput gameInput = inputActions.Evaluate(input,
            combatFeedback ? combatFeedback->GetGamepad() : nullptr, deltaTime);
        if (!battlePreview && !fullGame) {
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
                input.GetMouseDeltaX() + gameInput.lookX,
                input.GetMouseDeltaY() + gameInput.lookY);
            const int simulationSteps = fixedStepClock.Advance(deltaTime);
            for (int step = 0; step < simulationSteps; ++step) {
                const SimulationStepInput stepInput =
                    fixedStepInput.ConsumeStep(simulationSteps - step);
                if (gameFlow) {
                    const bool tutorialWasComplete = tutorial->IsComplete();
                    if (gameFlow->GetPhase() == FlowPhase::Tutorial &&
                        (stepInput.game.skip.pressed ||
                         (stepInput.game.skipPhase.pressed && !tutorial->IsComplete()))) {
                        tutorial->SkipToFreePractice();
                    }
                    FlowBattleResult result = FlowBattleResult::None;
                    if (combatSession) {
                        result = combatSession->GetPhase() == BattlePhase::Clear
                            ? FlowBattleResult::Clear :
                            combatSession->GetPhase() == BattlePhase::GameOver
                            ? FlowBattleResult::GameOver : FlowBattleResult::Playing;
                    }
                    gameFlow->Update(static_cast<float>(FixedStepClock::kStepSeconds),
                        stepInput.game, result, tutorialWasComplete);
                    if (gameFlow->ConsumePhaseChanged()) {
                        combatSession.reset();
                        firstPersonGame->ResetCombatState();
                        if (fullGame) {
                            std::vector<std::string> targetIds;
                            for (const FirstPersonTarget& target : firstPersonGame->GetTargets())
                                targetIds.push_back(target.id);
                            for (const std::string& id : targetIds)
                                firstPersonGame->RemoveTarget(id);
                        }
                        const FlowPhase phase = gameFlow->GetPhase();
                        SDL_Log("Full game phase: %d", static_cast<int>(phase));
                        const char* phaseTitle = phase == FlowPhase::Title
                            ? "BiggerFISTs | Title" : phase == FlowPhase::Prologue
                            ? "BiggerFISTs | Prologue" : phase == FlowPhase::Story0
                            ? "BiggerFISTs | Story 0" : phase == FlowPhase::Tutorial
                            ? "BiggerFISTs | Tutorial" : phase == FlowPhase::Story1
                            ? "BiggerFISTs | Story 1" : phase == FlowPhase::Day1
                            ? "BiggerFISTs | Day 1" : phase == FlowPhase::Story2
                            ? "BiggerFISTs | Story 2" : phase == FlowPhase::Day2
                            ? "BiggerFISTs | Day 2" : phase == FlowPhase::Story3
                            ? "BiggerFISTs | Story 3" : phase == FlowPhase::Boss
                            ? "BiggerFISTs | Boss" : "BiggerFISTs | Ending";
                        SDL_SetWindowTitle(window, phaseTitle);
                        cockpitVisible = true;
                        if (phase == FlowPhase::Tutorial) {
                            tutorial->Start(ObserveTutorial(*firstPersonGame));
                            firstPersonGame->SpawnTarget({"target_tutorial_left", {-1.55f, 4.55f, 9.0f}, 1,
                                {}, false, 2.0f});
                            firstPersonGame->SpawnTarget({"target_tutorial_right", {1.55f, 4.55f, 9.0f}, 1,
                                {}, false, 2.0f});
                            cockpitStartupRemaining = 4.8f;
                            tutorialDefenseElapsed = 0.0f;
                            tutorialMissileActive = false;
                        } else if (gameFlow->IsBattlePhase()) {
                            const BattlePreviewKind kind = phase == FlowPhase::Day1
                                ? BattlePreviewKind::Day1 : phase == FlowPhase::Day2
                                ? BattlePreviewKind::Day2 : BattlePreviewKind::Boss;
                            EncounterConfig encounter;
                            BattleCombatConfig battleConfig;
                            const char* encounterName = kind == BattlePreviewKind::Day1
                                ? "day1-encounter.cfg" : "day2-encounter.cfg";
                            if (!LoadBattleCombatConfig(executableDirectory, battleConfig, loadError) ||
                                (kind != BattlePreviewKind::Boss &&
                                !LoadEncounterConfig(executableDirectory / encounterName,
                                    encounter, loadError))) {
                                SDL_Log("%s", loadError.c_str());
                                exitCode = 1;
                                isRunning = false;
                                break;
                            }
                            combatSession.emplace(kind, *firstPersonGame,
                                std::move(encounter), battleConfig);
                            cockpitStartupRemaining = 4.8f;
                        } else {
                            firstPersonView->ClearDynamic();
                            cockpitStartupRemaining = 0.0f;
                        }
                        if (!options.smokeTest) {
                            const bool capture = phase == FlowPhase::Tutorial ||
                                gameFlow->IsBattlePhase();
                            SDL_SetWindowRelativeMouseMode(window, capture);
                        }
                        const std::string music = phase == FlowPhase::Tutorial ? "elevator" :
                            phase == FlowPhase::Day1 || phase == FlowPhase::Day2 ? "energetic" :
                            phase == FlowPhase::Boss ? "boss" :
                            phase == FlowPhase::Ending ? "win" :
                            phase == FlowPhase::Title ? "" : "hisAction";
                        combatFeedback->SetMusic(music);
                    }
                    if (gameFlow->GetScreenState().presentation != FlowPresentation::Normal) {
                        continue;
                    }
                    if (cockpitStartupRemaining > 0.0f) {
                        cockpitStartupRemaining -= static_cast<float>(FixedStepClock::kStepSeconds);
                        if (cockpitStartupRemaining <= 0.0f && combatSession)
                            combatSession->Start(options.smokeTest);
                        continue;
                    }
                    if (stepInput.game.switchView.pressed &&
                        (gameFlow->IsBattlePhase() ||
                         (gameFlow->GetPhase() == FlowPhase::Tutorial &&
                          tutorial->IsCockpitConnected()))) {
                        cockpitVisible = !cockpitVisible;
                    }
                    if (gameFlow->GetPhase() == FlowPhase::Tutorial) {
                        GameInput tutorialInput = tutorial->FilterInput(stepInput.game);
                        if (tutorial->GetStep() == TutorialStep::Defense) {
                            tutorialDefenseElapsed += static_cast<float>(FixedStepClock::kStepSeconds);
                            if (!tutorialMissileActive && tutorialDefenseElapsed >= 1.0f) {
                                const Vector3& position = firstPersonGame->GetPlayer().GetPosition();
                                tutorialMissilePosition = {position.x, 4.55f, position.z + 18.0f};
                                tutorialMissileActive = firstPersonGame->SpawnTarget(
                                    {"target_tutorial_missile", tutorialMissilePosition,
                                        1, {}, false, 1.0f});
                                tutorialDefenseElapsed = 0.0f;
                            }
                        }
                        const Vector3 previousMissilePosition = tutorialMissilePosition;
                        if (tutorialMissileActive) {
                            tutorialMissilePosition.z -=
                                12.0f * static_cast<float>(FixedStepClock::kStepSeconds);
                            firstPersonGame->MoveTarget("target_tutorial_missile",
                                tutorialMissilePosition);
                        }
                        firstPersonGame->Update(FixedStepClock::kStepSeconds,
                            tutorialInput, stepInput.mouseDeltaX, stepInput.mouseDeltaY);
                        if (tutorialMissileActive) {
                            const auto& targets = firstPersonGame->GetTargets();
                            const auto missile = std::find_if(targets.begin(), targets.end(),
                                [](const FirstPersonTarget& target) {
                                    return target.id == "target_tutorial_missile";
                                });
                            if (missile == targets.end() || missile->hitPoints <= 0) {
                                firstPersonGame->RemoveTarget("target_tutorial_missile");
                                tutorialMissileActive = false;
                            }
                        }
                        if (tutorialMissileActive) {
                            const Vector3& position = firstPersonGame->GetPlayer().GetPosition();
                            const Collision::Sphere body{
                                {position.x, 4.55f, position.z}, 1.6f};
                            if (Collision::SweepSphereAgainstSphere(previousMissilePosition,
                                    tutorialMissilePosition, 1.0f, body).didHit) {
                                firstPersonGame->ReceivePlayerHit(PlayerHitType::Projectile);
                                firstPersonGame->RemoveTarget("target_tutorial_missile");
                                tutorialMissileActive = false;
                            } else if (tutorialMissilePosition.z < position.z - 5.0f) {
                                firstPersonGame->RemoveTarget("target_tutorial_missile");
                                tutorialMissileActive = false;
                            }
                        }
                        tutorial->Update(static_cast<float>(FixedStepClock::kStepSeconds),
                            stepInput.game, ObserveTutorial(*firstPersonGame));
                        if (tutorial->GetStep() != TutorialStep::Defense && tutorialMissileActive) {
                            firstPersonGame->RemoveTarget("target_tutorial_missile");
                            tutorialMissileActive = false;
                        }
                        if (const char* cue = tutorial->ConsumeVoiceCue())
                            combatFeedback->PlayTutorialVoice(cue);
                        if (tutorial->GetStep() == TutorialStep::RocketPunch) {
                            for (const char* id : {"target_tutorial_left", "target_tutorial_right"}) {
                                auto found = std::find_if(firstPersonGame->GetTargets().begin(),
                                    firstPersonGame->GetTargets().end(),
                                    [&](const FirstPersonTarget& target) { return target.id == id; });
                                if (found != firstPersonGame->GetTargets().end() &&
                                    found->hitPoints <= 0) {
                                    firstPersonGame->RemoveTarget(id);
                                    firstPersonGame->SpawnTarget({id,
                                        {std::string_view(id).ends_with("left") ? -1.55f : 1.55f,
                                            4.55f, 9.0f},
                                        1, {}, false, 2.0f});
                                }
                            }
                        }
                    } else if (!combatSession) {
                        continue;
                    }
                }
                if (combatSession) {
                    const BattlePhase previousPhase = combatSession->GetPhase();
                    const int previousBaseHitPoints = combatSession->GetBaseHitPoints();
                    const int previousWaveIndex = combatSession->GetWaveIndex();
                    const BattleBoss* previousBoss = combatSession->GetBoss();
                    const int previousBossHitPoints = previousBoss
                        ? previousBoss->GetHitPoints() : -1;
                    combatSession->Update(static_cast<float>(FixedStepClock::kStepSeconds),
                        stepInput.game, stepInput.mouseDeltaX, stepInput.mouseDeltaY);
                    firstPersonView->QueueVisualEvents(
                        combatSession->GetVisualEvents());
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
                } else if (!gameFlow) {
                    firstPersonGame->Update(FixedStepClock::kStepSeconds, stepInput.game,
                        stepInput.mouseDeltaX, stepInput.mouseDeltaY);
                }
                if (combatSession) {
                    for (const BattleFeedbackEvent feedbackEvent :
                            combatSession->GetFeedbackEvents()) {
                        combatFeedback->PlayBattleEvent(feedbackEvent);
                    }
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
            firstPersonView->AdvanceVisualTime(deltaTime);
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
            } else if (gameFlow && gameFlow->GetPhase() == FlowPhase::Tutorial) {
                firstPersonView->SyncTutorial(*firstPersonGame, battleAssets,
                    tutorial->ShouldShowTargets());
            }
            if (gameFlow && (gameFlow->GetPhase() == FlowPhase::Tutorial ||
                    gameFlow->IsBattlePhase())) {
                firstPersonView->SyncCockpit(*firstPersonGame, battleAssets,
                    cockpitVisible && (gameFlow->GetPhase() != FlowPhase::Tutorial ||
                    tutorial->IsCockpitConnected() || cockpitStartupRemaining > 0.0f),
                    std::clamp(cockpitStartupRemaining / 4.8f, 0.0f, 1.0f));
            }
        } else {
            game.Update(deltaTime, gameInput);
        }
        const ModelScene& scene = firstPersonView ? firstPersonView->GetScene() : game.GetScene();
        if (gameFlow) {
            fullGameElapsed += deltaTime;
            BuildFullGameUi(gameFlow->GetScreenState(),
                gameFlow->GetPhase() == FlowPhase::Tutorial ? &*tutorial : nullptr,
                combatSession ? &*combatSession : nullptr, *firstPersonGame,
                fullGameElapsed, cockpitStartupRemaining, uiDrawList);
        }
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
        if (!renderer.Render(scene, windowSettings, &frameRendered,
                gameFlow ? &uiRenderer : nullptr, gameFlow ? &uiDrawList : nullptr)) {
            SDL_Log("3D rendering failed");
            isRunning = false;
            exitCode = 1;
        }
        if (options.smokeTest) {
            smokeRenderNanoseconds += SDL_GetTicksNS() - renderStarted;
        }
        if (options.smokeTest) {
            if (frameRendered && ++renderedFrames >= options.smokeFrames) {
                SDL_Log("Smoke timing: update %.2f ms/frame, render %.2f ms/frame",
                    smokeUpdateNanoseconds / (static_cast<double>(options.smokeFrames) * 1000000.0),
                    smokeRenderNanoseconds / (static_cast<double>(options.smokeFrames) * 1000000.0));
                if (options.dynamicTargetSmokeTest) {
                    SDL_Log("PASS: Dynamic Target spawned, moved, removed and rendered across 30 GPU frames");
                } else if (gameFlow) {
                    SDL_Log("PASS: Full game rendered %d GPU frames with UI", options.smokeFrames);
                } else if (battlePreview) {
                    SDL_Log("PASS: Battle preview rendered %d GPU frames with %u models, %u enemies and %u projectiles",
                        options.smokeFrames,
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
            else if (SDL_GetTicks() - smokeStarted >
                    static_cast<Uint64>(10000 + options.smokeFrames * 1000 / 30)) {
                SDL_Log("3D smoke test timed out");
                isRunning = false;
                exitCode = 1;
            }
        }

        frameTimer.EndFrame();
        if (gameFlow && combatFeedback)
            combatFeedback->UpdateMusic(static_cast<float>(deltaTime));
    }

    if (options.firstPersonPreview && !options.smokeTest) {
        SDL_SetWindowRelativeMouseMode(window, false);
    }
    if (combatFeedback) {
        combatFeedback->Shutdown();
    }
    uiRenderer.Shutdown();
    renderer.Shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}
