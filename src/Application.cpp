#include "Application.h"

#include "FrameTimer.h"
#include "Game.h"
#include "GameConfig.h"
#include "Input.h"
#include "InputActions.h"
#include "InputBindings.h"
#include "WindowSettings.h"

#include <SDL3/SDL.h>

int Application::Run() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    const GameConfig config = LoadGameConfig();
    const InputBindings bindings = LoadInputBindings();
    const DisplaySettings displaySettings = LoadDisplaySettings();

    SDL_Window* window = SDL_CreateWindow(
        "BiggerFISTs SDL3",
        displaySettings.windowWidth,
        displaySettings.windowHeight,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY
    );
    if (window == nullptr) {
        SDL_Log("SDL_CreateWindow failed: %s", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    WindowSettings windowSettings(window);
    if (!windowSettings.Apply(displaySettings)) {
        SDL_Log("Using the current window after display settings could not be applied");
    }
    SDL_Log("Display shortcuts: F11 fullscreen, F1 1280x720, F2 1600x900, F8 next display");

    SDL_Renderer* renderer = SDL_CreateRenderer(window, nullptr);
    if (renderer == nullptr) {
        SDL_Log("SDL_CreateRenderer failed: %s", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    Game game(config);
    FrameTimer frameTimer(config.targetFps);
    Input input;
    InputActions inputActions(bindings);

    bool isRunning = true;

    while (isRunning) {
        frameTimer.BeginFrame();
        const double deltaTime = frameTimer.GetDeltaTime();
        input.BeginFrame();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            input.ProcessEvent(event);
            windowSettings.ProcessEvent(event);

            if (event.type == SDL_EVENT_QUIT || event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                isRunning = false;
            }
        }

        input.Update();
        const GameInput gameInput = inputActions.Evaluate(input);

        if (gameInput.quit.pressed) {
            isRunning = false;
        }

        if (!isRunning) {
            break;
        }

        windowSettings.ApplyPending();

        int outputWidth = 0;
        int outputHeight = 0;

        if (!SDL_GetRenderOutputSize(
                renderer,
                &outputWidth,
                &outputHeight
            )) {
            SDL_Log(
                "SDL_GetRenderOutputSize failed: %s",
                SDL_GetError()
            );
            break;
        }

        windowSettings.UpdateOutputSize(
            static_cast<Uint32>(outputWidth),
            static_cast<Uint32>(outputHeight)
        );

        game.Update(
            deltaTime,
            gameInput,
            outputWidth,
            outputHeight
        );

        if (!game.Draw(renderer) ||
            !SDL_RenderPresent(renderer)) {
            SDL_Log("Rendering failed: %s", SDL_GetError());
            isRunning = false;
        }

        frameTimer.EndFrame();
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
