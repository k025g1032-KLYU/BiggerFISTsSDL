#include "App/Application.h"

#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    ApplicationOptions options;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--smoke-test") == 0 && !options.smokeTest) {
            options.smokeTest = true;
        } else if (std::strcmp(argv[i], "--scene-preview") == 0 && !options.scenePreview) {
            options.scenePreview = true;
        } else {
            std::fprintf(stderr,
                "Unknown or duplicate option: %s\nUsage: BiggerFISTsSDL [--scene-preview] [--smoke-test]\n",
                argv[i]);
            return 1;
        }
    }
    Application application;
    return application.Run(options);
}
