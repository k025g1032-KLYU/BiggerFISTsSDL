#include "MathTypes.h"
#include "WindowSettings.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstddef>
#include <cmath>
#include <cstring>
#include <numbers>
#include <string>
#include <type_traits>

struct Vertex {
    float position[3];
    float color[3];
};

static_assert(std::is_standard_layout_v<Vertex>);
static_assert(sizeof(Vertex) == sizeof(float) * 6);

struct QuadTransformSettings {
    float positionX = 0.0f;
    float positionY = 0.0f;
    float scale = 0.65f;
    float initialAngleDegrees = 0.0f;
    float rotationSpeedDegreesPerSecond = 45.0f;
};

constexpr QuadTransformSettings kQuadTransformSettings{};

Matrix4x4 BuildQuadTransform(double elapsedSeconds, Uint32 outputWidth, Uint32 outputHeight) {
    if (outputWidth == 0 || outputHeight == 0) {
        return MakeIdentityMatrix();
    }

    // Wrap the angle before converting to float to preserve precision over time.
    const double angleDegrees = std::fmod(
        kQuadTransformSettings.initialAngleDegrees +
            elapsedSeconds * kQuadTransformSettings.rotationSpeedDegreesPerSecond,
        360.0
    );
    const float angleRadians = static_cast<float>(angleDegrees * std::numbers::pi / 180.0);
    const Matrix4x4 scale = MakeScaleMatrix(
        kQuadTransformSettings.scale, kQuadTransformSettings.scale, 1.0f
    );
    const Matrix4x4 rotation = MakeRotationZMatrix(angleRadians);
    const Matrix4x4 translation = MakeTranslationMatrix(
        kQuadTransformSettings.positionX, kQuadTransformSettings.positionY, 0.0f
    );
    const float aspectRatio = static_cast<float>(outputWidth) / static_cast<float>(outputHeight);
    const Matrix4x4 aspectCorrection = MakeAspectCorrectionMatrix(aspectRatio);
    const Matrix4x4 world = MultiplyMatrices(MultiplyMatrices(scale, rotation), translation);
    return MultiplyMatrices(world, aspectCorrection);
}

constexpr Vertex kQuadVertices[] = {
    { { -0.6f, 0.6f, 0.0f }, { 1.0f, 0.0f, 0.0f } },
    { { 0.6f, 0.6f, 0.0f }, { 0.0f, 1.0f, 0.0f } },
    { { 0.6f, -0.6f, 0.0f }, { 0.0f, 0.0f, 1.0f } },
    { { -0.6f, -0.6f, 0.0f }, { 1.0f, 1.0f, 0.0f } }
};

constexpr Uint16 kQuadIndices[] = {
    0, 1, 2,
    0, 2, 3
};

constexpr Uint32 kQuadVertexCount =
    static_cast<Uint32>(sizeof(kQuadVertices) / sizeof(kQuadVertices[0]));
constexpr Uint32 kQuadIndexCount =
    static_cast<Uint32>(sizeof(kQuadIndices) / sizeof(kQuadIndices[0]));
constexpr Uint32 kQuadVertexDataSize = static_cast<Uint32>(sizeof(kQuadVertices));
constexpr Uint32 kQuadIndexDataSize = static_cast<Uint32>(sizeof(kQuadIndices));

constexpr bool AreQuadIndicesValid() {
    for (const Uint16 index : kQuadIndices) {
        if (index >= kQuadVertexCount) {
            return false;
        }
    }
    return true;
}

static_assert(AreQuadIndicesValid());
static_assert(kQuadIndexCount % 3 == 0);
static_assert(sizeof(Uint16) == 2);

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

SDL_GPUBuffer* CreateQuadVertexBuffer(SDL_GPUDevice* device) {
    SDL_GPUBuffer* vertexBuffer = CreateUploadedBuffer(
        device,
        kQuadVertices,
        kQuadVertexDataSize,
        SDL_GPU_BUFFERUSAGE_VERTEX
    );
    if (vertexBuffer != nullptr) {
        SDL_Log("Vertex upload submitted: %u vertices, %u bytes", kQuadVertexCount, kQuadVertexDataSize);
    }
    return vertexBuffer;
}

SDL_GPUBuffer* CreateQuadIndexBuffer(SDL_GPUDevice* device) {
    SDL_GPUBuffer* indexBuffer = CreateUploadedBuffer(
        device,
        kQuadIndices,
        kQuadIndexDataSize,
        SDL_GPU_BUFFERUSAGE_INDEX
    );
    if (indexBuffer != nullptr) {
        SDL_Log("Index upload submitted: %u indices, %u bytes (16-bit)", kQuadIndexCount, kQuadIndexDataSize);
    }
    return indexBuffer;
}

SDL_GPUShader* LoadShader(
    SDL_GPUDevice* device,
    const char* filename,
    SDL_GPUShaderStage stage,
    Uint32 uniformBufferCount
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
    SDL_Window* window
) {
    SDL_GPUShader* vertexShader = LoadShader(
        device,
        "triangle.vert.dxil",
        SDL_GPU_SHADERSTAGE_VERTEX,
        1
    );

    if (vertexShader == nullptr) {
        return nullptr;
    }

    SDL_GPUShader* fragmentShader = LoadShader(
        device,
        "triangle.frag.dxil",
        SDL_GPU_SHADERSTAGE_FRAGMENT,
        0
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
    vertexDescription.pitch = static_cast<Uint32>(sizeof(Vertex));
    vertexDescription.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute attributes[2]{};
    attributes[0].location = 0;
    attributes[0].buffer_slot = 0;
    attributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attributes[0].offset = static_cast<Uint32>(offsetof(Vertex, position));
    attributes[1].location = 1;
    attributes[1].buffer_slot = 0;
    attributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attributes[1].offset = static_cast<Uint32>(offsetof(Vertex, color));

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
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.target_info.color_target_descriptions = &colorTarget;
    info.target_info.num_color_targets = 1;

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

int RunLoop(
    SDL_GPUDevice* device,
    SDL_Window* window,
    SDL_GPUGraphicsPipeline* pipeline,
    SDL_GPUBuffer* vertexBuffer,
    SDL_GPUBuffer* indexBuffer,
    WindowSettings& windowSettings,
    bool testDisplaySettings
) {
    bool isRunning = true;
    const Uint64 animationStartedNS = SDL_GetTicksNS();
    const SDL_Scancode testKeys[] = {
        SDL_SCANCODE_F2, SDL_SCANCODE_F11, SDL_SCANCODE_F11,
        SDL_SCANCODE_F11, SDL_SCANCODE_F11, SDL_SCANCODE_F1,
        SDL_SCANCODE_F11, SDL_SCANCODE_F2, SDL_SCANCODE_F8
    };
    const int testKeyCount = static_cast<int>(sizeof(testKeys) / sizeof(testKeys[0]));
    const Uint64 testStarted = SDL_GetTicks();
    int testStep = 0;
    int renderedFrames = 0;
    bool testedMaximize = false;
    bool testedMinimize = false;
    Uint64 restoreAt = 0;
    int finishAfterFrame = 0;

    while (isRunning) {
        SDL_Event event;

        while (SDL_PollEvent(&event)) {
            windowSettings.ProcessEvent(event);
            if (event.type == SDL_EVENT_QUIT ||
                event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                isRunning = false;
            }

            if (event.type == SDL_EVENT_KEY_DOWN &&
                event.key.scancode == SDL_SCANCODE_ESCAPE) {
                isRunning = false;
            }
        }

        if (!isRunning) {
            break;
        }

        // Exercise the same event path without touching the physical keyboard.
        if (testDisplaySettings) {
            if (SDL_GetTicks() - testStarted > 10000) {
                SDL_Log("Display smoke test timed out");
                return 1;
            }
            if (restoreAt != 0 && SDL_GetTicks() >= restoreAt) {
                if (!SDL_RestoreWindow(window) || !SDL_SyncWindow(window)) {
                    SDL_Log("Smoke test restore failed: %s", SDL_GetError());
                    return 1;
                }
                restoreAt = 0;
                finishAfterFrame = renderedFrames + 5;
            }
            if (testStep < testKeyCount && renderedFrames >= 5 + testStep * 10) {
                SDL_Event testEvent{};
                testEvent.key.type = SDL_EVENT_KEY_DOWN;
                testEvent.key.windowID = SDL_GetWindowID(window);
                testEvent.key.scancode = testKeys[testStep++];
                windowSettings.ProcessEvent(testEvent);
            }
            if (!testedMaximize && renderedFrames >= testKeyCount * 10 + 5) {
                if (!SDL_MaximizeWindow(window) || !SDL_SyncWindow(window)) {
                    SDL_Log("Smoke test maximize failed: %s", SDL_GetError());
                    return 1;
                }
                SDL_Event testEvent{};
                testEvent.key.type = SDL_EVENT_KEY_DOWN;
                testEvent.key.windowID = SDL_GetWindowID(window);
                testEvent.key.scancode = SDL_SCANCODE_F1;
                windowSettings.ProcessEvent(testEvent);
                testedMaximize = true;
            }
            if (!testedMinimize && renderedFrames >= testKeyCount * 10 + 15) {
                if (!SDL_MinimizeWindow(window) || !SDL_SyncWindow(window)) {
                    SDL_Log("Smoke test minimize failed: %s", SDL_GetError());
                    return 1;
                }
                testedMinimize = true;
                restoreAt = SDL_GetTicks() + 100;
            }
            if (finishAfterFrame > 0 && renderedFrames >= finishAfterFrame) {
                SDL_Log("PASS: GPU rendering after display changes, maximize and minimize/restore (%d frames)", renderedFrames);
                return 0;
            }
        }

        if (!windowSettings.ApplyPending() && testDisplaySettings) {
            return 1;
        }

        SDL_GPUCommandBuffer* commands =
            SDL_AcquireGPUCommandBuffer(device);

        if (commands == nullptr) {
            SDL_Log(
                "SDL_AcquireGPUCommandBuffer failed: %s",
                SDL_GetError()
            );
            return 1;
        }

        SDL_GPUTexture* backbuffer = nullptr;
        Uint32 outputWidth = 0;
        Uint32 outputHeight = 0;

        if (!SDL_WaitAndAcquireGPUSwapchainTexture(
            commands,
            window,
            &backbuffer,
            &outputWidth,
            &outputHeight)) {
            SDL_Log(
                "SDL_WaitAndAcquireGPUSwapchainTexture failed: %s",
                SDL_GetError()
            );
            SDL_CancelGPUCommandBuffer(commands);
            return 1;
        }

        if (backbuffer == nullptr) {
            if (!SDL_CancelGPUCommandBuffer(commands)) {
                SDL_Log(
                    "SDL_CancelGPUCommandBuffer failed: %s",
                    SDL_GetError()
                );
                return 1;
            }

            SDL_Delay(10);
            continue;
        }

        windowSettings.UpdateOutputSize(outputWidth, outputHeight);

        const double elapsedSeconds = (SDL_GetTicksNS() - animationStartedNS) / 1000000000.0;
        const Matrix4x4 transform = BuildQuadTransform(elapsedSeconds, outputWidth, outputHeight);
        SDL_PushGPUVertexUniformData(commands, 0, &transform, static_cast<Uint32>(sizeof(transform)));

        SDL_GPUColorTargetInfo colorTarget{};
        colorTarget.texture = backbuffer;
        colorTarget.clear_color = {
            0.05f, 0.08f, 0.12f, 1.0f
        };
        colorTarget.load_op = SDL_GPU_LOADOP_CLEAR;
        colorTarget.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(
            commands,
            &colorTarget,
            1,
            nullptr
        );

        if (pass == nullptr) {
            SDL_Log(
                "SDL_BeginGPURenderPass failed: %s",
                SDL_GetError()
            );
            SDL_SubmitGPUCommandBuffer(commands);
            return 1;
        }

        SDL_BindGPUGraphicsPipeline(pass, pipeline);
        SDL_GPUBufferBinding vertexBinding{};
        vertexBinding.buffer = vertexBuffer;
        vertexBinding.offset = 0;
        SDL_BindGPUVertexBuffers(pass, 0, &vertexBinding, 1);

        SDL_GPUBufferBinding indexBinding{};
        indexBinding.buffer = indexBuffer;
        indexBinding.offset = 0;
        SDL_BindGPUIndexBuffer(pass, &indexBinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
        SDL_DrawGPUIndexedPrimitives(pass, kQuadIndexCount, 1, 0, 0, 0);
        SDL_EndGPURenderPass(pass);

        if (!SDL_SubmitGPUCommandBuffer(commands)) {
            SDL_Log(
                "SDL_SubmitGPUCommandBuffer failed: %s",
                SDL_GetError()
            );
            return 1;
        }
        if (testDisplaySettings) {
            ++renderedFrames;
        }
    }

    return 0;
}

int main(int argc, char** argv) {
    const bool testDisplaySettings = argc > 1 && std::strcmp(argv[1], "--test-display-settings") == 0;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    const DisplaySettings displaySettings = LoadDisplaySettings();

    SDL_GPUDevice* device = SDL_CreateGPUDevice(
        SDL_GPU_SHADERFORMAT_DXIL,
        true,
        "direct3d12"
    );

    if (device == nullptr) {
        SDL_Log("SDL_CreateGPUDevice failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "SDL GPU Transform Quad",
        displaySettings.windowWidth,
        displaySettings.windowHeight,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );

    if (window == nullptr) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_DestroyGPUDevice(device);
        SDL_Quit();
        return 1;
    }

    WindowSettings windowSettings(window);
    if (!windowSettings.Apply(displaySettings)) {
        SDL_Log("Startup display settings could not be applied; continuing with the actual window state");
    }
    SDL_Log("Display shortcuts: F1=1280x720, F2=1600x900, F8=next display, F11=fullscreen, Escape=quit");
    SDL_Log("Transform: position=(%.2f,%.2f), scale=%.2f, rotation=%.1f degrees/second, vertex uniform=%u bytes",
        kQuadTransformSettings.positionX, kQuadTransformSettings.positionY,
        kQuadTransformSettings.scale, kQuadTransformSettings.rotationSpeedDegreesPerSecond,
        static_cast<Uint32>(sizeof(Matrix4x4)));

    const bool windowClaimed =
        SDL_ClaimWindowForGPUDevice(device, window);

    int exitCode = 1;

    if (!windowClaimed) {
        SDL_Log(
            "SDL_ClaimWindowForGPUDevice failed: %s",
            SDL_GetError()
        );
    }
    else {
        SDL_GPUGraphicsPipeline* pipeline =
            CreatePipeline(device, window);

        if (pipeline != nullptr) {
            SDL_GPUBuffer* vertexBuffer = CreateQuadVertexBuffer(device);
            if (vertexBuffer != nullptr) {
                SDL_GPUBuffer* indexBuffer = CreateQuadIndexBuffer(device);
                if (indexBuffer != nullptr) {
                    exitCode = RunLoop(device, window, pipeline, vertexBuffer, indexBuffer, windowSettings, testDisplaySettings);
                    SDL_ReleaseGPUBuffer(device, indexBuffer);
                }
                SDL_ReleaseGPUBuffer(device, vertexBuffer);
            }
            SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        }

        SDL_ReleaseWindowFromGPUDevice(device, window);
    }

    SDL_DestroyWindow(window);
    SDL_DestroyGPUDevice(device);
    SDL_Quit();

    return exitCode;
}
