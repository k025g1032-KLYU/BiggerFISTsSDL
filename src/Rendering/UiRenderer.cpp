#include "Rendering/UiRenderer.h"

#include "Rendering/TextureLoader.h"
#include "Rendering/UiLayout.h"
#include "Data/StoryConfig.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {
std::string PathText(const std::filesystem::path& path) {
    const auto bytes = path.u8string();
    return {reinterpret_cast<const char*>(bytes.data()), bytes.size()};
}
SDL_GPUShader* LoadUiShader(SDL_GPUDevice* device, const char* filename,
    SDL_GPUShaderStage stage) {
    const std::string path = std::string(SDL_GetBasePath()) + "shaders/" + filename;
    size_t length = 0;
    void* bytes = SDL_LoadFile(path.c_str(), &length);
    if (!bytes) return nullptr;
    SDL_GPUShaderCreateInfo info{};
    info.code = static_cast<const Uint8*>(bytes);
    info.code_size = length;
    info.entrypoint = "main";
    info.format = SDL_GPU_SHADERFORMAT_DXIL;
    info.stage = stage;
    info.num_uniform_buffers = 1;
    info.num_samplers = stage == SDL_GPU_SHADERSTAGE_FRAGMENT ? 1 : 0;
    SDL_GPUShader* shader = SDL_CreateGPUShader(device, &info);
    SDL_free(bytes);
    return shader;
}
}

UiRenderer::~UiRenderer() { Shutdown(); }

bool UiRenderer::Initialize(SDL_GPUDevice* device, SDL_Window* window,
    const std::filesystem::path& assetsRoot, const StoryConfig& stories,
    std::string& error) {
    Shutdown();
    device_ = device;
    SDL_GPUShader* vertex = LoadUiShader(device, "ui.vert.dxil", SDL_GPU_SHADERSTAGE_VERTEX);
    SDL_GPUShader* fragment = LoadUiShader(device, "ui.frag.dxil", SDL_GPU_SHADERSTAGE_FRAGMENT);
    if (!vertex || !fragment) {
        error = std::string("Could not load UI shaders: ") + SDL_GetError();
        if (vertex) SDL_ReleaseGPUShader(device, vertex);
        if (fragment) SDL_ReleaseGPUShader(device, fragment);
        Shutdown();
        return false;
    }
    SDL_GPUColorTargetDescription target{};
    target.format = SDL_GetGPUSwapchainTextureFormat(device, window);
    target.blend_state.enable_blend = true;
    target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.color_write_mask = SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G |
        SDL_GPU_COLORCOMPONENT_B | SDL_GPU_COLORCOMPONENT_A;
    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = vertex;
    info.fragment_shader = fragment;
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;
    pipeline_ = SDL_CreateGPUGraphicsPipeline(device, &info);
    SDL_ReleaseGPUShader(device, vertex);
    SDL_ReleaseGPUShader(device, fragment);
    SDL_GPUSamplerCreateInfo samplerInfo{};
    samplerInfo.min_filter = SDL_GPU_FILTER_LINEAR;
    samplerInfo.mag_filter = SDL_GPU_FILTER_LINEAR;
    samplerInfo.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    samplerInfo.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    samplerInfo.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    imageSampler_ = SDL_CreateGPUSampler(device, &samplerInfo);
    samplerInfo.min_filter = SDL_GPU_FILTER_NEAREST;
    samplerInfo.mag_filter = SDL_GPU_FILTER_NEAREST;
    fontSampler_ = SDL_CreateGPUSampler(device, &samplerInfo);
    if (!pipeline_ || !imageSampler_ || !fontSampler_) {
        error = std::string("Could not create UI pipeline: ") + SDL_GetError();
        Shutdown();
        return false;
    }
    const std::array uiFiles{"white1x1.png", "debugfont.png", "title.png",
        "startbar.png", "Badend.png", "retry.png", "skip.png"};
    for (const char* filename : uiFiles) {
        const auto path = assetsRoot / "ui" / filename;
        textures_[filename] = LoadPngTexture(device, PathText(path).c_str());
        if (!textures_[filename]) {
            error = "Required UI image failed to load: " + PathText(path);
            Shutdown();
            return false;
        }
    }
    for (const StoryPage& page : stories.pages) {
        if (textures_.contains(page.image)) continue;
        const auto path = assetsRoot / "story" / page.image;
        textures_[page.image] = LoadPngTexture(device, PathText(path).c_str());
        if (!textures_[page.image]) {
            error = "Required story image failed to load: " + PathText(path);
            Shutdown();
            return false;
        }
    }
    return true;
}

bool UiRenderer::Draw(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* backbuffer,
    Uint32 width, Uint32 height, const UiDrawList& list) {
    if (!pipeline_ || width == 0 || height == 0) return false;
    SDL_GPUColorTargetInfo target{};
    target.texture = backbuffer;
    target.load_op = SDL_GPU_LOADOP_LOAD;
    target.store_op = SDL_GPU_STOREOP_STORE;
    SDL_GPURenderPass* pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
    if (!pass) return false;
    SDL_BindGPUGraphicsPipeline(pass, pipeline_);
    const UiLayout layout(static_cast<float>(width), static_cast<float>(height));
    SDL_Rect fullScissor{0, 0, static_cast<int>(width), static_cast<int>(height)};
    SDL_Rect storyScissor{0, static_cast<int>(layout.safeOffsetY),
        static_cast<int>(width), static_cast<int>(500.0f * layout.safeScale)};
    bool storyClipped = false;
    for (const UiCommand& command : list.GetCommands()) {
        auto found = textures_.find(command.image);
        if (found == textures_.end()) continue;
        const bool isFont = command.image == "debugfont.png";
        if (command.cropStory != storyClipped) {
            SDL_SetGPUScissor(pass, command.cropStory ? &storyScissor : &fullScissor);
            storyClipped = command.cropStory;
        }
        const UiBounds bounds = layout.Map(command);
        float left = bounds.left;
        float top = bounds.top;
        float right = left + bounds.width;
        float bottom = top + bounds.height;
        if (isFont) {
            left = std::round(left);
            top = std::round(top);
            right = std::round(right);
            bottom = std::round(bottom);
        }
        const float uniform[12]{
            left, top, right - left, bottom - top,
            command.u, command.v, command.uSize, command.vSize,
            static_cast<float>(width), static_cast<float>(height),
            command.rotationRadians, 0.0f
        };
        const float halfTexel = 0.5f / 128.0f;
        const float fragmentUniform[12]{
            command.color.r, command.color.g, command.color.b, command.color.a,
            command.u + halfTexel, command.v + halfTexel,
            command.u + command.uSize - halfTexel,
            command.v + command.vSize - halfTexel,
            isFont ? 1.0f : 0.0f, 0.0f, 0.0f, 0.0f
        };
        SDL_PushGPUVertexUniformData(commands, 0, uniform, sizeof(uniform));
        SDL_PushGPUFragmentUniformData(commands, 0,
            fragmentUniform, sizeof(fragmentUniform));
        SDL_GPUTextureSamplerBinding binding{};
        binding.texture = found->second;
        const float glyphPixelScale = bounds.width / 9.0f;
        const bool integerFontScale = isFont &&
            std::abs(glyphPixelScale - std::round(glyphPixelScale)) < 0.001f;
        binding.sampler = integerFontScale ? fontSampler_ : imageSampler_;
        SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
        SDL_DrawGPUPrimitives(pass, 6, 1, 0, 0);
    }
    SDL_EndGPURenderPass(pass);
    return true;
}

void UiRenderer::Shutdown() {
    if (!device_) return;
    SDL_WaitForGPUIdle(device_);
    for (auto& [name, texture] : textures_) {
        if (texture) SDL_ReleaseGPUTexture(device_, texture);
    }
    textures_.clear();
    if (fontSampler_) SDL_ReleaseGPUSampler(device_, fontSampler_);
    if (imageSampler_) SDL_ReleaseGPUSampler(device_, imageSampler_);
    if (pipeline_) SDL_ReleaseGPUGraphicsPipeline(device_, pipeline_);
    fontSampler_ = nullptr;
    imageSampler_ = nullptr;
    pipeline_ = nullptr;
    device_ = nullptr;
}
