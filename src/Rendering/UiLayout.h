#pragma once

#include "Rendering/UiRenderer.h"

#include <algorithm>

struct UiBounds {
    float left = 0.0f;
    float top = 0.0f;
    float width = 0.0f;
    float height = 0.0f;
};

struct UiLayout {
    float width = 1280.0f;
    float height = 720.0f;
    float safeScale = 1.0f;
    float safeOffsetX = 0.0f;
    float safeOffsetY = 0.0f;

    UiLayout(float outputWidth, float outputHeight)
        : width(outputWidth), height(outputHeight) {
        safeScale = std::min(width / 1280.0f, height / 720.0f);
        safeOffsetX = (width - 1280.0f * safeScale) * 0.5f;
        safeOffsetY = (height - 720.0f * safeScale) * 0.5f;
    }

    UiBounds Map(const UiCommand& command) const {
        float scaleX = safeScale;
        float scaleY = safeScale;
        float offsetX = safeOffsetX;
        float offsetY = safeOffsetY;
        switch (command.placement) {
        case UiPlacement::Cover:
            scaleX = scaleY = std::max(width / 1280.0f, height / 720.0f);
            offsetX = (width - 1280.0f * scaleX) * 0.5f;
            offsetY = (height - 720.0f * scaleY) * 0.5f;
            break;
        case UiPlacement::FullViewport:
            scaleX = width / 1280.0f;
            scaleY = height / 720.0f;
            offsetX = offsetY = 0.0f;
            break;
        case UiPlacement::FullWidth:
            scaleX = width / 1280.0f;
            offsetX = 0.0f;
            break;
        case UiPlacement::LeftEdge:
            offsetX = 0.0f;
            break;
        case UiPlacement::RightEdge:
            offsetX = width - 1280.0f * safeScale;
            break;
        case UiPlacement::Safe:
            break;
        }
        return {offsetX + command.x * scaleX,
            offsetY + command.y * scaleY,
            command.width * scaleX, command.height * scaleY};
    }
};
