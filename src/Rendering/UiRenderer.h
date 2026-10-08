#pragma once

#include <SDL3/SDL.h>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct UiColor { float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f; };
struct StoryConfig;
struct UiCommand {
    std::string image;
    float x = 0.0f, y = 0.0f, width = 0.0f, height = 0.0f;
    float u = 0.0f, v = 0.0f, uSize = 1.0f, vSize = 1.0f;
    UiColor color{};
    bool cropStory = false;
};

class UiDrawList {
public:
    void Clear() { commands_.clear(); }
    void Image(std::string_view name, float x, float y, float width, float height,
        UiColor color = {}, bool cropStory = false);
    void ImageFrame(std::string_view name, float x, float y, float width,
        float height, int columns, int frame);
    void Rectangle(float x, float y, float width, float height, UiColor color);
    void Text(std::string_view text, float x, float y, float scale = 1.0f,
        UiColor color = {});
    const std::vector<UiCommand>& GetCommands() const { return commands_; }
private:
    std::vector<UiCommand> commands_;
};

class UiRenderer {
public:
    ~UiRenderer();
    bool Initialize(SDL_GPUDevice* device, SDL_Window* window,
        const std::filesystem::path& assetsRoot, const StoryConfig& stories,
        std::string& error);
    bool Draw(SDL_GPUCommandBuffer* commands, SDL_GPUTexture* backbuffer,
        Uint32 width, Uint32 height, const UiDrawList& list);
    void Shutdown();
private:
    SDL_GPUDevice* device_ = nullptr;
    SDL_GPUGraphicsPipeline* pipeline_ = nullptr;
    SDL_GPUSampler* sampler_ = nullptr;
    std::unordered_map<std::string, SDL_GPUTexture*> textures_;
};
