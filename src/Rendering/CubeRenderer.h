#pragma once

#include "Rendering/DepthBuffer.h"

#include <SDL3/SDL.h>

#include <memory>

class WindowSettings;
struct CubeScene;

class CubeRenderer {
public:
    CubeRenderer() = default;
    ~CubeRenderer();

    CubeRenderer(const CubeRenderer&) = delete;
    CubeRenderer& operator=(const CubeRenderer&) = delete;

    bool Initialize(SDL_Window* window);
    bool Render(const CubeScene& scene, WindowSettings& windowSettings, bool* frameRendered = nullptr);
    void Shutdown();

private:
    SDL_GPUDevice* device_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_ = nullptr;
    SDL_GPUBuffer* vertexBuffer_ = nullptr;
    SDL_GPUBuffer* indexBuffer_ = nullptr;
    std::unique_ptr<DepthBuffer> depthBuffer_;
};
