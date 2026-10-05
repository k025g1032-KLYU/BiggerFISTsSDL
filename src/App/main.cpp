#include "App/Application.h"

#include <SDL3/SDL_main.h>

#include <cstring>

int main(int argc, char** argv) {
    Application application;
    const bool smokeTest = argc > 1 && std::strcmp(argv[1], "--smoke-test") == 0;
    return application.Run(smokeTest);
}
