#include "Platform/WindowSettings.h"

#include <SDL3/SDL_main.h>

#include <cstdio>
#include <limits>
#include <string>

namespace {
bool Expect(bool condition, const char* message) {
    if (!condition) {
        SDL_Log("FAIL: %s", message);
    }
    return condition;
}

bool IsFullscreen(SDL_Window* window) {
    return (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN) != 0;
}

SDL_Event KeyEvent(SDL_Window* window, SDL_Scancode key, bool repeat = false) {
    SDL_Event event{};
    event.key.type = SDL_EVENT_KEY_DOWN;
    event.key.windowID = SDL_GetWindowID(window);
    event.key.scancode = key;
    event.key.repeat = repeat;
    return event;
}

DisplaySettings LoadTestConfig(const std::string& text, bool& success) {
    const std::string path = "display-settings-test-" + std::to_string(SDL_GetTicksNS()) + ".cfg";
    if (!SDL_SaveFile(path.c_str(), text.data(), text.size())) {
        success &= Expect(false, "Test config must be saved successfully");
        return {};
    }
    const DisplaySettings settings = LoadDisplaySettings(path.c_str());
    success &= Expect(std::remove(path.c_str()) == 0, "Temporary config must be removed");
    return settings;
}
}

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    bool success = true;

    const DisplaySettings loaded = LoadTestConfig(
        "\xEF\xBB\xBF"
        "windowWidth 1600\r\nwindowHeight 900\r\nfullscreen 1\r\ndisplayIndex 2\r\n", success);
    success &= Expect(loaded.windowWidth == 1600 && loaded.windowHeight == 900 &&
        loaded.fullscreen && loaded.displayIndex == 2, "BOM and CRLF config must load all settings");
    const DisplaySettings invalid = LoadTestConfig(
        "windowWidth 640\nwindowWidth 800\nwindowWidth -1\nwindowHeight 0\n"
        "fullscreen 2\ndisplayIndex -1\nunknown 5\nwindowHeight 500 extra\n"
        "windowWidth 999999999999999999999\nwindowHeight 720.5\nfullscreen\n", success);
    success &= Expect(invalid.windowWidth == 800 && invalid.windowHeight == 720 &&
        !invalid.fullscreen && invalid.displayIndex == 0,
        "Invalid lines must preserve defaults or the last valid setting");
    const std::string missing = "missing-display-" + std::to_string(SDL_GetTicksNS()) + ".cfg";
    const DisplaySettings defaults = LoadDisplaySettings(missing.c_str());
    success &= Expect(defaults.windowWidth == 1280 && defaults.windowHeight == 720 &&
        !defaults.fullscreen && defaults.displayIndex == 0, "A missing file must use defaults");

    SDL_Window* window = SDL_CreateWindow("Window settings tests", 640, 480,
        SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE);
    if (window == nullptr) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }
    WindowSettings controls(window);
    DisplaySettings baseline;
    baseline.windowWidth = 640;
    baseline.windowHeight = 480;
    success &= Expect(controls.Apply(baseline), "Initial settings must apply");

    DisplaySettings badSize = baseline;
    badSize.windowWidth = -1;
    success &= Expect(!controls.Apply(badSize) && controls.GetSettings().windowWidth == 640,
        "An invalid runtime size must leave the current geometry intact");

    controls.ProcessEvent(KeyEvent(window, SDL_SCANCODE_F2, true));
    success &= Expect(controls.ApplyPending() && controls.GetSettings().windowWidth == 640,
        "Repeated key events must not resize the window");
    SDL_Event foreign = KeyEvent(window, SDL_SCANCODE_F2);
    foreign.key.windowID += 1;
    controls.ProcessEvent(foreign);
    success &= Expect(controls.ApplyPending() && controls.GetSettings().windowWidth == 640,
        "Events for another window must be ignored");

    controls.ProcessEvent(KeyEvent(window, SDL_SCANCODE_F11));
    controls.ProcessEvent(KeyEvent(window, SDL_SCANCODE_F11));
    success &= Expect(controls.ApplyPending() && !IsFullscreen(window),
        "Two toggles in one frame must preserve the original mode");

    int savedX = 0;
    int savedY = 0;
    SDL_GetWindowPosition(window, &savedX, &savedY);
    for (int i = 0; i < 3; ++i) {
        controls.ProcessEvent(KeyEvent(window, SDL_SCANCODE_F11));
        success &= Expect(!IsFullscreen(window), "Queued changes must wait until ApplyPending");
        success &= Expect(controls.ApplyPending() && IsFullscreen(window), "F11 must enter fullscreen");
        success &= Expect(controls.GetSettings().windowWidth == 640 &&
            controls.GetSettings().windowHeight == 480, "Fullscreen must preserve the normal size");
        controls.ProcessEvent(KeyEvent(window, SDL_SCANCODE_F11));
        success &= Expect(controls.ApplyPending() && !IsFullscreen(window), "F11 must leave fullscreen");
        int width = 0;
        int height = 0;
        int x = 0;
        int y = 0;
        SDL_GetWindowSize(window, &width, &height);
        SDL_GetWindowPosition(window, &x, &y);
        success &= Expect(width == 640 && height == 480 && x == savedX && y == savedY,
            "Repeated fullscreen round trips must restore normal geometry");
    }

    controls.ProcessEvent(KeyEvent(window, SDL_SCANCODE_F11));
    success &= Expect(controls.ApplyPending(), "Fullscreen setup must apply");
    controls.ProcessEvent(KeyEvent(window, SDL_SCANCODE_F2));
    success &= Expect(controls.ApplyPending() && !IsFullscreen(window),
        "Selecting a window size while fullscreen must restore windowed mode");
    int actualWidth = 0;
    int actualHeight = 0;
    SDL_GetWindowSize(window, &actualWidth, &actualHeight);
    success &= Expect(actualWidth == controls.GetSettings().windowWidth &&
        actualHeight == controls.GetSettings().windowHeight && actualWidth > 0 && actualHeight > 0,
        "Stored dimensions must match the applied window size");

    success &= Expect(SDL_SetWindowSize(window, 640, 480) && SDL_SyncWindow(window),
        "Manual resize setup must succeed");
    SDL_Event resized{};
    resized.window.type = SDL_EVENT_WINDOW_RESIZED;
    resized.window.windowID = SDL_GetWindowID(window);
    controls.ProcessEvent(resized);
    controls.ProcessEvent(KeyEvent(window, SDL_SCANCODE_F11));
    success &= Expect(controls.ApplyPending(), "Fullscreen after manual resize must apply");
    controls.ProcessEvent(KeyEvent(window, SDL_SCANCODE_F11));
    success &= Expect(controls.ApplyPending() && controls.GetSettings().windowWidth == 640 &&
        controls.GetSettings().windowHeight == 480, "Manual window size must survive a fullscreen round trip");

    if (SDL_strcmp(SDL_GetCurrentVideoDriver(), "dummy") != 0) {
        success &= Expect(SDL_MaximizeWindow(window) && SDL_SyncWindow(window),
            "Maximize setup must succeed");
        controls.ProcessEvent(KeyEvent(window, SDL_SCANCODE_F1));
        success &= Expect(controls.ApplyPending() &&
            (SDL_GetWindowFlags(window) & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN)) == 0,
            "Choosing a window size must leave maximized and fullscreen states");
    }
    else {
        SDL_Log("SKIP: the dummy driver cannot maximize; covered by the native GPU smoke test");
    }

    int count = 0;
    SDL_DisplayID* ids = SDL_GetDisplays(&count);
    const SDL_DisplayID beforeCycle = SDL_GetDisplayForWindow(window);
    controls.ProcessEvent(KeyEvent(window, SDL_SCANCODE_F8));
    success &= Expect(controls.ApplyPending(), "Display cycling must succeed or safely do nothing");
    success &= Expect(count > 1 ? SDL_GetDisplayForWindow(window) != beforeCycle :
        SDL_GetDisplayForWindow(window) == beforeCycle,
        "F8 must cycle a real display list and handle a single display");
    SDL_free(ids);
    SDL_Log("Display cycle tested with %d connected display(s)", count);

    DisplaySettings unavailable = controls.GetSettings();
    unavailable.displayIndex = std::numeric_limits<int>::max();
    success &= Expect(controls.Apply(unavailable) &&
        SDL_GetDisplayForWindow(window) == SDL_GetPrimaryDisplay(),
        "An unavailable display must fall back to the primary display");
    SDL_Event removed{};
    removed.display.type = SDL_EVENT_DISPLAY_REMOVED;
    removed.display.displayID = SDL_GetDisplayForWindow(window);
    controls.ProcessEvent(removed);
    success &= Expect(controls.ApplyPending() &&
        SDL_GetDisplayForWindow(window) == SDL_GetPrimaryDisplay(),
        "Removal handling must queue a fallback to the primary display");

    SDL_DestroyWindow(window);
    SDL_Quit();
    if (success) {
        SDL_Log("PASS: display config, queued shortcuts, geometry restore, maximize and display fallback");
    }
    return success ? 0 : 1;
}
