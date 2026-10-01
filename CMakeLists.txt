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

add_executable(BiggerFISTsSDL src/main.cpp)
target_link_libraries(BiggerFISTsSDL PRIVATE SDL3::SDL3-shared)

if(MSVC)
    target_compile_options(BiggerFISTsSDL PRIVATE /W4 /utf-8)
endif()

add_custom_command(TARGET BiggerFISTsSDL POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL3-shared>
        $<TARGET_FILE_DIR:BiggerFISTsSDL>
)
