#include "Rendering/TextureLoader.h"

#include <cstring>
#include <limits>
#include <memory>

SDL_GPUTexture* LoadPngTexture(SDL_GPUDevice* device, const char* path) {
    if (device == nullptr || path == nullptr || path[0] == '\0') {
        SDL_Log("LoadPngTexture requires a GPU device and a non-empty path");
        return nullptr;
    }

    using SurfacePtr = std::unique_ptr<SDL_Surface, decltype(&SDL_DestroySurface)>;
    SurfacePtr loaded(SDL_LoadPNG(path), SDL_DestroySurface);
    if (!loaded) {
        SDL_Log("Could not load PNG %s: %s", path, SDL_GetError());
        return nullptr;
    }
    SurfacePtr surface(SDL_ConvertSurface(loaded.get(), SDL_PIXELFORMAT_RGBA32), SDL_DestroySurface);
    loaded.reset();
    if (!surface) {
        SDL_Log("Could not convert PNG %s to RGBA32: %s", path, SDL_GetError());
        return nullptr;
    }
    if (surface->w <= 0 || surface->h <= 0 || surface->pitch <= 0) {
        SDL_Log("Invalid PNG dimensions or pitch: %s", path);
        return nullptr;
    }

    const Uint32 width = static_cast<Uint32>(surface->w);
    const Uint32 height = static_cast<Uint32>(surface->h);
    const Uint64 rowBytes = static_cast<Uint64>(width) * 4;
    // Align upload rows to 256 bytes for D3D12. The starting offset is zero.
    const Uint64 uploadPitch = (rowBytes + 255) & ~Uint64{255};
    const Uint64 uploadSize = uploadPitch * height;
    if (rowBytes > static_cast<Uint64>(surface->pitch) ||
        uploadSize > std::numeric_limits<Uint32>::max()) {
        SDL_Log("PNG is too large or has an invalid row pitch: %s", path);
        return nullptr;
    }

    SDL_GPUTextureCreateInfo textureInfo{};
    textureInfo.type = SDL_GPU_TEXTURETYPE_2D;
    textureInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
    textureInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;
    textureInfo.width = width;
    textureInfo.height = height;
    textureInfo.layer_count_or_depth = 1;
    textureInfo.num_levels = 1;
    textureInfo.sample_count = SDL_GPU_SAMPLECOUNT_1;
    SDL_GPUTexture* texture = SDL_CreateGPUTexture(device, &textureInfo);
    if (texture == nullptr) {
        SDL_Log("Could not create GPU texture for %s: %s", path, SDL_GetError());
        return nullptr;
    }

    SDL_GPUTransferBufferCreateInfo transferInfo{};
    transferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    transferInfo.size = static_cast<Uint32>(uploadSize);
    SDL_GPUTransferBuffer* transfer = SDL_CreateGPUTransferBuffer(device, &transferInfo);
    if (transfer == nullptr) {
        SDL_Log("Could not create texture upload buffer for %s: %s", path, SDL_GetError());
        SDL_ReleaseGPUTexture(device, texture);
        return nullptr;
    }
    void* mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
    if (mapped == nullptr) {
        SDL_Log("Could not map texture upload buffer for %s: %s", path, SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return nullptr;
    }
    if (!SDL_LockSurface(surface.get())) {
        SDL_Log("Could not lock PNG pixels for %s: %s", path, SDL_GetError());
        SDL_UnmapGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return nullptr;
    }
    // Copy actual pixels row by row; CPU and GPU row padding can differ.
    const auto* pixels = static_cast<const Uint8*>(surface->pixels);
    auto* destination = static_cast<Uint8*>(mapped);
    for (Uint32 y = 0; y < height; ++y) {
        std::memcpy(destination + static_cast<size_t>(y) * static_cast<size_t>(uploadPitch),
            pixels + static_cast<size_t>(y) * static_cast<size_t>(surface->pitch),
            static_cast<size_t>(rowBytes));
    }
    SDL_UnlockSurface(surface.get());
    SDL_UnmapGPUTransferBuffer(device, transfer);
    surface.reset();

    SDL_GPUCommandBuffer* commands = SDL_AcquireGPUCommandBuffer(device);
    if (commands == nullptr) {
        SDL_Log("Could not acquire texture upload commands for %s: %s", path, SDL_GetError());
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return nullptr;
    }
    SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(commands);
    if (copyPass == nullptr) {
        SDL_Log("Could not begin texture copy pass for %s: %s", path, SDL_GetError());
        SDL_CancelGPUCommandBuffer(commands);
        SDL_ReleaseGPUTransferBuffer(device, transfer);
        SDL_ReleaseGPUTexture(device, texture);
        return nullptr;
    }
    SDL_GPUTextureTransferInfo source{};
    source.transfer_buffer = transfer;
    source.pixels_per_row = static_cast<Uint32>(uploadPitch / 4);
    source.rows_per_layer = height;
    SDL_GPUTextureRegion region{};
    region.texture = texture;
    region.w = width;
    region.h = height;
    region.d = 1;
    SDL_UploadToGPUTexture(copyPass, &source, &region, false);
    SDL_EndGPUCopyPass(copyPass);

    const bool submitted = SDL_SubmitGPUCommandBuffer(commands);
    // SDL keeps submitted resources alive until the GPU has finished using them.
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    if (!submitted) {
        SDL_Log("Could not submit texture upload for %s: %s", path, SDL_GetError());
        SDL_ReleaseGPUTexture(device, texture);
        return nullptr;
    }
    SDL_Log("PNG upload submitted: %s (%u x %u, RGBA8)", path, width, height);
    return texture;
}
