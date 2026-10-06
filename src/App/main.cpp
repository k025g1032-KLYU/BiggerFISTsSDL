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
        } else if (std::strcmp(argv[i], "--missile-unit-model-preview") == 0 &&
            !options.scenePreview && !options.firstPersonPreview) {
            options.scenePreview = true;
            options.missileUnitModelPreview = true;
        } else if (std::strcmp(argv[i], "--first-person-preview") == 0 &&
            !options.firstPersonPreview && !options.scenePreview) {
            options.firstPersonPreview = true;
        } else if (std::strcmp(argv[i], "--player-combat-preview") == 0 &&
            !options.firstPersonPreview && !options.scenePreview) {
            options.firstPersonPreview = true;
            options.playerCombatPreview = true;
        } else if (std::strncmp(argv[i], "--battle-preview=", 17) == 0 &&
            !options.firstPersonPreview && !options.scenePreview) {
            const char* value = argv[i] + 17;
            if (std::strcmp(value, "missile-unit") == 0) {
                options.battlePreview = BattlePreviewKind::MissileUnit;
            } else if (std::strcmp(value, "bomber") == 0) {
                options.battlePreview = BattlePreviewKind::Bomber;
            } else if (std::strcmp(value, "charge-unit") == 0) {
                options.battlePreview = BattlePreviewKind::ChargeUnit;
            } else if (std::strcmp(value, "day1") == 0) {
                options.battlePreview = BattlePreviewKind::Day1;
            } else if (std::strcmp(value, "day2") == 0) {
                options.battlePreview = BattlePreviewKind::Day2;
            } else if (std::strcmp(value, "boss") == 0) {
                options.battlePreview = BattlePreviewKind::Boss;
            } else {
                std::fprintf(stderr, "Unknown battle preview: %s\n", value);
                return 1;
            }
            options.firstPersonPreview = true;
        } else if (std::strcmp(argv[i], "--dynamic-target-smoke-test") == 0 &&
            !options.dynamicTargetSmokeTest && !options.firstPersonPreview &&
            !options.scenePreview && !options.smokeTest) {
            options.dynamicTargetSmokeTest = true;
            options.firstPersonPreview = true;
            options.smokeTest = true;
        } else {
            std::fprintf(stderr,
                "Unknown, duplicate or conflicting option: %s\nUsage: BiggerFISTsSDL [--scene-preview | --missile-unit-model-preview | --first-person-preview | --player-combat-preview | --battle-preview=missile-unit|bomber|charge-unit|day1|day2|boss] [--smoke-test]\n       BiggerFISTsSDL --dynamic-target-smoke-test\n",
                argv[i]);
            return 1;
        }
    }
    Application application;
    return application.Run(options);
}
