#include "Input/InputActions.h"

#include <SDL3/SDL_main.h>

#include <cstdio>
#include <string>

namespace {
bool Expect(bool condition, const char* message) {
    if (!condition) {
        SDL_Log("FAIL: %s", message);
    }
    return condition;
}

SDL_Event KeyEvent(SDL_EventType type, SDL_Scancode key, bool repeat = false) {
    SDL_Event event{};
    event.key.type = type;
    event.key.scancode = key;
    event.key.down = type == SDL_EVENT_KEY_DOWN;
    event.key.repeat = repeat;
    return event;
}

std::string TestConfigPath() {
    return "input-bindings-test-" + std::to_string(SDL_GetTicksNS()) + ".cfg";
}

InputBindings LoadTestConfig(const std::string& text, bool& success) {
    const std::string path = TestConfigPath();
    if (!SDL_SaveFile(path.c_str(), text.data(), text.size())) {
        success &= Expect(false, "Test config must be saved successfully");
        return {};
    }

    const InputBindings bindings = LoadInputBindings(path.c_str());
    success &= Expect(std::remove(path.c_str()) == 0,
        "Temporary test config must be removed");
    return bindings;
}

bool HasDefaultBindings(const InputBindings& bindings) {
    return bindings.moveLeft == SDL_SCANCODE_A &&
        bindings.moveRight == SDL_SCANCODE_D &&
        bindings.moveUp == SDL_SCANCODE_W &&
        bindings.moveDown == SDL_SCANCODE_S &&
        bindings.leftPunch == SDL_SCANCODE_Q &&
        bindings.rightPunch == SDL_SCANCODE_E &&
        bindings.resetTarget == SDL_SCANCODE_R &&
        bindings.changeBackground == SDL_SCANCODE_SPACE &&
        bindings.quit == SDL_SCANCODE_ESCAPE;
}
}

int main(int, char**) {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        return 1;
    }

    bool success = true;
    Input input;
    InputActions defaults;

    success &= Expect(!input.IsKeyDown(SDL_SCANCODE_W),
        "Queries before Update must be safe");

    input.BeginFrame();
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE));
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_SPACE));
    input.Update();

    GameInput state = defaults.Evaluate(input);
    success &= Expect(state.changeBackground.pressed &&
        state.changeBackground.released,
        "A tap within one frame must preserve both edges");

    input.BeginFrame();
    state = defaults.Evaluate(input);
    success &= Expect(!state.changeBackground.pressed &&
        !state.changeBackground.released,
        "Edges must not persist into the next frame");

    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE, true));
    success &= Expect(!defaults.Evaluate(input).changeBackground.pressed,
        "Keyboard repeat must not trigger another press");

    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE));
    success &= Expect(defaults.Evaluate(input).changeBackground.pressed,
        "A new press must trigger the default action");

    InputBindings bindings;
    bindings.changeBackground = SDL_SCANCODE_RETURN;
    InputActions remapped(bindings);
    success &= Expect(!remapped.Evaluate(input).changeBackground.pressed,
        "The previous key must not trigger a remapped action");

    input.BeginFrame();
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RETURN));
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_RETURN));
    state = remapped.Evaluate(input);
    success &= Expect(state.changeBackground.pressed &&
        state.changeBackground.released,
        "A remapped action must preserve both tap edges");
    success &= Expect(!defaults.Evaluate(input).changeBackground.pressed,
        "Independent bindings must not change the defaults");

    input.BeginFrame();
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_ESCAPE));
    success &= Expect(defaults.Evaluate(input).quit.pressed,
        "Escape must trigger the quit action");
    input.BeginFrame();
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_R));
    success &= Expect(defaults.Evaluate(input).resetTarget.pressed,
        "R must trigger the Target reset action");

    const SDL_Scancode invalidKeys[] = {
        SDL_SCANCODE_UNKNOWN,
        static_cast<SDL_Scancode>(-1),
        SDL_SCANCODE_COUNT
    };
    for (SDL_Scancode key : invalidKeys) {
        input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, key));
        input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_UP, key));
        success &= Expect(!input.IsKeyPressed(key) && !input.IsKeyReleased(key),
            "Invalid scancodes must not create edges");
        success &= Expect(!input.IsKeyDown(key),
            "Invalid scancodes must not access keyboard data");
    }

    input.BeginFrame();
    SDL_Event otherEvent{};
    otherEvent.type = SDL_EVENT_MOUSE_MOTION;
    otherEvent.motion.xrel = 3.0f;
    otherEvent.motion.yrel = -2.0f;
    input.ProcessEvent(otherEvent);
    input.ProcessEvent(otherEvent);
    success &= Expect(input.GetMouseDeltaX() == 6.0f &&
        input.GetMouseDeltaY() == -4.0f,
        "Relative mouse movement accumulates during the frame");
    input.BeginFrame();
    success &= Expect(input.GetMouseDeltaX() == 0.0f &&
        input.GetMouseDeltaY() == 0.0f,
        "Relative mouse movement resets each frame");
    input.ProcessEvent(otherEvent);
    SDL_Event mouseDown{};
    mouseDown.button.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    mouseDown.button.button = SDL_BUTTON_LEFT;
    input.ProcessEvent(mouseDown);
    success &= Expect(defaults.Evaluate(input).leftMouseDown &&
        !defaults.Evaluate(input).rightMouseDown,
        "Left mouse button is mapped independently to the left arm");
    mouseDown.button.button = SDL_BUTTON_RIGHT;
    input.ProcessEvent(mouseDown);
    success &= Expect(defaults.Evaluate(input).leftMouseDown &&
        defaults.Evaluate(input).rightMouseDown,
        "Both mouse buttons can be held independently");
    SDL_Event mouseUp{};
    mouseUp.button.type = SDL_EVENT_MOUSE_BUTTON_UP;
    mouseUp.button.button = SDL_BUTTON_LEFT;
    input.ProcessEvent(mouseUp);
    success &= Expect(!defaults.Evaluate(input).leftMouseDown &&
        defaults.Evaluate(input).rightMouseDown,
        "Releasing one mouse button leaves the other held");
    SDL_Event focusLost{};
    focusLost.type = SDL_EVENT_WINDOW_FOCUS_LOST;
    input.ProcessEvent(focusLost);
    success &= Expect(input.GetMouseDeltaX() == 0.0f &&
        input.GetMouseDeltaY() == 0.0f,
        "Mouse movement is discarded when the window loses focus");
    success &= Expect(!defaults.Evaluate(input).leftMouseDown &&
        !defaults.Evaluate(input).rightMouseDown,
        "Losing focus releases both mouse attack buttons");
    state = defaults.Evaluate(input);
    success &= Expect(!state.changeBackground.pressed && !state.quit.pressed,
        "Non-keyboard events must not create key actions");
    SDL_Event focusGained{};
    focusGained.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
    input.ProcessEvent(focusGained);

    const InputBindings configured = LoadTestConfig(
        "moveLeft Left\nmoveRight Right\nmoveUp Up\nmoveDown Down\n"
        "leftPunch Z\nrightPunch X\nresetTarget T\nchangeBackground Return\nquit Q\n", success);
    success &= Expect(configured.moveLeft == SDL_SCANCODE_LEFT &&
        configured.moveRight == SDL_SCANCODE_RIGHT &&
        configured.moveUp == SDL_SCANCODE_UP &&
        configured.moveDown == SDL_SCANCODE_DOWN &&
        configured.leftPunch == SDL_SCANCODE_Z &&
        configured.rightPunch == SDL_SCANCODE_X &&
        configured.resetTarget == SDL_SCANCODE_T &&
        configured.changeBackground == SDL_SCANCODE_RETURN &&
        configured.quit == SDL_SCANCODE_Q,
        "All actions must load their configured keys");

    InputActions fromFile(configured);
    input.BeginFrame();
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_SPACE));
    success &= Expect(!fromFile.Evaluate(input).changeBackground.pressed,
        "The original key must not activate an action remapped by file");
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RETURN));
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_RETURN));
    state = fromFile.Evaluate(input);
    success &= Expect(state.changeBackground.pressed && state.changeBackground.released,
        "Bindings loaded from file must reach the game actions");

    const InputBindings spaced = LoadTestConfig(
        "\xEF\xBB\xBF"
        "\r\n  moveLeft   Left Shift \t\r\n\tmoveUp Q\r\n", success);
    success &= Expect(spaced.moveLeft == SDL_SCANCODE_LSHIFT &&
        spaced.moveUp == SDL_SCANCODE_Q &&
        spaced.moveRight == SDL_SCANCODE_D &&
        spaced.changeBackground == SDL_SCANCODE_SPACE,
        "BOM, CRLF, whitespace and multiword keys must load with partial defaults");

    const InputBindings invalid = LoadTestConfig(
        "unknownAction Q\nmoveRight NotAKey\nmoveDown\nquit Escape extra\n"
        "moveUp Q\nmoveUp E\nmoveUp InvalidKey\nchangeBackground Return\n", success);
    success &= Expect(invalid.moveLeft == SDL_SCANCODE_A &&
        invalid.moveRight == SDL_SCANCODE_D &&
        invalid.moveDown == SDL_SCANCODE_S &&
        invalid.quit == SDL_SCANCODE_ESCAPE &&
        invalid.moveUp == SDL_SCANCODE_E &&
        invalid.changeBackground == SDL_SCANCODE_RETURN,
        "Invalid lines must be skipped and the last valid binding must be retained");

    success &= Expect(HasDefaultBindings(LoadTestConfig(" \t\r\n", success)),
        "An empty config must retain every default binding");
    const std::string missingPath = TestConfigPath();
    success &= Expect(HasDefaultBindings(LoadInputBindings(missingPath.c_str())),
        "A missing config must retain every default binding");

    SDL_Quit();
    if (success) {
        SDL_Log("PASS: input edges, action bindings and config loading");
    }
    return success ? 0 : 1;
}
