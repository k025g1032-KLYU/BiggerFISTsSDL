cmake_minimum_required(VERSION 3.21)

project(BiggerFISTsSDL LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SDL_SHARED ON CACHE BOOL "Build SDL3 as a shared library" FORCE)
set(SDL_STATIC OFF CACHE BOOL "Do not build SDL3 as a static library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Do not build SDL3 tests" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Do not build SDL3 examples" FORCE)

set(BIGGERFISTS_SDL_REQUIRED_VERSION "3.4.16")
set(BIGGERFISTS_SDL_REQUIRED_COMMIT "fa2c02bb6e21974a89ea9824bc53c9932abe5f9c")
set(BIGGERFISTS_SDL_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/SDL")

if(NOT EXISTS "${BIGGERFISTS_SDL_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR
        "SDL submodule is missing. Run 'git submodule update --init --recursive' "
        "from the project directory, then configure again."
    )
endif()

if(NOT EXISTS "${BIGGERFISTS_SDL_SOURCE_DIR}/.git")
    message(FATAL_ERROR
        "vendor/SDL must be the pinned Git submodule. "
        "Clone with --recurse-submodules or run 'git submodule update --init --recursive'."
    )
endif()

find_package(Git REQUIRED)
execute_process(
    COMMAND "${GIT_EXECUTABLE}" -C "${BIGGERFISTS_SDL_SOURCE_DIR}" rev-parse HEAD
    OUTPUT_VARIABLE BIGGERFISTS_SDL_CHECKOUT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
    RESULT_VARIABLE BIGGERFISTS_SDL_GIT_RESULT
)
if(NOT BIGGERFISTS_SDL_GIT_RESULT EQUAL 0 OR
   NOT BIGGERFISTS_SDL_CHECKOUT_COMMIT STREQUAL BIGGERFISTS_SDL_REQUIRED_COMMIT)
    message(FATAL_ERROR
        "SDL checkout does not match the pinned commit ${BIGGERFISTS_SDL_REQUIRED_COMMIT}. "
        "Run 'git submodule update --init --recursive' to restore it."
    )
endif()

add_subdirectory(vendor/SDL EXCLUDE_FROM_ALL)
get_target_property(BIGGERFISTS_SDL_ACTUAL_VERSION SDL3-shared SDL_FULL_VERSION)
if(NOT BIGGERFISTS_SDL_ACTUAL_VERSION STREQUAL BIGGERFISTS_SDL_REQUIRED_VERSION)
    message(FATAL_ERROR
        "Expected SDL ${BIGGERFISTS_SDL_REQUIRED_VERSION}, "
        "but vendor/SDL reports ${BIGGERFISTS_SDL_ACTUAL_VERSION}."
    )
endif()

set(SCENE_RENDERING_SOURCES
    src/World/ModelScene.cpp
    src/World/ModelScene.h
    src/Math/MathTypes.cpp
    src/Math/MathTypes.h
    src/Rendering/DepthBuffer.cpp
    src/Rendering/DepthBuffer.h
    src/Rendering/MeshRenderer.cpp
    src/Rendering/MeshRenderer.h
    src/Rendering/UiRenderer.cpp
    src/Rendering/UiRenderer.h
    src/Rendering/TextureLoader.cpp
    src/Rendering/TextureLoader.h
)

set(SCENE_LOADING_SOURCES
    src/Data/SceneConfig.cpp
    src/Data/SceneConfig.h
    src/Data/SceneLoader.cpp
    src/Data/SceneLoader.h
)

set(PLAYER_COMBAT_SOURCES
    src/Gameplay/Player/PlayerVitality.cpp
    src/Gameplay/Player/PlayerVitality.h
    src/Gameplay/Combat/CombatHitResolver.cpp
    src/Gameplay/Combat/CombatHitResolver.h
)

add_executable(BiggerFISTsSDL
    src/App/main.cpp
    src/App/Application.cpp
    src/App/Application.h
    src/Platform/WindowSettings.cpp
    src/Platform/WindowSettings.h
    src/Platform/FrameTimer.cpp
    src/Platform/FrameTimer.h
    src/Platform/FixedStepClock.cpp
    src/Platform/FixedStepClock.h
    src/Platform/CombatFeedback.cpp
    src/Platform/CombatFeedback.h
    src/Input/Input.cpp
    src/Input/Input.h
    src/Input/FixedStepInputBuffer.cpp
    src/Input/FixedStepInputBuffer.h
    src/Input/InputActions.cpp
    src/Input/InputActions.h
    src/Input/InputBindings.cpp
    src/Input/InputBindings.h
    src/Input/GameInput.h
    src/Data/GameConfig.cpp
    src/Data/GameConfig.h
    src/Data/CombatFeedbackConfig.cpp
    src/Data/CombatFeedbackConfig.h
    src/Data/PlayerCombatConfig.cpp
    src/Data/PlayerCombatConfig.h
    src/Data/BattleAssetCatalog.cpp
    src/Data/BattleAssetCatalog.h
    src/Gameplay/Combat/EncounterConfig.cpp
    src/Gameplay/Combat/EncounterConfig.h
    src/Gameplay/Combat/EncounterDirector.cpp
    src/Gameplay/Combat/EncounterDirector.h
    src/Gameplay/Combat/CombatSession.cpp
    src/Gameplay/Combat/CombatSession.h
    src/Gameplay/Tutorial/TutorialSession.cpp
    src/Gameplay/Tutorial/TutorialSession.h
    src/Gameplay/Cockpit/CockpitPresentation.cpp
    src/Gameplay/Cockpit/CockpitPresentation.h
    src/Gameplay/Combat/BattleBoss.cpp
    src/Gameplay/Combat/BattleBoss.h
    src/Data/BattleCombatConfig.cpp
    src/Data/BattleCombatConfig.h
    src/Rendering/BattleScenePresenter.cpp
    src/Rendering/FullGameUi.cpp
    src/Rendering/FullGameUi.h
    src/Game/GameFlow.cpp
    src/Game/GameFlow.h
    src/Data/StoryConfig.cpp
    src/Data/StoryConfig.h
    src/Data/ObjLoader.cpp
    src/Data/ObjLoader.h
    src/Data/MeshData.h
    src/Data/ModelConfig.cpp
    src/Data/ModelConfig.h
    src/Game/Game.cpp
    src/Game/Game.h
    src/Game/FirstPersonGame.cpp
    src/Game/FirstPersonGame.h
    src/Collision/Collision.cpp
    src/Collision/Collision.h
    src/Gameplay/Player/FirstPersonTuning.h
    src/Gameplay/Player/PlayerMovement.cpp
    src/Gameplay/Player/PlayerMovement.h
    src/Gameplay/Player/PunchArm.cpp
    src/Gameplay/Player/PunchArm.h
    ${PLAYER_COMBAT_SOURCES}
    src/Gameplay/SimulationTiming.h
    src/Rendering/FirstPersonScenePresenter.cpp
    src/Rendering/FirstPersonScenePresenter.h
    src/World/FirstPersonCamera.cpp
    src/World/FirstPersonCamera.h
    ${SCENE_LOADING_SOURCES}
    ${SCENE_RENDERING_SOURCES}
)
target_include_directories(BiggerFISTsSDL PRIVATE src)
target_link_libraries(BiggerFISTsSDL PRIVATE SDL3::SDL3-shared)

add_custom_target(CombatSoundAssets
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/sound"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/enemy_is_hit.wav"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/sound/enemy_is_hit.wav"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/RocketShot.wav"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/sound/RocketShot.wav"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/whoosh.wav"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/sound/whoosh.wav"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/mokugyo.wav"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/sound/mokugyo.wav"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/charge_hit.wav"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/sound/charge_hit.wav"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/step.wav"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/sound/step.wav"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/boss_change_mode.wav"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/sound/boss_change_mode.wav"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/boss_sharp_hit.wav"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/sound/boss_sharp_hit.wav"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/boss_riser.wav"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/sound/boss_riser.wav"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/battle_explosion.wav"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/sound/battle_explosion.wav"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/enemy_is_hit.wav"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/RocketShot.wav"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/whoosh.wav"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/mokugyo.wav"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/charge_hit.wav"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/step.wav"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/boss_change_mode.wav"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/boss_sharp_hit.wav"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/boss_riser.wav"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/battle_explosion.wav"
    VERBATIM
)
add_dependencies(BiggerFISTsSDL CombatSoundAssets)

set(ADDITIONAL_COMBAT_SOUNDS
    Engine.wav Sp2.wav Sp3.wav activate.wav dash.wav direction_set.wav
    explosion.wav object-falling.wav overload_shutdown.wav sharp_whoosh.wav)
add_custom_target(AdditionalCombatSoundAssets
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/sound"
    VERBATIM)
foreach(sound IN LISTS ADDITIONAL_COMBAT_SOUNDS)
    add_custom_command(TARGET AdditionalCombatSoundAssets POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/${sound}"
            "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/sound/${sound}"
        VERBATIM)
endforeach()
add_dependencies(BiggerFISTsSDL AdditionalCombatSoundAssets)

add_custom_target(FullGameAssets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/ui"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/ui"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/story"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/story"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/BGM"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/sound/BGM"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound/Tutorial"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/sound/Tutorial"
    VERBATIM
)
add_dependencies(BiggerFISTsSDL FullGameAssets)

add_executable(InputActionsTests EXCLUDE_FROM_ALL
    tests/InputActionsTests.cpp
    src/Input/Input.cpp
    src/Input/InputActions.cpp
    src/Input/InputBindings.cpp
)
target_include_directories(InputActionsTests PRIVATE src)
target_link_libraries(InputActionsTests PRIVATE SDL3::SDL3-shared)

enable_testing()
add_test(NAME InputActionsTests COMMAND InputActionsTests)
set_tests_properties(InputActionsTests PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy"
)

add_executable(WindowSettingsTests EXCLUDE_FROM_ALL
    tests/WindowSettingsTests.cpp
    src/Platform/WindowSettings.cpp
    src/Platform/WindowSettings.h
)
target_include_directories(WindowSettingsTests PRIVATE src)
target_link_libraries(WindowSettingsTests PRIVATE SDL3::SDL3-shared)
add_test(NAME WindowSettingsTests COMMAND WindowSettingsTests)
set_tests_properties(WindowSettingsTests PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy"
)

add_executable(MathTypesTests EXCLUDE_FROM_ALL
    tests/MathTypesTests.cpp
    src/Math/MathTypes.cpp
    src/Math/MathTypes.h
)
target_include_directories(MathTypesTests PRIVATE src)
add_test(NAME MathTypesTests COMMAND MathTypesTests)

add_executable(GameTests EXCLUDE_FROM_ALL
    tests/GameTests.cpp
    src/Game/Game.cpp
    src/World/ModelScene.cpp
    src/Math/MathTypes.cpp
)
target_include_directories(GameTests PRIVATE src)
add_test(NAME GameTests COMMAND GameTests)

add_executable(FirstPersonGameTests EXCLUDE_FROM_ALL
    tests/FirstPersonGameTests.cpp
    src/Game/FirstPersonGame.cpp
    src/Collision/Collision.cpp
    src/Gameplay/Player/PlayerMovement.cpp
    src/Gameplay/Player/PunchArm.cpp
    src/Rendering/FirstPersonScenePresenter.cpp
    src/World/FirstPersonCamera.cpp
    src/World/ModelScene.cpp
    src/Math/MathTypes.cpp
    ${PLAYER_COMBAT_SOURCES}
)
target_include_directories(FirstPersonGameTests PRIVATE src)
add_test(NAME FirstPersonGameTests COMMAND FirstPersonGameTests)

add_executable(CombatFeedbackTests EXCLUDE_FROM_ALL
    tests/CombatFeedbackTests.cpp
    src/Data/CombatFeedbackConfig.cpp
    src/Data/CombatFeedbackConfig.h
    src/Platform/CombatFeedback.cpp
    src/Platform/CombatFeedback.h
)
target_include_directories(CombatFeedbackTests PRIVATE src)
target_link_libraries(CombatFeedbackTests PRIVATE SDL3::SDL3-shared)
add_test(NAME CombatFeedbackTests
    COMMAND CombatFeedbackTests "${CMAKE_CURRENT_SOURCE_DIR}/assets/sound"
        "${CMAKE_CURRENT_SOURCE_DIR}/config/combat-feedback.cfg")
set_tests_properties(CombatFeedbackTests PROPERTIES
    ENVIRONMENT "SDL_AUDIODRIVER=dummy"
)

add_executable(RocketPunchTests EXCLUDE_FROM_ALL
    tests/RocketPunchTests.cpp
    src/Game/FirstPersonGame.cpp
    src/Collision/Collision.cpp
    src/Gameplay/Player/PlayerMovement.cpp
    src/Gameplay/Player/PunchArm.cpp
    src/Rendering/FirstPersonScenePresenter.cpp
    src/World/FirstPersonCamera.cpp
    src/World/ModelScene.cpp
    src/Math/MathTypes.cpp
    ${PLAYER_COMBAT_SOURCES}
)
target_include_directories(RocketPunchTests PRIVATE src)
add_test(NAME RocketPunchTests COMMAND RocketPunchTests)

add_executable(PlayerCombatTests EXCLUDE_FROM_ALL
    tests/PlayerCombatTests.cpp
    src/Data/PlayerCombatConfig.cpp
    src/Game/FirstPersonGame.cpp
    src/Collision/Collision.cpp
    src/Gameplay/Player/PlayerMovement.cpp
    src/Gameplay/Player/PunchArm.cpp
    src/Rendering/FirstPersonScenePresenter.cpp
    src/World/FirstPersonCamera.cpp
    src/World/ModelScene.cpp
    src/Math/MathTypes.cpp
    ${PLAYER_COMBAT_SOURCES}
)
target_include_directories(PlayerCombatTests PRIVATE src)
add_test(NAME PlayerCombatTests COMMAND PlayerCombatTests
    "${CMAKE_CURRENT_SOURCE_DIR}/config/player-combat.cfg")

add_executable(FixedStepTests EXCLUDE_FROM_ALL
    tests/FixedStepTests.cpp
    src/Platform/FixedStepClock.cpp
    src/Input/FixedStepInputBuffer.cpp
    src/Game/FirstPersonGame.cpp
    src/Collision/Collision.cpp
    src/Gameplay/Player/PlayerMovement.cpp
    src/Gameplay/Player/PunchArm.cpp
    src/World/FirstPersonCamera.cpp
    src/Math/MathTypes.cpp
    ${PLAYER_COMBAT_SOURCES}
)
target_include_directories(FixedStepTests PRIVATE src)
add_test(NAME FixedStepTests COMMAND FixedStepTests)

add_executable(CollisionTests EXCLUDE_FROM_ALL
    tests/CollisionTests.cpp
    src/Collision/Collision.cpp
    src/Collision/Collision.h
)
target_include_directories(CollisionTests PRIVATE src)
add_test(NAME CollisionTests COMMAND CollisionTests)

add_executable(ObjLoaderTests EXCLUDE_FROM_ALL
    tests/ObjLoaderTests.cpp
    src/Data/ObjLoader.cpp
)
target_include_directories(ObjLoaderTests PRIVATE src)
target_compile_definitions(ObjLoaderTests PRIVATE
    OBJ_TEST_MODEL_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/models/Target/Target.obj"
    OBJ_TEST_FIST_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LFist/Lfist.obj"
    OBJ_TEST_GAME_FIST_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LfistTEST/LfistTEST.obj"
    OBJ_TEST_RIGHT_FIST_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/models/RFist/Rfist.obj"
    OBJ_TEST_MISSILE_UNIT_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/models/RocketLaunch/RocketLaunch.obj"
    OBJ_TEST_TUTORIAL_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/models/tutorial/tutorial.obj"
)
add_test(NAME ObjLoaderTests COMMAND ObjLoaderTests)
set_tests_properties(ObjLoaderTests PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:ObjLoaderTests>")

add_executable(ModelConfigTests EXCLUDE_FROM_ALL
    tests/ModelConfigTests.cpp
    src/Data/ModelConfig.cpp
)
target_include_directories(ModelConfigTests PRIVATE src)
add_test(NAME ModelConfigTests COMMAND ModelConfigTests)
set_tests_properties(ModelConfigTests PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:ModelConfigTests>")

add_executable(SceneConfigTests EXCLUDE_FROM_ALL
    tests/SceneConfigTests.cpp
    src/Data/SceneConfig.cpp
)
target_include_directories(SceneConfigTests PRIVATE src)
add_test(NAME SceneConfigTests COMMAND SceneConfigTests)
set_tests_properties(SceneConfigTests PROPERTIES
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:SceneConfigTests>")

add_executable(SceneLoaderTests EXCLUDE_FROM_ALL
    tests/SceneLoaderTests.cpp
    src/Data/SceneLoader.cpp
    src/Data/ObjLoader.cpp
    src/World/ModelScene.cpp
    src/Math/MathTypes.cpp
)
target_include_directories(SceneLoaderTests PRIVATE src)
target_compile_definitions(SceneLoaderTests PRIVATE
    SCENE_TEST_MODEL_DIRECTORY="${CMAKE_CURRENT_SOURCE_DIR}/assets/models"
)
add_test(NAME SceneLoaderTests COMMAND SceneLoaderTests)

add_executable(BattleAssetTests EXCLUDE_FROM_ALL
    tests/BattleAssetTests.cpp
    src/Data/BattleAssetCatalog.cpp
    src/Data/SceneConfig.cpp
    src/Data/SceneLoader.cpp
    src/Data/ObjLoader.cpp
    src/World/ModelScene.cpp
    src/Math/MathTypes.cpp
)
target_include_directories(BattleAssetTests PRIVATE src)
add_test(NAME BattleAssetTests COMMAND BattleAssetTests
    "${CMAKE_CURRENT_SOURCE_DIR}/config"
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/models")

add_executable(EncounterConfigTests EXCLUDE_FROM_ALL
    tests/EncounterConfigTests.cpp
    src/Gameplay/Combat/EncounterConfig.cpp
)
target_include_directories(EncounterConfigTests PRIVATE src)
add_test(NAME EncounterConfigTests COMMAND EncounterConfigTests
    "${CMAKE_CURRENT_SOURCE_DIR}/config")

add_executable(EncounterDirectorTests EXCLUDE_FROM_ALL
    tests/EncounterDirectorTests.cpp
    src/Gameplay/Combat/EncounterConfig.cpp
    src/Gameplay/Combat/EncounterDirector.cpp
)
target_include_directories(EncounterDirectorTests PRIVATE src)
add_test(NAME EncounterDirectorTests COMMAND EncounterDirectorTests
    "${CMAKE_CURRENT_SOURCE_DIR}/config")

add_executable(BattleBossTests EXCLUDE_FROM_ALL
    tests/BattleBossTests.cpp
    src/Data/BattleCombatConfig.cpp
    src/Gameplay/Combat/BattleBoss.cpp
    src/Collision/Collision.cpp
)
target_include_directories(BattleBossTests PRIVATE src)
add_test(NAME BattleBossTests COMMAND BattleBossTests
    "${CMAKE_CURRENT_SOURCE_DIR}/config")

add_executable(CombatSessionTests EXCLUDE_FROM_ALL
    tests/CombatSessionTests.cpp
    src/Data/BattleCombatConfig.cpp
    src/Gameplay/Combat/BattleBoss.cpp
    src/Gameplay/Combat/CombatSession.cpp
    src/Gameplay/Combat/EncounterConfig.cpp
    src/Gameplay/Combat/EncounterDirector.cpp
    src/Game/FirstPersonGame.cpp
    src/Gameplay/Player/PlayerMovement.cpp
    src/Gameplay/Player/PlayerVitality.cpp
    src/Gameplay/Player/PunchArm.cpp
    src/Gameplay/Combat/CombatHitResolver.cpp
    src/Collision/Collision.cpp
    src/World/FirstPersonCamera.cpp
    src/Math/MathTypes.cpp
    src/Input/FixedStepInputBuffer.cpp
    src/Platform/FixedStepClock.cpp
)
target_include_directories(CombatSessionTests PRIVATE src)
add_test(NAME CombatSessionTests COMMAND CombatSessionTests
    "${CMAKE_CURRENT_SOURCE_DIR}/config")

add_executable(BattleReplayTests EXCLUDE_FROM_ALL
    tests/BattleReplayTests.cpp
    src/Data/BattleCombatConfig.cpp
    src/Gameplay/Combat/BattleBoss.cpp
    src/Gameplay/Combat/CombatSession.cpp
    src/Gameplay/Combat/EncounterConfig.cpp
    src/Gameplay/Combat/EncounterDirector.cpp
    src/Game/FirstPersonGame.cpp
    src/Gameplay/Player/PlayerMovement.cpp
    src/Gameplay/Player/PlayerVitality.cpp
    src/Gameplay/Player/PunchArm.cpp
    src/Gameplay/Combat/CombatHitResolver.cpp
    src/Collision/Collision.cpp
    src/World/FirstPersonCamera.cpp
    src/Math/MathTypes.cpp
    src/Input/FixedStepInputBuffer.cpp
    src/Platform/FixedStepClock.cpp
    src/Data/BattleAssetCatalog.cpp
    src/Data/ObjLoader.cpp
    src/World/ModelScene.cpp
    src/Rendering/FirstPersonScenePresenter.cpp
    src/Rendering/BattleScenePresenter.cpp
)
target_include_directories(BattleReplayTests PRIVATE src)
add_test(NAME BattleReplayTests COMMAND BattleReplayTests
    "${CMAKE_CURRENT_SOURCE_DIR}/config")

add_executable(GameFlowTests EXCLUDE_FROM_ALL
    tests/GameFlowTests.cpp
    src/Game/GameFlow.cpp
    src/Data/StoryConfig.cpp
    src/Platform/FixedStepClock.cpp
)
target_include_directories(GameFlowTests PRIVATE src)
add_test(NAME GameFlowTests COMMAND GameFlowTests
    "${CMAKE_CURRENT_SOURCE_DIR}/config")

add_executable(TutorialSessionTests EXCLUDE_FROM_ALL
    tests/TutorialSessionTests.cpp
    src/Gameplay/Tutorial/TutorialSession.cpp
)
target_include_directories(TutorialSessionTests PRIVATE src)
add_test(NAME TutorialSessionTests COMMAND TutorialSessionTests)

add_executable(CockpitPresentationTests EXCLUDE_FROM_ALL
    tests/CockpitPresentationTests.cpp
    src/Gameplay/Cockpit/CockpitPresentation.cpp
)
target_include_directories(CockpitPresentationTests PRIVATE src)
add_test(NAME CockpitPresentationTests COMMAND CockpitPresentationTests)

add_executable(FullGameIntegrationTests EXCLUDE_FROM_ALL
    tests/FullGameIntegrationTests.cpp
    src/Game/GameFlow.cpp
    src/Data/StoryConfig.cpp
    src/Data/BattleCombatConfig.cpp
    src/Gameplay/Combat/BattleBoss.cpp
    src/Gameplay/Combat/CombatSession.cpp
    src/Gameplay/Combat/EncounterConfig.cpp
    src/Gameplay/Combat/EncounterDirector.cpp
    src/Game/FirstPersonGame.cpp
    src/Gameplay/Player/PlayerMovement.cpp
    src/Gameplay/Player/PlayerVitality.cpp
    src/Gameplay/Player/PunchArm.cpp
    src/Gameplay/Combat/CombatHitResolver.cpp
    src/Collision/Collision.cpp
    src/World/FirstPersonCamera.cpp
    src/Math/MathTypes.cpp
)
target_include_directories(FullGameIntegrationTests PRIVATE src)
add_test(NAME FullGameIntegrationTests COMMAND FullGameIntegrationTests
    "${CMAKE_CURRENT_SOURCE_DIR}/config")

if(MSVC)
    target_compile_options(BiggerFISTsSDL PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(InputActionsTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(WindowSettingsTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(MathTypesTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(GameTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(FirstPersonGameTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(CombatFeedbackTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(RocketPunchTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(PlayerCombatTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(FixedStepTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(CollisionTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(ObjLoaderTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(ModelConfigTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(SceneConfigTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(SceneLoaderTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(BattleAssetTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(EncounterConfigTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(EncounterDirectorTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(BattleBossTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(CombatSessionTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(BattleReplayTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(GameFlowTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(TutorialSessionTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(CockpitPresentationTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(FullGameIntegrationTests PRIVATE /W4 /utf-8 /EHsc)
endif()

add_custom_command(TARGET BiggerFISTsSDL POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:BiggerFISTsSDL>
)

add_custom_command(TARGET InputActionsTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:InputActionsTests>
)

add_custom_command(TARGET WindowSettingsTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:WindowSettingsTests>
)

add_custom_command(TARGET CombatFeedbackTests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:CombatFeedbackTests>
)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/config/gameplay.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/input.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/display.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/model.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/scene.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/first-person-scene.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/missile-unit-scene.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/combat-feedback.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/player-combat.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/battlefield-scene.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/battle-assets.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/day1-encounter.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/day2-encounter.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/enemy-combat.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/projectile-combat.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/boss-combat.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/gameplay.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/gameplay.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/input.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/input.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/display.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/display.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/model.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/model.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/scene.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/scene.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/first-person-scene.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/first-person-scene.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/missile-unit-scene.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/missile-unit-scene.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/combat-feedback.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/combat-feedback.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/player-combat.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/player-combat.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/battlefield-scene.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/battlefield-scene.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/battle-assets.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/battle-assets.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/day1-encounter.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/day1-encounter.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/day2-encounter.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/day2-encounter.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/enemy-combat.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/enemy-combat.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/projectile-combat.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/projectile-combat.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/boss-combat.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/boss-combat.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/story-pages.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/story-pages.cfg"
)

find_program(GPU_DXC_EXECUTABLE
    NAMES dxc
    HINTS "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64"
    REQUIRED
)

add_executable(GpuTriangle
    src/Samples/GpuTriangle.cpp
    src/Samples/SampleCubeMesh.cpp
    src/Samples/SampleCubeMesh.h
    src/Platform/WindowSettings.cpp
    src/Platform/WindowSettings.h
    ${SCENE_RENDERING_SOURCES}
)
target_include_directories(GpuTriangle PRIVATE src)

target_link_libraries(GpuTriangle
    PRIVATE SDL3::SDL3-shared
)

if(MSVC)
    target_compile_options(GpuTriangle
        PRIVATE /W4 /utf-8 /EHsc
    )
endif()

set(GPU_SHADER_SOURCE_DIR
    "${CMAKE_CURRENT_SOURCE_DIR}/shaders"
)

set(GPU_SHADER_OUTPUT_DIR
    "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/shaders"
)

add_custom_command(
    OUTPUT
        "${GPU_SHADER_OUTPUT_DIR}/triangle.vert.dxil"
        "${GPU_SHADER_OUTPUT_DIR}/triangle.frag.dxil"
        "${GPU_SHADER_OUTPUT_DIR}/ui.vert.dxil"
        "${GPU_SHADER_OUTPUT_DIR}/ui.frag.dxil"

    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${GPU_SHADER_OUTPUT_DIR}"

    COMMAND "${GPU_DXC_EXECUTABLE}"
        -T vs_6_0
        -E main
        -Fo "${GPU_SHADER_OUTPUT_DIR}/triangle.vert.dxil"
        "${GPU_SHADER_SOURCE_DIR}/triangle.vert.hlsl"

    COMMAND "${GPU_DXC_EXECUTABLE}"
        -T ps_6_0
        -E main
        -Fo "${GPU_SHADER_OUTPUT_DIR}/triangle.frag.dxil"
        "${GPU_SHADER_SOURCE_DIR}/triangle.frag.hlsl"

    COMMAND "${GPU_DXC_EXECUTABLE}"
        -T vs_6_0
        -E main
        -Fo "${GPU_SHADER_OUTPUT_DIR}/ui.vert.dxil"
        "${GPU_SHADER_SOURCE_DIR}/ui.vert.hlsl"

    COMMAND "${GPU_DXC_EXECUTABLE}"
        -T ps_6_0
        -E main
        -Fo "${GPU_SHADER_OUTPUT_DIR}/ui.frag.dxil"
        "${GPU_SHADER_SOURCE_DIR}/ui.frag.hlsl"

    DEPENDS
        "${GPU_SHADER_SOURCE_DIR}/triangle.vert.hlsl"
        "${GPU_SHADER_SOURCE_DIR}/triangle.frag.hlsl"
        "${GPU_SHADER_SOURCE_DIR}/ui.vert.hlsl"
        "${GPU_SHADER_SOURCE_DIR}/ui.frag.hlsl"

    VERBATIM
)

add_custom_target(CubeShaders
    DEPENDS
        "${GPU_SHADER_OUTPUT_DIR}/triangle.vert.dxil"
        "${GPU_SHADER_OUTPUT_DIR}/triangle.frag.dxil"
        "${GPU_SHADER_OUTPUT_DIR}/ui.vert.dxil"
        "${GPU_SHADER_OUTPUT_DIR}/ui.frag.dxil"
)

add_dependencies(GpuTriangle
    CubeShaders
)

add_dependencies(BiggerFISTsSDL
    CubeShaders
)

# Run even when only the PNG changed and no C++ relink is needed.
add_custom_target(CubeAssets
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/textures"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures/cube.png"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/textures/cube.png"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/textures/cube.png"
    VERBATIM
)
add_dependencies(GpuTriangle CubeAssets)

add_custom_target(ModelAssets
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/UpperArm"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/UpperArm/UpperArm.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/UpperArm/UpperArm.obj"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/UpperArm/arm.mtl"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/UpperArm/arm.mtl"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/UpperArm/ARM.png"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/UpperArm/ARM.png"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/Target"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/Target/Target.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/Target/Target.obj"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/Target/Target.mtl"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/Target/Target.mtl"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/Target/target.png"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/Target/target.png"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/LFist"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LFist/Lfist.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/LFist/Lfist.obj"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LFist/Lfist.mtl"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/LFist/Lfist.mtl"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LFist/Lfist.png"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/LFist/Lfist.png"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/LfistTEST"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LfistTEST/LfistTEST.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/LfistTEST/LfistTEST.obj"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LfistTEST/LfistTEST.mtl"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/LfistTEST/LfistTEST.mtl"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LfistTEST/Lfist.png"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/LfistTEST/Lfist.png"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/RFist"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/RFist/Rfist.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/RFist/Rfist.obj"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/RFist/Rfist.mtl"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/RFist/Rfist.mtl"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/RFist/Lfist.png"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/RFist/Lfist.png"
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/RocketLaunch"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/RocketLaunch/RocketLaunch.obj"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/RocketLaunch/RocketLaunch.obj"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/RocketLaunch/RocketLaunch.mtl"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/RocketLaunch/RocketLaunch.mtl"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/RocketLaunch/Launcher.png"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/RocketLaunch/Launcher.png"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/RocketLaunch/Launcherv2.png"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models/RocketLaunch/Launcherv2.png"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/UpperArm/UpperArm.obj"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/UpperArm/arm.mtl"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/UpperArm/ARM.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/Target/Target.obj"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/Target/Target.mtl"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/Target/target.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LFist/Lfist.obj"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LFist/Lfist.mtl"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LFist/Lfist.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LfistTEST/LfistTEST.obj"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LfistTEST/LfistTEST.mtl"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LfistTEST/Lfist.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/RFist/Rfist.obj"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/RFist/Rfist.mtl"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/RFist/Lfist.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/RocketLaunch/RocketLaunch.obj"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/RocketLaunch/RocketLaunch.mtl"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/RocketLaunch/Launcher.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/RocketLaunch/Launcherv2.png"
    VERBATIM
)
add_dependencies(BiggerFISTsSDL ModelAssets)

add_custom_target(BattleModelAssets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models"
        "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/assets/models"
    VERBATIM
)
add_dependencies(BiggerFISTsSDL BattleModelAssets)

add_executable(MultiModelPreview
    src/Samples/MultiModelPreview.cpp
    src/Platform/WindowSettings.cpp
    src/Platform/WindowSettings.h
    src/Platform/FrameTimer.cpp
    src/Platform/FrameTimer.h
    src/Data/ObjLoader.cpp
    src/Data/ObjLoader.h
    src/Data/MeshData.h
    ${SCENE_LOADING_SOURCES}
    ${SCENE_RENDERING_SOURCES}
)
target_include_directories(MultiModelPreview PRIVATE src)
target_link_libraries(MultiModelPreview PRIVATE SDL3::SDL3-shared)
if(MSVC)
    target_compile_options(MultiModelPreview PRIVATE /W4 /utf-8 /EHsc)
endif()
add_dependencies(MultiModelPreview CubeShaders ModelAssets)
add_custom_command(TARGET MultiModelPreview POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:MultiModelPreview>
)

add_custom_command(TARGET GpuTriangle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:GpuTriangle>
)
