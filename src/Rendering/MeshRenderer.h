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
class UiRenderer;
class UiDrawList;

class MeshRenderer {
public:
    MeshRenderer() = default;
    ~MeshRenderer();

    MeshRenderer(const MeshRenderer&) = delete;
    MeshRenderer& operator=(const MeshRenderer&) = delete;

    bool Initialize(SDL_Window* window, const MeshData& mesh);
    bool Initialize(SDL_Window* window, std::span<const MeshData> meshes);
    bool Render(const ModelScene& scene, WindowSettings& windowSettings,
        bool* frameRendered = nullptr, UiRenderer* uiRenderer = nullptr,
        const UiDrawList* uiDrawList = nullptr,
        const char* capturePath = nullptr);
    SDL_GPUDevice* GetDevice() const { return device_; }
    void Shutdown();

private:
    struct ModelVertexUniforms {
        Matrix4x4 transform;
        Matrix4x4 world;
        Matrix4x4 normalTransform;
    };
    struct GpuDrawRange {
        Uint32 firstIndex = 0;
        Uint32 indexCount = 0;
        std::size_t textureIndex = 0;
        MeshMaterial material{};
    };
    struct GpuMesh {
        SDL_GPUBuffer* vertexBuffer = nullptr;
        SDL_GPUBuffer* indexBuffer = nullptr;
        std::vector<GpuDrawRange> drawRanges;
        std::vector<SDL_GPUTexture*> textures;
    };

    SDL_GPUDevice* device_ = nullptr;
    SDL_Window* window_ = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_ = nullptr;
    SDL_GPUGraphicsPipeline* transparentPipeline_ = nullptr;
    SDL_GPUGraphicsPipeline* additivePipeline_ = nullptr;
    SDL_GPUGraphicsPipeline* firstPersonPipeline_ = nullptr;
    std::vector<GpuMesh> meshes_;
    SDL_GPUSampler* sampler_ = nullptr;
    bool encodeSrgbOutput_ = false;
    std::unique_ptr<DepthBuffer> depthBuffer_;
    std::vector<ModelVertexUniforms> transformScratch_;
};
