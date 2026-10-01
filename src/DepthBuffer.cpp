#include "DepthBuffer.h"

SDL_GPUTextureFormat SelectDepthFormat(SDL_GPUDevice* device) {
    const SDL_GPUTextureFormat candidates[] = {
        SDL_GPU_TEXTUREFORMAT_D32_FLOAT,
        SDL_GPU_TEXTUREFORMAT_D24_UNORM,
        SDL_GPU_TEXTUREFORMAT_D16_UNORM
    };
    for (const SDL_GPUTextureFormat format : candidates) {
        if (SDL_GPUTextureSupportsFormat(device, format, SDL_GPU_TEXTURETYPE_2D,
                SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET)) {
            SDL_Log("Selected depth format: %d", static_cast<int>(format));
            return format;
        }
    }
    SDL_Log("No supported depth target format is available");
    return SDL_GPU_TEXTUREFORMAT_INVALID;
}

DepthBuffer::DepthBuffer(SDL_GPUDevice* device, SDL_GPUTextureFormat format)
    : device_(device), format_(format) {
}

DepthBuffer::~DepthBuffer() {
    SDL_ReleaseGPUTexture(device_, texture_);
}

SDL_GPUTexture* DepthBuffer::GetTexture() const {
    return texture_;
}

bool DepthBuffer::Resize(Uint32 width, Uint32 height) {
    if (width == 0 || height == 0) {
        SDL_Log("Invalid depth buffer dimensions: %ux%u", width, height);
        return false;
    }
    if (texture_ != nullptr && width == width_ && height == height_) {
        return true;
    }

    const SDL_PropertiesID properties = SDL_CreateProperties();
    if (properties == 0) {
        SDL_Log("Could not create depth texture properties: %s", SDL_GetError());
        return false;
    }
    if (!SDL_SetFloatProperty(properties, SDL_PROP_GPU_TEXTURE_CREATE_D3D12_CLEAR_DEPTH_FLOAT, 1.0f)) {
        SDL_Log("Could not set the depth clear value: %s", SDL_GetError());
        SDL_DestroyProperties(properties);
        return false;
    }

    SDL_GPUTextureCreateInfo info{};
    info.type = SDL_GPU_TEXTURETYPE_2D;
    info.format = format_;
    info.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
    info.width = width;
    info.height = height;
    info.layer_count_or_depth = 1;
    info.num_levels = 1;
    info.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.props = properties;
    SDL_GPUTexture* replacement = SDL_CreateGPUTexture(device_, &info);
    SDL_DestroyProperties(properties);
    if (replacement == nullptr) {
        SDL_Log("Could not resize the depth buffer to %ux%u: %s", width, height, SDL_GetError());
        return false;
    }

    // Create first, then release the previous texture; SDL defers unsafe destruction.
    SDL_ReleaseGPUTexture(device_, texture_);
    texture_ = replacement;
    width_ = width;
    height_ = height;
    SDL_Log("Depth buffer resized: %ux%u", width_, height_);
    return true;
}
