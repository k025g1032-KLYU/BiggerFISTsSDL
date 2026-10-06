#include "Rendering/MeshRenderer.h"

#include "Platform/WindowSettings.h"
#include "Rendering/DepthBuffer.h"
#include "Rendering/TextureLoader.h"
#include "World/ModelScene.h"

#include <cstddef>
#include <cstring>
#include <limits>
#include <string>
#include <type_traits>

namespace {
static_assert(std::is_standard_layout_v<MeshVertex>);
static_assert(sizeof(MeshVertex) == sizeof(float) * 5);

SDL_GPUBuffer* CreateUploadedBuffer(
    SDL_GPUDevice* device,
    const void* data,
    Uint32 dataSize,
    SDL_GPUBufferUsageFlags usage
) {
    SDL_GPUBufferCreateInfo bufferInfo{};
    bufferInfo.usage = usage;
    bufferInfo.size = dataSize;

    SDL_GPUBuffer* buffer = SDL_CreateGPUBuffer(device, &bufferInfo);
    if (buffer == nullptr) {
        SDL_Log("SDL_CreateGPUBuffer failed: %s", SDL_GetError());
        return nullptr;
    }

    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = dataSize;

    SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
    if (transferBuffer == nullptr) {
        SDL_Log("SDL_CreateGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUBuffer(device, buffer);
        return nullptr;
    }

    void* mappedData = SDL_MapGPUTransferBuffer(device, transferBuffer, false);
    if (mappedData == nullptr) {
        SDL_Log("SDL_MapGPUTransferBuffer failed: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return nullptr;
    }

    std::memcpy(mappedData, data, dataSize);
    SDL_UnmapGPUTransferBuffer(device, transferBuffer);

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (commands == nullptr) {
        SDL_Log("Could not acquire buffer upload commands: %s", SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return nullptr;
    }

    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commands);
    if (copyPass == nullptr) {
        SDL_Log("SDL_BeginGPUCopyPass failed: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(commands);
        SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
        SDL_ReleaseGPUBuffer(device, buffer);
        return nullptr;
    }

    SDL_GPUTransferBufferLocation source{};
    source.transfer_buffer = transferBuffer;
    source.offset = 0;

    SDL_GPUBufferRegion destination{};
    destination.buffer = buffer;
    destination.offset = 0;
    destination.size = dataSize;

    SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);
    SDL_EndGPUCopyPass(copyPass);

    const bool submitted = SDL_SubmitGPUCommandBuffer(commands);
    if (!submitted) {
        SDL_Log("Could not submit buffer upload commands: %s", SDL_GetError());
    }

    // SDL defers destruction until submitted GPU work no longer uses this buffer.
    SDL_ReleaseGPUTransferBuffer(device, transferBuffer);
    if (!submitted) {
        SDL_ReleaseGPUBuffer(device, buffer);
        return nullptr;
    }

    return buffer;
}

SDL_GPUBuffer* CreateVertexBuffer(SDL_GPUDevice* device, const MeshData& mesh) {
    const Uint32 dataSize = static_cast<Uint32>(mesh.vertices.size() * sizeof(MeshVertex));
    SDL_GPUBuffer* vertexBuffer = CreateUploadedBuffer(
        device,
        mesh.vertices.data(),
        dataSize,
        SDL_GPU_BUFFERUSAGE_VERTEX
    );
    if (vertexBuffer != nullptr) {
        SDL_Log("Vertex upload submitted: %u vertices, %u bytes",
            static_cast<Uint32>(mesh.vertices.size()), dataSize);
    }
    return vertexBuffer;
}

SDL_GPUBuffer* CreateIndexBuffer(SDL_GPUDevice* device, const MeshData& mesh) {
    const Uint32 dataSize = static_cast<Uint32>(mesh.indices.size() * sizeof(Uint32));
    SDL_GPUBuffer* indexBuffer = CreateUploadedBuffer(
        device,
        mesh.indices.data(),
        dataSize,
        SDL_GPU_BUFFERUSAGE_INDEX
    );
    if (indexBuffer != nullptr) {
        SDL_Log("Index upload submitted: %u indices, %u bytes (32-bit)",
            static_cast<Uint32>(mesh.indices.size()), dataSize);
    }
    return indexBuffer;
}

SDL_GPUShader* LoadShader(
    SDL_GPUDevice* device,
    const char* filename,
    SDL_GPUShaderStage stage,
    Uint32 uniformBufferCount,
    Uint32 samplerCount
) {
    const char* basePath = SDL_GetBasePath();
    if (basePath == nullptr) {
        SDL_Log("SDL_GetBasePath failed: %s", SDL_GetError());
        return nullptr;
    }

    const std::string path =
        std::string(basePath) + "shaders/" + filename;

    size_t codeSize = 0;
    void* code = SDL_LoadFile(path.c_str(), &codeSize);
    if (code == nullptr) {
        SDL_Log("Could not load %s: %s", path.c_str(), SDL_GetError());
        return nullptr;
    }

    SDL_GPUShaderCreateInfo info{};
    info.code = static_cast<const Uint8*>(code);
    info.code_size = codeSize;
    info.entrypoint = "main";
    info.format = SDL_GPU_SHADERFORMAT_DXIL;
    info.stage = stage;
    info.num_uniform_buffers = uniformBufferCount;
    info.num_samplers = samplerCount;

    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
    SDL_free(code);

    if (shader == nullptr) {
        SDL_Log(
            "SDL_CreateGPUShader failed for %s: %s",
            filename,
            SDL_GetError()
        );
    }

    return shader;
}

SDL_GPUGraphicsPipeline* CreatePipeline(
    SDL_GPUDevice* device,
    SDL_Window* window,
    SDL_GPUTextureFormat depthFormat
) {
    SDL_GPUShader* vertexShader = LoadShader(
        device,
        "triangle.vert.dxil",
        SDL_GPU_SHADERSTAGE_VERTEX,
        1,
        0
    );

    if (vertexShader == nullptr) {
        return nullptr;
    }

    SDL_GPUShader* fragmentShader = LoadShader(
        device,
        "triangle.frag.dxil",
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        0,
        1
    );

    if (fragmentShader == nullptr) {
        SDL_ReleaseGPUShader(device, vertexShader);
        return nullptr;
    }

    SDL_GPUColorTargetDescription colorTarget{};
    colorTarget.format =
        SDL_GetGPUSwapchainTextureFormat(device, window);

    SDL_GPUVertexBufferDescription vertexDescription{};
    vertexDescription.slot = 0;
    vertexDescription.pitch = static_cast<Uint32>(sizeof(MeshVertex));
    vertexDescription.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute attributes[2]{};
    attributes[0].location = 0;
    attributes[0].buffer_slot = 0;
    attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attributes[0].offset = static_cast<Uint32>(offsetof(MeshVertex, position));
    attributes[1].location = 1;
    attributes[1].buffer_slot = 0;
    attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attributes[1].offset = static_cast<Uint32>(offsetof(MeshVertex, uv));

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vertexShader;
    info.fragment_shader = fragmentShader;
    info.vertex_input_state.vertex_buffer_descriptions = &vertexDescription;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes;
    info.vertex_input_state.num_vertex_attributes = 2;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.rasterizer_state.enable_depth_clip = true;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.depth_stencil_state.enable_depth_test = true;
    info.depth_stencil_state.enable_depth_write = true;
    info.depth_stencil_state.compare_op = SDL_GPU_COMPAREOP_LESS;
    info.target_info.color_target_descriptions = &colorTarget;
    info.target_info.num_color_targets = 1;
    info.target_info.has_depth_stencil_target = true;
    info.target_info.depth_stencil_format = depthFormat;

    SDL_GPUGraphicsPipeline* pipeline =
        SDL_CreateGPUGraphicsPipeline(device, &info);

    SDL_ReleaseGPUShader(device, fragmentShader);
    SDL_ReleaseGPUShader(device, vertexShader);

    if (pipeline == nullptr) {
        SDL_Log(
            "SDL_CreateGPUGraphicsPipeline failed: %s",
            SDL_GetError()
        );
    }

    return pipeline;
}

}

MeshRenderer::~MeshRenderer() {
    Shutdown();
}

bool MeshRenderer::Initialize(SDL_Window* window, const MeshData& mesh) {
    return Initialize(window, std::span<const MeshData>(&mesh, 1));
}

bool MeshRenderer::Initialize(SDL_Window* window, std::span<const MeshData> meshes) {
    if (device_ != nullptr || window == nullptr || meshes.empty()) {
        return false;
    }
    for (const MeshData& mesh : meshes) {
        if (mesh.vertices.empty() || mesh.indices.empty() || mesh.texturePath.empty() ||
            mesh.vertices.size() > std::numeric_limits<Uint32>::max() / sizeof(MeshVertex) ||
            mesh.indices.size() > std::numeric_limits<Uint32>::max() / sizeof(Uint32)) {
            SDL_Log("Mesh has no vertices, indices or texture, or is too large for GPU buffers");
            return false;
        }
        for (const Uint32 index : mesh.indices) {
            if (index >= mesh.vertices.size()) {
                SDL_Log("Mesh index %u is outside the vertex buffer", index);
                return false;
            }
        }
    }

    device_ = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_DXIL, true, "direct3d12");
    if (device_ == nullptr) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        return false;
    }
    if (!SDL_ClaimWindowForGPUDevice(device_, window)) {
        SDL_Log("SDL_ClaimWindowForGPUDevice failed: %s", SDL_GetError());
        Shutdown();
        return false;
    }
    window_ = window;

    const SDL_GPUTextureFormat depthFormat = SelectDepthFormat(device_);
    if (depthFormat == SDL_GPU_TEXTUREFORMAT_INVALID) {
        Shutdown();
        return false;
    }
    pipeline_ = CreatePipeline(device_, window_, depthFormat);
    if (pipeline_ == nullptr) {
        Shutdown();
        return false;
    }
    SDL_GPUSamplerCreateInfo samplerInfo{};
    samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler_ = SDL_CreateGPUSampler(device_, &samplerInfo);
    if (sampler_ == nullptr) {
        SDL_Log("SDL_CreateGPUSampler failed: %s", SDL_GetError());
        Shutdown();
        return false;
    }

    for (const MeshData& mesh : meshes) {
        meshes_.emplace_back();
        GpuMesh& gpuMesh = meshes_.back();
        gpuMesh.vertexBuffer = CreateVertexBuffer(device_, mesh);
        if (gpuMesh.vertexBuffer == nullptr) {
            Shutdown();
            return false;
        }
        gpuMesh.indexBuffer = CreateIndexBuffer(device_, mesh);
        if (gpuMesh.indexBuffer == nullptr) {
            Shutdown();
            return false;
        }
        gpuMesh.indexCount = static_cast<Uint32>(mesh.indices.size());
        const auto utf8Path = mesh.texturePath.u8string();
        const std::string texturePath(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());
        gpuMesh.texture = LoadPngTexture(device_, texturePath.c_str());
        if (gpuMesh.texture == nullptr) {
            Shutdown();
            return false;
        }
    }
    depthBuffer_ = std::make_unique<DepthBuffer>(device_, depthFormat);
    SDL_Log("MeshRenderer initialized with %u mesh resources", static_cast<Uint32>(meshes_.size()));
    return true;
}

bool MeshRenderer::Render(const ModelScene& scene, WindowSettings& windowSettings, bool* frameRendered) {
    if (frameRendered != nullptr) {
        *frameRendered = false;
    }
    if (device_ == nullptr || window_ == nullptr || depthBuffer_ == nullptr) {
        SDL_Log("MeshRenderer is not initialized");
        return false;
    }
    if (scene.models.empty()) {
        SDL_Log("Scene has no models to draw");
        return false;
    }
    for (const ModelInstance& model : scene.models) {
        if (model.meshIndex >= meshes_.size()) {
            SDL_Log("Scene model index %u has no GPU mesh", static_cast<Uint32>(model.meshIndex));
            return false;
        }
    }

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device_);
    if (commands == nullptr) {
        SDL_Log("SDL_AcquireGPUCommandBuffer failed: %s", SDL_GetError());
        return false;
    }

    SDL_GPUTexture* backbuffer = nullptr;
    Uint32 outputWidth = 0;
    Uint32 outputHeight = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            commands, window_, &backbuffer, &outputWidth, &outputHeight)) {
        SDL_Log("SDL_WaitAndAcquireGPUSwapchainTexture failed: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(commands);
        return false;
    }
    if (backbuffer == nullptr) {
        if (!SDL_CancelGPUCommandBuffer(commands)) {
            SDL_Log("SDL_CancelGPUCommandBuffer failed: %s", SDL_GetError());
            return false;
        }
        SDL_Delay(10);
        return true;
    }

    windowSettings.UpdateOutputSize(outputWidth, outputHeight);
    if (!depthBuffer_->Resize(outputWidth, outputHeight)) {
        SDL_Log("Could not prepare the mesh depth buffer");
        // Acquired swapchain textures must be submitted, even on this error path.
        SDL_SubmitGPUCommandBuffer(commands);
        return false;
    }
    transformScratch_.resize(scene.models.size());
    for (std::size_t i = 0; i < scene.models.size(); ++i) {
        if (!TryBuildModelTransform(scene, scene.models[i], outputWidth, outputHeight,
                transformScratch_[i])) {
            SDL_Log("Could not build transform for scene model %u", static_cast<Uint32>(i));
            SDL_SubmitGPUCommandBuffer(commands);
            return false;
        }
    }

    SDL_GPUColorTargetInfo colorTarget{};
    colorTarget.texture = backbuffer;
    colorTarget.clear_color = scene.warmBackground
        ? SDL_FColor{ 180.0f / 255.0f, 80.0f / 255.0f, 40.0f / 255.0f, 1.0f }
        : SDL_FColor{ 0.05f, 0.08f, 0.12f, 1.0f };
    colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    colorTarget.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPUDepthStencilTargetInfo depthTarget{};
    depthTarget.texture = depthBuffer_->GetTexture();
    depthTarget.clear_depth = 1.0f;
    depthTarget.load_op = SDL_GPU_LOADOP_CLEAR;
    depthTarget.store_op = SDL_GPU_STOREOP_DONT_CARE;
    depthTarget.stencil_load_op = SDL_GPU_LOADOP_DONT_CARE;
    depthTarget.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
    depthTarget.cycle = true;

    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &colorTarget, 1, &depthTarget);
    if (pass == nullptr) {
        SDL_Log("SDL_BeginGPURenderPass failed: %s", SDL_GetError());
        SDL_SubmitGPUCommandBuffer(commands);
        return false;
    }

    SDL_BindGPUGraphicsPipeline(pass, pipeline_);
    for (std::size_t i = 0; i < scene.models.size(); ++i) {
        const GpuMesh& mesh = meshes_[scene.models[i].meshIndex];
        SDL_PushGPUVertexUniformData(commands, 0, &transformScratch_[i],
            static_cast<Uint32>(sizeof(Matrix4x4)));
        SDL_GPUBufferBinding vertexBinding{};
        vertexBinding.buffer = mesh.vertexBuffer;
        SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);

        SDL_GPUBufferBinding indexBinding{};
        indexBinding.buffer = mesh.indexBuffer;
        SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_32BIT);
        SDL_GPUTextureSamplerBinding textureBinding{};
        textureBinding.texture = mesh.texture;
        textureBinding.sampler = sampler_;
        SDL_BindGPUFragmentSamplers(pass, 0, &textureBinding, 1);
        SDL_DrawGPUIndexedPrimitives(pass, mesh.indexCount, 1, 0, 0, 0);
    }
    SDL_EndGPURenderPass(pass);

    if (!SDL_SubmitGPUCommandBuffer(commands)) {
        SDL_Log("SDL_SubmitGPUCommandBuffer failed: %s", SDL_GetError());
        return false;
    }
    if (frameRendered != nullptr) {
        *frameRendered = true;
    }
    return true;
}

void MeshRenderer::Shutdown() {
    if (device_ == nullptr) {
        return;
    }
    if (!SDL_WaitForGPUIdle(device_)) {
        SDL_Log("SDL_WaitForGPUIdle failed: %s", SDL_GetError());
    }
    depthBuffer_.reset();
    if (sampler_ != nullptr) {
        SDL_ReleaseGPUSampler(device_, sampler_);
        sampler_ = nullptr;
    }
    for (GpuMesh& mesh : meshes_) {
        if (mesh.texture != nullptr) {
            SDL_ReleaseGPUTexture(device_, mesh.texture);
        }
        if (mesh.indexBuffer != nullptr) {
            SDL_ReleaseGPUBuffer(device_, mesh.indexBuffer);
        }
        if (mesh.vertexBuffer != nullptr) {
            SDL_ReleaseGPUBuffer(device_, mesh.vertexBuffer);
        }
    }
    meshes_.clear();
    transformScratch_.clear();
    if (pipeline_ != nullptr) {
        SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
        pipeline_ = nullptr;
    }
    if (window_ != nullptr) {
        SDL_ReleaseWindowFromGPUDevice(device_, window_);
        window_ = nullptr;
    }
    SDL_DestroyGPUDevice(device_);
    device_ = nullptr;
}
