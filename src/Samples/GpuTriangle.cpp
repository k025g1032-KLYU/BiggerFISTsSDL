#include "Platform/WindowSettings.h"
#include "Rendering/CubeRenderer.h"
#include "World/CubeScene.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <cstring>

namespace {
int RunLoop(CubeRenderer& renderer, SDL_Window* window, WindowSettings& windowSettings,
    bool testDisplaySettings) {
    CubeScene scene{};
    const Uint64 animationStartedNS = SDL_GetTicksNS();
    const SDL_Scancode testKeys[] = {
        SDL_SCANCODE_F2, SDL_SCANCODE_F11, SDL_SCANCODE_F11,
        SDL_SCANCODE_F11, SDL_SCANCODE_F11, SDL_SCANCODE_F1,
        SDL_SCANCODE_F11, SDL_SCANCODE_F2, SDL_SCANCODE_F8
    };
    const int testKeyCount = static_cast<int>(sizeof(testKeys) / sizeof(testKeys[0]));
    const Uint64 testStarted = SDL_GetTicks();
    int testStep = 0;
    int renderedFrames = 0;
    bool testedMaximize = false;
    bool testedMinimize = false;
    Uint64 restoreAt = 0;
    int finishAfterFrame = 0;
    bool isRunning = true;

    while (isRunning) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            windowSettings.ProcessEvent(event);
            if (event.type == SDL_EVENT_QUIT ||
                event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED ||
                (event.type == SDL_EVENT_KEY_DOWN && event.key.scancode == SDL_SCANCODE_ESCAPE)) {
                isRunning = false;
            }
        }
        if (!isRunning) {
            break;
        }

        if (testDisplaySettings) {
            if (SDL_GetTicks() - testStarted > 10000) {
                SDL_Log("Display smoke test timed out");
                return 1;
            }
            if (restoreAt != 0 && SDL_GetTicks() >= restoreAt) {
                if (!SDL_RestoreWindow(window) || !SDL_SyncWindow(window)) {
                    SDL_Log("Smoke test restore failed: %s", SDL_GetError());
                    return 1;
                }
                restoreAt = 0;
                finishAfterFrame = renderedFrames + 5;
            }
            if (testStep < testKeyCount && renderedFrames >= 5 + testStep * 10) {
                SDL_Event testEvent{};
                testEvent.key.type = SDL_EVENT_KEY_DOWN;
                testEvent.key.windowID = SDL_GetWindowID(window);
                testEvent.key.scancode = testKeys[testStep++];
                windowSettings.ProcessEvent(testEvent);
            }
            if (!testedMaximize && renderedFrames >= testKeyCount * 10 + 5) {
                if (!SDL_MaximizeWindow(window) || !SDL_SyncWindow(window)) {
                    SDL_Log("Smoke test maximize failed: %s", SDL_GetError());
                    return 1;
                }
                SDL_Event testEvent{};
                testEvent.key.type = SDL_EVENT_KEY_DOWN;
                testEvent.key.windowID = SDL_GetWindowID(window);
                testEvent.key.scancode = SDL_SCANCODE_F1;
                windowSettings.ProcessEvent(testEvent);
                testedMaximize = true;
            }
            if (!testedMinimize && renderedFrames >= testKeyCount * 10 + 15) {
                if (!SDL_MinimizeWindow(window) || !SDL_SyncWindow(window)) {
                    SDL_Log("Smoke test minimize failed: %s", SDL_GetError());
                    return 1;
                }
                testedMinimize = true;
                restoreAt = SDL_GetTicks() + 100;
            }
            if (finishAfterFrame > 0 && renderedFrames >= finishAfterFrame) {
                SDL_Log("PASS: GPU rendering after display changes, maximize and minimize/restore (%d frames)",
                    renderedFrames);
                return 0;
            }
        }

        if (!windowSettings.ApplyPending() && testDisplaySettings) {
            return 1;
        }
        scene.elapsedSeconds = (SDL_GetTicksNS() - animationStartedNS) / 1000000000.0;
        bool frameRendered = false;
        if (!renderer.Render(scene, windowSettings, &frameRendered)) {
            return 1;
        }
        if (testDisplaySettings && frameRendered) {
            ++renderedFrames;
        }
    }
    return 0;
}
}

int main(int argc, char** argv) {
    const bool testDisplaySettings = argc > 1 &&
        std::strcmp(argv[1], "--test-display-settings") == 0;
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    const DisplaySettings displaySettings = LoadDisplaySettings();
    SDL_Window* window = SDL_CreateWindow(
        "SDL GPU Perspective Cube", displaySettings.windowWidth,
        displaySettings.windowHeight, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    WindowSettings windowSettings(window);
    if (!windowSettings.Apply(displaySettings)) {
        SDL_Log("Startup display settings could not be applied; continuing with the actual window state");
    }
    SDL_Log("Display shortcuts: F1=1280x720, F2=1600x900, F8=next display, F11=fullscreen, Escape=quit");

    CubeRenderer renderer;
    int exitCode = 1;
    if (renderer.Initialize(window)) {
        exitCode = RunLoop(renderer, window, windowSettings, testDisplaySettings);
    }
    renderer.Shutdown();
    SDL_DestroyWindow(window);
    SDL_Quit();
    return exitCode;
}
