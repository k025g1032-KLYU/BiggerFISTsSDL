#pragma once

#include <SDL3/SDL.h>

// Returns an uploaded, single-level RGBA8 texture, or nullptr on failure.
// The caller must release it with SDL_ReleaseGPUTexture before destroying device.
SDL_GPUTexture* LoadPngTexture(SDL_GPUDevice* device, const char* path,
    SDL_GPUTextureFormat format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM);
