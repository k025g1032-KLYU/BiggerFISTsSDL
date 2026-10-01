#include "WindowSettings.h"

#include <algorithm>
#include <sstream>
#include <vector>

namespace {
constexpr int kMinWidth = 320;
constexpr int kMinHeight = 240;
constexpr int kMaxWidth = 7680;
constexpr int kMaxHeight = 4320;

bool ValidSize(int width, int height) {
    return width >= kMinWidth && width <= kMaxWidth &&
        height >= kMinHeight && height <= kMaxHeight;
}

std::vector<SDL_DisplayID> DisplayIds() {
    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);
    if (ids == nullptr) {
        SDL_Log("SDL_GetDisplays failed: %s", SDL_GetError());
        return {};
    }
    std::vector<SDL_DisplayID> result(ids, ids + count);
    SDL_free(ids);
    return result;
}

int DisplayIndex(const std::vector<SDL_DisplayID>& ids, SDL_DisplayID id) {
    const auto found = std::find(ids.begin(), ids.end(), id);
    return found == ids.end() ? -1 : static_cast<int>(found - ids.begin());
}

bool FinishRequest(SDL_Window* window, bool accepted, const char* operation) {
    if (!accepted) {
        SDL_Log("%s failed: %s", operation, SDL_GetError());
        return false;
    }
    if (!SDL_SyncWindow(window)) {
        SDL_Log("%s did not finish: %s", operation, SDL_GetError());
        return false;
    }
    return true;
}

bool ApplyToWindow(
    SDL_Window* window,
    const DisplaySettings& settings,
    SDL_DisplayID displayId,
    int x,
    int y,
    SDL_Rect* windowedGeometry = nullptr
) {
    SDL_Rect bounds{};
    if (!SDL_GetDisplayUsableBounds(displayId, &bounds) || bounds.w <= 0 || bounds.h <= 0) {
        SDL_Log("Could not get usable display bounds: %s", SDL_GetError());
        return false;
    }

    if ((SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0 &&
        !FinishRequest(window, SDL_SetWindowFullscreen(window, false), "Leave fullscreen")) {
        return false;
    }
    if ((SDL_GetWindowFlags(window) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED)) != 0 &&
        !FinishRequest(window, SDL_RestoreWindow(window), "Restore window")) {
        return false;
    }
    if (!FinishRequest(window, SDL_SetWindowFullscreenMode(window, nullptr), "Set borderless mode")) {
        return false;
    }

    const int width = std::min(settings.windowWidth, std::max(kMinWidth, bounds.w));
    const int height = std::min(settings.windowHeight, std::max(kMinHeight, bounds.h));
    if (!SDL_WINDOWPOS_ISCENTERED(x)) {
        x = std::clamp(x, bounds.x, bounds.x + std::max(0, bounds.w - width));
    }
    if (!SDL_WINDOWPOS_ISCENTERED(y)) {
        y = std::clamp(y, bounds.y, bounds.y + std::max(0, bounds.h - height));
    }
    if (!FinishRequest(window, SDL_SetWindowSize(window, width, height), "Resize window") ||
        !FinishRequest(window, SDL_SetWindowPosition(window, x, y), "Move window")) {
        return false;
    }

    if (windowedGeometry != nullptr &&
        (!SDL_GetWindowPosition(window, &windowedGeometry->x, &windowedGeometry->y) ||
         !SDL_GetWindowSize(window, &windowedGeometry->w, &windowedGeometry->h))) {
        SDL_Log("Could not query the applied window geometry: %s", SDL_GetError());
        return false;
    }

    if (settings.fullscreen &&
        !FinishRequest(window, SDL_SetWindowFullscreen(window, true), "Enter fullscreen")) {
        return false;
    }

    const bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
    if (fullscreen != settings.fullscreen || SDL_GetDisplayForWindow(window) != displayId) {
        SDL_Log("The window system did not apply the requested mode or display");
        return false;
    }
    return true;
}
}

DisplaySettings LoadDisplaySettings(const char* filePath) {
    DisplaySettings settings;
    std::string path;
    if (filePath != nullptr) {
        path = filePath;
    }
    else {
        const char* basePath = SDL_GetBasePath();
        if (basePath == nullptr) {
            SDL_Log("SDL_GetBasePath failed: %s; using default display settings", SDL_GetError());
            return settings;
        }
        path = std::string(basePath) + "display.cfg";
    }

    size_t size = 0;
    void* data = SDL_LoadFile(path.c_str(), &size);
    if (data == nullptr) {
        SDL_Log("Could not load %s: %s; using default display settings", path.c_str(), SDL_GetError());
        return settings;
    }
    std::string text(static_cast<const char*>(data), size);
    SDL_free(data);
    if (text.starts_with("\xEF\xBB\xBF")) {
        text.erase(0, 3);
    }

    std::istringstream input(text);
    std::string line;
    int lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        std::istringstream lineInput(line);
        std::string key;
        if (!(lineInput >> key)) {
            continue;
        }
        int value = 0;
        std::string extra;
        if (!(lineInput >> value) || (lineInput >> extra)) {
            SDL_Log("display.cfg line %d: invalid format; skipping line", lineNumber);
            continue;
        }
        if (key == "windowWidth" && value >= kMinWidth && value <= kMaxWidth) {
            settings.windowWidth = value;
        }
        else if (key == "windowHeight" && value >= kMinHeight && value <= kMaxHeight) {
            settings.windowHeight = value;
        }
        else if (key == "displayIndex" && value >= 0) {
            settings.displayIndex = value;
        }
        else if (key == "fullscreen" && (value == 0 || value == 1)) {
            settings.fullscreen = value != 0;
        }
        else {
            SDL_Log("display.cfg line %d: unknown key or invalid value for '%s'; skipping line",
                lineNumber, key.c_str());
        }
    }
    SDL_Log("Display config: window=%dx%d, fullscreen=%d, displayIndex=%d",
        settings.windowWidth, settings.windowHeight, settings.fullscreen ? 1 : 0, settings.displayIndex);
    return settings;
}

WindowSettings::WindowSettings(SDL_Window* window)
    : window_(window), baseTitle_(SDL_GetWindowTitle(window)) {
    if (!SDL_SetWindowMinimumSize(window_, kMinWidth, kMinHeight)) {
        SDL_Log("SDL_SetWindowMinimumSize failed: %s", SDL_GetError());
    }
    Refresh();
}

const DisplaySettings& WindowSettings::GetSettings() const {
    return settings_;
}

void WindowSettings::Refresh() {
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window_);
    settings_.fullscreen = (flags & SDL_WINDOW_FULLSCREEN) != 0;
    if ((flags & (SDL_WINDOW_FULLSCREEN | SDL_WINDOW_MAXIMIZED | SDL_WINDOW_MINIMIZED)) == 0) {
        int width = 0;
        int height = 0;
        if (SDL_GetWindowSize(window_, &width, &height) && ValidSize(width, height)) {
            settings_.windowWidth = width;
            settings_.windowHeight = height;
        }
        SDL_GetWindowPosition(window_, &windowedX_, &windowedY_);
    }
    displayId_ = SDL_GetDisplayForWindow(window_);
    const int index = DisplayIndex(DisplayIds(), displayId_);
    if (index >= 0) {
        settings_.displayIndex = index;
    }
    int width = 0;
    int height = 0;
    if (SDL_GetWindowSizeInPixels(window_, &width, &height) && width > 0 && height > 0) {
        outputWidth_ = static_cast<Uint32>(width);
        outputHeight_ = static_cast<Uint32>(height);
    }
    Report();
}

void WindowSettings::Report() {
    const char* displayName = displayId_ != 0 ? SDL_GetDisplayName(displayId_) : nullptr;
    std::ostringstream title;
    title << baseTitle_ << " | " << (settings_.fullscreen ? "Borderless" : "Windowed")
        << " | Output " << outputWidth_ << 'x' << outputHeight_
        << " | Display " << settings_.displayIndex << ": " << (displayName != nullptr ? displayName : "Unknown");
    const std::string text = title.str();
    if (text != lastTitle_) {
        if (!SDL_SetWindowTitle(window_, text.c_str())) {
            SDL_Log("SDL_SetWindowTitle failed: %s", SDL_GetError());
        }
        SDL_Log("%s | Windowed size %dx%d", text.c_str(), settings_.windowWidth, settings_.windowHeight);
        lastTitle_ = text;
    }
}

void WindowSettings::UpdateOutputSize(Uint32 width, Uint32 height) {
    if (width > 0 && height > 0 && (width != outputWidth_ || height != outputHeight_)) {
        outputWidth_ = width;
        outputHeight_ = height;
        Report();
    }
}

bool WindowSettings::Apply(const DisplaySettings& requested) {
    if (!ValidSize(requested.windowWidth, requested.windowHeight) || requested.displayIndex < 0) {
        SDL_Log("Invalid display settings; keeping the current window");
        return false;
    }

    Refresh();
    const std::vector<SDL_DisplayID> ids = DisplayIds();
    if (ids.empty()) {
        return false;
    }
    DisplaySettings target = requested;
    if (target.displayIndex >= static_cast<int>(ids.size())) {
        const int primaryIndex = DisplayIndex(ids, SDL_GetPrimaryDisplay());
        SDL_Log("Display index %d is unavailable; using the primary display", target.displayIndex);
        target.displayIndex = primaryIndex >= 0 ? primaryIndex : 0;
    }
    const SDL_DisplayID targetId = ids[target.displayIndex];
    const DisplaySettings previous = settings_;
    const SDL_DisplayID previousId = displayId_;
    const int previousX = windowedX_;
    const int previousY = windowedY_;
    const bool changingDisplay = targetId != displayId_;
    const int centered = static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(targetId));
    SDL_Rect windowedGeometry{};

    if (!ApplyToWindow(window_, target, targetId,
            changingDisplay ? centered : windowedX_, changingDisplay ? centered : windowedY_, &windowedGeometry)) {
        SDL_Log("Could not apply display settings; attempting to restore the previous window");
        const bool previousDisplayExists = DisplayIndex(DisplayIds(), previousId) >= 0;
        const SDL_DisplayID restoreId = previousDisplayExists ? previousId : SDL_GetPrimaryDisplay();
        const int restoreCentered = static_cast<int>(SDL_WINDOWPOS_CENTERED_DISPLAY(restoreId));
        if (!ApplyToWindow(window_, previous, restoreId,
                previousDisplayExists ? previousX : restoreCentered,
                previousDisplayExists ? previousY : restoreCentered)) {
            SDL_Log("Could not restore all previous window settings; using actual window state");
        }
        settings_ = previous;
        Refresh();
        return false;
    }

    // Keep the actual normal geometry even while fullscreen uses the desktop size.
    settings_.windowWidth = windowedGeometry.w;
    settings_.windowHeight = windowedGeometry.h;
    windowedX_ = windowedGeometry.x;
    windowedY_ = windowedGeometry.y;
    Refresh();
    return true;
}

void WindowSettings::ProcessEvent(const SDL_Event& event) {
    if (event.type == SDL_EVENT_DISPLAY_REMOVED) {
        if (event.display.displayID == displayId_) {
            pending_ = hasPending_ ? pending_ : settings_;
            const int index = DisplayIndex(DisplayIds(), SDL_GetPrimaryDisplay());
            pending_.displayIndex = index >= 0 ? index : 0;
            hasPending_ = true;
        }
        Refresh();
        return;
    }
    if (event.type == SDL_EVENT_DISPLAY_ADDED) {
        Refresh();
        return;
    }

    if (event.type >= SDL_EVENT_WINDOW_FIRST && event.type <= SDL_EVENT_WINDOW_LAST &&
        event.window.windowID == SDL_GetWindowID(window_)) {
        if (event.type == SDL_EVENT_WINDOW_RESIZED || event.type == SDL_EVENT_WINDOW_MOVED ||
            event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || event.type == SDL_EVENT_WINDOW_RESTORED ||
            event.type == SDL_EVENT_WINDOW_DISPLAY_CHANGED || event.type == SDL_EVENT_WINDOW_ENTER_FULLSCREEN ||
            event.type == SDL_EVENT_WINDOW_LEAVE_FULLSCREEN) {
            Refresh();
        }
        return;
    }

    if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat ||
        event.key.windowID != SDL_GetWindowID(window_)) {
        return;
    }
    const SDL_Scancode key = event.key.scancode;
    if (key != SDL_SCANCODE_F1 && key != SDL_SCANCODE_F2 && key != SDL_SCANCODE_F8 && key != SDL_SCANCODE_F11) {
        return;
    }
    if (!hasPending_) {
        Refresh();
        pending_ = settings_;
    }
    if (key == SDL_SCANCODE_F11) {
        pending_.fullscreen = !pending_.fullscreen;
    }
    else if (key == SDL_SCANCODE_F1 || key == SDL_SCANCODE_F2) {
        pending_.windowWidth = key == SDL_SCANCODE_F1 ? 1280 : 1600;
        pending_.windowHeight = key == SDL_SCANCODE_F1 ? 720 : 900;
        pending_.fullscreen = false;
    }
    else {
        const std::vector<SDL_DisplayID> ids = DisplayIds();
        if (ids.size() <= 1) {
            SDL_Log("No other display is available");
            return;
        }
        pending_.displayIndex = (pending_.displayIndex + 1) % static_cast<int>(ids.size());
    }
    hasPending_ = true;
}

bool WindowSettings::ApplyPending() {
    if (!hasPending_) {
        return true;
    }
    const DisplaySettings requested = pending_;
    hasPending_ = false;
    return Apply(requested);
}
