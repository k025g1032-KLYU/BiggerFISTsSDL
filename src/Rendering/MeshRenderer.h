#pragma once

#include "Rendering/DepthBuffer.h"
#include "Data/MeshData.h"
#include "Math/MathTypes.h"

#include <SDL3/SDL.h>

#include <memory>
#include <span>
#include <vector>

class WindowSettings;
struct ModelScene;

class MeshRenderer {
public:
    MeshRenderer() = default;
    ~MeshRenderer();

    MeshRenderer(const MeshRenderer&) = delete;
    MeshRenderer& operator=(const MeshRenderer&) = delete;

    bool Initialize(SDL_Window* window, const MeshData& mesh);
    bool Initialize(SDL_Window* window, std::span<const MeshData> meshes);
    bool Render(const ModelScene& scene, WindowSettings& windowSettings, bool* frameRendered = nullptr);
    void Shutdown();

private:
    struct GpuMesh {
        SDL_GPUBuffer* vertexBuffer = nullptr;
        SDL_GPUBuffer* indexBuffer = nullptr;
        Uint32 indexCount = 0;
        SDL_GPUTexture* texture = nullptr;
    };

    SDL_GPUDevice* device_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_ = nullptr;
    std::vector<GpuMesh> meshes_;
    SDL_GPUSampler* sampler_ = nullptr;
    std::unique_ptr<DepthBuffer> depthBuffer_;
    std::vector<Matrix4x4> transformScratch_;
};
