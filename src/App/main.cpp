#include "App/Application.h"

#include <SDL3/SDL_main.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>

int main(int argc, char** argv) {
    ApplicationOptions options;
    for (int i = 1; i < argc; ++i) {
        if (std::strcmp(argv[i], "--smoke-test") == 0 && !options.smokeTest) {
            options.smokeTest = true;
        } else if (std::strncmp(argv[i], "--smoke-frames=", 15) == 0 &&
            options.smokeFrames == 30) {
            char* end = nullptr;
            const long count = std::strtol(argv[i] + 15, &end, 10);
            if (*end != '\0' || count < 30 || count > 3600) {
                std::fprintf(stderr, "smoke-frames must be 30..3600\n");
                return 1;
            }
            options.smokeFrames = static_cast<int>(count);
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
        } else if (std::strcmp(argv[i], "--show-hitboxes") == 0 &&
            !options.showHitboxes) {
            options.showHitboxes = true;
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
        } else if (std::strncmp(argv[i], "--presentation-preview=", 23) == 0 &&
            !options.presentationPreview && !options.scenePreview &&
            !options.firstPersonPreview) {
            const char* value = argv[i] + 23;
            if (std::strcmp(value, "boot") == 0) {
                options.presentationBoot = true;
            } else if (std::strcmp(value, "cockpit") != 0) {
                std::fprintf(stderr, "Unknown presentation preview: %s\n", value);
                return 1;
            }
            options.presentationPreview = true;
        } else if (std::strncmp(argv[i], "--presentation-tick=", 20) == 0 &&
            options.presentationTick == -1) {
            char* end = nullptr;
            const long tick = std::strtol(argv[i] + 20, &end, 10);
            if (*end != '\0' || tick < 0 || tick > 288) {
                std::fprintf(stderr, "presentation-tick must be 0..288\n");
                return 1;
            }
            options.presentationTick = static_cast<int>(tick);
        } else if (std::strncmp(argv[i], "--presentation-capture=", 23) == 0 &&
            options.presentationCapture.empty() && argv[i][23] != '\0') {
            options.presentationCapture = argv[i] + 23;
        } else if (std::strncmp(argv[i], "--capture-frame=", 16) == 0 &&
            options.captureFrame.empty() && argv[i][16] != '\0') {
            options.captureFrame = argv[i] + 16;
        } else if (std::strncmp(argv[i], "--capture-at-frame=", 19) == 0 &&
            options.captureAtFrame < 0) {
            char* end = nullptr;
            const long frame = std::strtol(argv[i] + 19, &end, 10);
            if (*end != '\0' || frame < 0 || frame > 3599) {
                std::fprintf(stderr, "capture-at-frame must be 0..3599\n");
                return 1;
            }
            options.captureAtFrame = static_cast<int>(frame);
        } else if (std::strncmp(argv[i], "--replay-input=", 15) == 0 &&
            options.replayInput.empty() && argv[i][15] != '\0') {
            options.replayInput = argv[i] + 15;
        } else {
            std::fprintf(stderr,
                "Unknown, duplicate or conflicting option: %s\nUsage: BiggerFISTsSDL [--scene-preview | --missile-unit-model-preview | --first-person-preview | --player-combat-preview | --battle-preview=missile-unit|bomber|charge-unit|day1|day2|boss [--show-hitboxes] | --presentation-preview=boot|cockpit [--presentation-tick=0..288] [--presentation-capture=FILE.bmp]] [--capture-frame=FILE.bmp [--capture-at-frame=0..3599] [--replay-input=FILE]] [--smoke-test] [--smoke-frames=30..3600]\n       BiggerFISTsSDL --dynamic-target-smoke-test\n",
                argv[i]);
            return 1;
        }
    }
    if (options.smokeFrames != 30 && !options.smokeTest) {
        std::fprintf(stderr, "--smoke-frames requires --smoke-test\n");
        return 1;
    }
    if (options.showHitboxes &&
        options.battlePreview == BattlePreviewKind::None) {
        std::fprintf(stderr, "--show-hitboxes requires --battle-preview\n");
        return 1;
    }
    if (options.presentationTick >= 0 && !options.presentationPreview) {
        std::fprintf(stderr, "--presentation-tick requires --presentation-preview\n");
        return 1;
    }
    if (!options.presentationCapture.empty() && !options.presentationPreview) {
        std::fprintf(stderr, "--presentation-capture requires --presentation-preview\n");
        return 1;
    }
    if (!options.presentationCapture.empty() && !options.captureFrame.empty()) {
        std::fprintf(stderr, "Use only one capture option\n");
        return 1;
    }
    if (options.captureAtFrame >= 0 && options.captureFrame.empty()) {
        std::fprintf(stderr, "--capture-at-frame requires --capture-frame\n");
        return 1;
    }
    if (options.captureAtFrame >= 0 && !options.smokeTest) {
        std::fprintf(stderr, "--capture-at-frame requires --smoke-test\n");
        return 1;
    }
    if (options.smokeTest && options.captureAtFrame >= options.smokeFrames) {
        std::fprintf(stderr, "capture-at-frame must be below smoke-frames\n");
        return 1;
    }
    if (!options.replayInput.empty() &&
        (options.captureAtFrame < 0 || options.presentationPreview ||
            options.scenePreview || options.dynamicTargetSmokeTest)) {
        std::fprintf(stderr,
            "--replay-input requires fixed-frame capture in a game or combat preview\n");
        return 1;
    }
    Application application;
    return application.Run(options);
}
