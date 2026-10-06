#pragma once

#include <SDL3/SDL.h>

struct InputBindings {
    SDL_Scancode moveLeft = SDL_SCANCODE_A;
    SDL_Scancode moveRight = SDL_SCANCODE_D;
    SDL_Scancode moveUp = SDL_SCANCODE_W;
    SDL_Scancode moveDown = SDL_SCANCODE_S;
    SDL_Scancode leftPunch = SDL_SCANCODE_Q;
    SDL_Scancode rightPunch = SDL_SCANCODE_E;
    SDL_Scancode resetTarget = SDL_SCANCODE_R;
    SDL_Scancode changeBackground = SDL_SCANCODE_SPACE;
    SDL_Scancode quit = SDL_SCANCODE_ESCAPE;
};

InputBindings LoadInputBindings(const char* filePath = nullptr);
