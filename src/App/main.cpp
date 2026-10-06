#include "App/Application.h"

#include <SDL3/SDL_main.h>

#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    ApplicationOptions options;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--smoke-test") == 0 && !options.smokeTest) {
            options.smokeTest = true;
        } else if (std::strcmp(argv[i], "--scene-preview") == 0 &&
            !options.scenePreview && !options.firstPersonPreview) {
            options.scenePreview = true;
        } else if (std::strcmp(argv[i], "--first-person-preview") == 0 &&
            !options.firstPersonPreview && !options.scenePreview) {
            options.firstPersonPreview = true;
        } else {
            std::fprintf(stderr,
                "Unknown, duplicate or conflicting option: %s\nUsage: BiggerFISTsSDL [--scene-preview | --first-person-preview] [--smoke-test]\n",
                argv[i]);
            return 1;
        }
    }
    Application application;
    return application.Run(options);
}
