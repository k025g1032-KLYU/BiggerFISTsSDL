cmake_minimum_required(VERSION 3.21)

project(BiggerFISTsSDL LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(SDL_SHARED ON CACHE BOOL "Build SDL3 as a shared library" FORCE)
set(SDL_STATIC OFF CACHE BOOL "Do not build SDL3 as a static library" FORCE)
set(SDL_TESTS OFF CACHE BOOL "Do not build SDL3 tests" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "Do not build SDL3 examples" FORCE)

add_subdirectory(vendor/SDL EXCLUDE_FROM_ALL)

add_executable(BiggerFISTsSDL
    src/main.cpp
    src/Application.cpp
    src/Application.h
    src/FrameTimer.cpp
    src/FrameTimer.h
    src/Input.cpp
    src/Input.h
    src/InputActions.cpp
    src/InputActions.h
    src/InputBindings.cpp
    src/InputBindings.h
    src/GameInput.h
    src/GameConfig.cpp
    src/GameConfig.h
    src/Game.cpp
    src/Game.h
)
target_link_libraries(BiggerFISTsSDL PRIVATE SDL3::SDL3-shared)

add_executable(InputActionsTests EXCLUDE_FROM_ALL
    tests/InputActionsTests.cpp
    src/Input.cpp
    src/InputActions.cpp
    src/InputBindings.cpp
)
target_include_directories(InputActionsTests PRIVATE src)
target_link_libraries(InputActionsTests PRIVATE SDL3::SDL3-shared)

enable_testing()
add_test(NAME InputActionsTests COMMAND InputActionsTests)
set_tests_properties(InputActionsTests PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=dummy"
)

if(MSVC)
    target_compile_options(BiggerFISTsSDL PRIVATE /W4 /utf-8 /EHsc)
    target_compile_options(InputActionsTests PRIVATE /W4 /utf-8 /EHsc)
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

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/config/gameplay.cfg"
    "${CMAKE_CURRENT_SOURCE_DIR}/config/input.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/gameplay.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/gameplay.cfg"
)

file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/input.cfg"
    INPUT "${CMAKE_CURRENT_SOURCE_DIR}/config/input.cfg"
)

find_program(GPU_DXC_EXECUTABLE
    NAMES dxc
    HINTS "C:/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64"
    REQUIRED
)

add_executable(GpuTriangle
    src/GpuTriangle.cpp
)

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

add_custom_target(GpuTriangleShaders
    DEPENDS
        "${GPU_SHADER_OUTPUT_DIR}/triangle.vert.dxil"
        "${GPU_SHADER_OUTPUT_DIR}/triangle.frag.dxil"
)

add_dependencies(GpuTriangle
    GpuTriangleShaders
)

add_custom_command(TARGET GpuTriangle POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:GpuTriangle>
)
