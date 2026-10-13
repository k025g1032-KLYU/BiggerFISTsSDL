#include "Rendering/UiRenderer.h"

void UiDrawList::Image(std::string_view name, float x, float y, float width,
    float height, UiColor color, bool cropStory) {
    commands_.push_back({std::string(name), x, y, width, height,
        0.0f, 0.0f, 1.0f, 1.0f, color, cropStory});
    commands_.back().placement = placement_;
}

void UiDrawList::Rectangle(float x, float y, float width, float height, UiColor color) {
    Image("white1x1.png", x, y, width, height, color);
}

void UiDrawList::RotatedRectangle(float centerX, float centerY,
    float width, float height, float radians, UiColor color) {
    Image("white1x1.png", centerX - width * 0.5f,
        centerY - height * 0.5f, width, height, color);
    commands_.back().rotationRadians = radians;
}

void UiDrawList::ImageFrame(std::string_view name, float x, float y,
    float width, float height, int columns, int frame) {
    if (columns <= 0) return;
    commands_.push_back({std::string(name), x, y, width, height,
        static_cast<float>(frame % columns) / columns, 0.0f,
        1.0f / columns, 1.0f, {}, false});
    commands_.back().placement = placement_;
}

void UiDrawList::Text(std::string_view value, float x, float y, float scale, UiColor color) {
    const float glyphWidth = 9.0f * scale;
    const float glyphHeight = 18.0f * scale;
    const float startX = x;
    for (const unsigned char glyph : value) {
        if (glyph == '\n') { x = startX; y += glyphHeight; continue; }
        if (glyph < 32 || glyph > 126) { x += glyphWidth; continue; }
        const int index = glyph - 32;
        const int column = index % 14;
        const int row = index / 14;
        commands_.push_back({"debugfont.png", x, y, glyphWidth, glyphHeight,
            column * 9.0f / 128.0f, row * 18.0f / 128.0f,
            9.0f / 128.0f, 18.0f / 128.0f, color, false});
        commands_.back().placement = placement_;
        x += glyphWidth;
    }
}
