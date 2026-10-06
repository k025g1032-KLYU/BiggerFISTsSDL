#pragma once

#include <SDL3/SDL.h>

// Returns an uploaded, single-level RGBA8 texture, or nullptr on failure.
// The caller must release it with SDL_ReleaseGPUTexture before destroying device.
SDL_GPUTexture* LoadPngTexture(SDL_GPUDevice* device, const char* path);
