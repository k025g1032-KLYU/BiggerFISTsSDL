#pragma once

#include <SDL3/SDL.h>

SDL_GPUTextureFormat SelectDepthFormat(SDL_GPUDevice* device);

class DepthBuffer {
public:
    DepthBuffer(SDL_GPUDevice* device, SDL_GPUTextureFormat format);
    ~DepthBuffer();

    DepthBuffer(const DepthBuffer&) = delete;
    DepthBuffer& operator=(const DepthBuffer&) = delete;

    bool Resize(Uint32 width, Uint32 height);
    SDL_GPUTexture* GetTexture() const;

private:
    SDL_GPUDevice* device_;
    SDL_GPUTextureFormat format_;
    SDL_GPUTexture* texture_ = nullptr;
    Uint32 width_ = 0;
    Uint32 height_ = 0;
};
