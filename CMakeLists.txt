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
    src/Rendering/TextureLoader.cpp
    src/Rendering/TextureLoader.h
)

set(SCENE_LOADING_SOURCES
    src/Data/SceneConfig.cpp
    src/Data/SceneConfig.h
    src/Data/SceneLoader.cpp
    src/Data/SceneLoader.h
)

add_executable(BiggerFISTsSDL
    src/App/main.cpp
    src/App/Application.cpp
    src/App/Application.h
    src/Platform/WindowSettings.cpp
    src/Platform/WindowSettings.h
    src/Platform/FrameTimer.cpp
    src/Platform/FrameTimer.h
    src/Input/Input.cpp
    src/Input/Input.h
    src/Input/InputActions.cpp
    src/Input/InputActions.h
    src/Input/InputBindings.cpp
    src/Input/InputBindings.h
    src/Input/GameInput.h
    src/Data/GameConfig.cpp
    src/Data/GameConfig.h
    src/Data/ObjLoader.cpp
    src/Data/ObjLoader.h
    src/Data/MeshData.h
    src/Data/ModelConfig.cpp
    src/Data/ModelConfig.h
    src/Game/Game.cpp
    src/Game/Game.h
    ${SCENE_LOADING_SOURCES}
    ${SCENE_RENDERING_SOURCES}
)
target_include_directories(BiggerFISTsSDL PRIVATE src)
target_link_libraries(BiggerFISTsSDL PRIVATE SDL3::SDL3-shared)

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

add_executable(ObjLoaderTests EXCLUDE_FROM_ALL
    tests/ObjLoaderTests.cpp
    src/Data/ObjLoader.cpp
)
target_include_directories(ObjLoaderTests PRIVATE src)
target_compile_definitions(ObjLoaderTests PRIVATE
    OBJ_TEST_MODEL_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/models/Target/Target.obj"
    OBJ_TEST_FIST_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LFist/Lfist.obj"
    OBJ_TEST_GAME_FIST_PATH="${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LfistTEST/LfistTEST.obj"
)
add_test(NAME ObjLoaderTests COMMAND ObjLoaderTests)

add_executable(ModelConfigTests EXCLUDE_FROM_ALL
    tests/ModelConfigTests.cpp
    src/Data/ModelConfig.cpp
)
target_include_directories(ModelConfigTests PRIVATE src)
add_test(NAME ModelConfigTests COMMAND ModelConfigTests)

add_executable(SceneConfigTests EXCLUDE_FROM_ALL
    tests/SceneConfigTests.cpp
    src/Data/SceneConfig.cpp
)
target_include_directories(SceneConfigTests PRIVATE src)
add_test(NAME SceneConfigTests COMMAND SceneConfigTests)

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

if(MSVC)
    target_compile_options(BiggerFISTsSDL PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(InputActionsTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(WindowSettingsTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(MathTypesTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(GameTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(ObjLoaderTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(ModelConfigTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(SceneConfigTests PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(SceneLoaderTests PRIVATE /W4 /utf-8 /EHsc)
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

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/config/gameplay.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/input.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/display.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/model.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/scene.cfg"
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

    DEPENDS
        "${GPU_SHADER_SOURCE_DIR}/triangle.vert.hlsl"
        "${GPU_SHADER_SOURCE_DIR}/triangle.frag.hlsl"

    VERBATIM
)

add_custom_target(CubeShaders
    DEPENDS
        "${GPU_SHADER_OUTPUT_DIR}/triangle.vert.dxil"
        "${GPU_SHADER_OUTPUT_DIR}/triangle.frag.dxil"
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
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/Target/Target.obj"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/Target/Target.mtl"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/Target/target.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LFist/Lfist.obj"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LFist/Lfist.mtl"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LFist/Lfist.png"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LfistTEST/LfistTEST.obj"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LfistTEST/LfistTEST.mtl"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/models/LfistTEST/Lfist.png"
    VERBATIM
)
add_dependencies(BiggerFISTsSDL ModelAssets)

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
