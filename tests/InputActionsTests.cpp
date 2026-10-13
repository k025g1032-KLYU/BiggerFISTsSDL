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
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
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
    input.BeginFrame();
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F1));
    success &= Expect(defaults.Evaluate(input).toggleThirdPerson.pressed,
        "F1 must map to the shared combat camera command");
    input.BeginFrame();
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_7));
    success &= Expect(defaults.Evaluate(input).practiceCommand ==
        PracticeCommand::MixedEnemies, "7 must map to the mixed enemy command");
    input.BeginFrame();
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_B));
    success &= Expect(defaults.Evaluate(input).practiceCommand ==
        PracticeCommand::BossBoxer, "B must map to the Boss Boxer command");
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_F1));
    const auto checkPracticeKey = [&](SDL_Scancode key, PracticeCommand expected) {
        input.BeginFrame();
        input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, key));
        const bool matched = defaults.Evaluate(input).practiceCommand == expected;
        input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_UP, key));
        return matched;
    };
    input.BeginFrame();
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F2));
    success &= Expect(defaults.Evaluate(input).cycleCockpitModel.pressed,
        "F2 must map to the shared cockpit model command");
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_F2));
    input.BeginFrame();
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F3));
    success &= Expect(defaults.Evaluate(input).toggleCombatData.pressed,
        "F3 must map to the shared combat data command");
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_F3));
    success &= Expect(checkPracticeKey(SDL_SCANCODE_F5, PracticeCommand::FullStartup) &&
        checkPracticeKey(SDL_SCANCODE_F6, PracticeCommand::QuickStartup) &&
        checkPracticeKey(SDL_SCANCODE_F7, PracticeCommand::BossStartup) &&
        checkPracticeKey(SDL_SCANCODE_F8, PracticeCommand::SystemOffline),
        "F2, F3 and F5-F8 must map to distinct practice commands");
    success &= Expect(checkPracticeKey(SDL_SCANCODE_0, PracticeCommand::BossSpawn) &&
        checkPracticeKey(SDL_SCANCODE_KP_0, PracticeCommand::BossSpawn) &&
        checkPracticeKey(SDL_SCANCODE_8, PracticeCommand::BossCycleNeedles) &&
        checkPracticeKey(SDL_SCANCODE_9, PracticeCommand::BossConeCharge) &&
        checkPracticeKey(SDL_SCANCODE_C, PracticeCommand::BossNeedleCombo) &&
        checkPracticeKey(SDL_SCANCODE_N, PracticeCommand::BossFullBarrage) &&
        checkPracticeKey(SDL_SCANCODE_M, PracticeCommand::BossSpreadBurst),
        "Boss practice keys must retain their original mapping");
    input.BeginFrame();
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_LSHIFT));
    success &= Expect(checkPracticeKey(SDL_SCANCODE_1, PracticeCommand::SingleMissile) &&
        checkPracticeKey(SDL_SCANCODE_2, PracticeCommand::MissileLine) &&
        checkPracticeKey(SDL_SCANCODE_3, PracticeCommand::FloatingBombs) &&
        checkPracticeKey(SDL_SCANCODE_4, PracticeCommand::MixedProjectiles),
        "Shift+1-4 must select projectile patterns");
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_LSHIFT));
    success &= Expect(!IsGamepadTriggerPressed(0) &&
        !IsGamepadTriggerPressed(3855) && IsGamepadTriggerPressed(3856),
        "Trigger threshold must match the original XInput 30/255 boundary");
    input.BeginFrame();
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_LCTRL));
    success &= Expect(checkPracticeKey(SDL_SCANCODE_F1, PracticeCommand::None),
        "Ctrl+F1 must remain a display shortcut");
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_LCTRL));
    input.BeginFrame();
    SDL_Event quickShift = KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_1);
    quickShift.key.mod = SDL_KMOD_SHIFT;
    input.ProcessEvent(quickShift);
    success &= Expect(defaults.Evaluate(input).practiceCommand ==
        PracticeCommand::SingleMissile,
        "A Shift+1 tap must survive a modifier released in the same frame");
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_1));
    input.BeginFrame();
    SDL_Event quickCtrl = KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_F1);
    quickCtrl.key.mod = SDL_KMOD_CTRL;
    input.ProcessEvent(quickCtrl);
    success &= Expect(defaults.Evaluate(input).practiceCommand == PracticeCommand::None,
        "A Ctrl+F1 tap must never trigger the practice camera");
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_F1));

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
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_LSHIFT));
    state = defaults.Evaluate(input);
    success &= Expect(state.shiftDown && state.leftMouseDown &&
        state.rightMouseDown,
        "Left Shift plus both mouse buttons reaches both arms");
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_LSHIFT));
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_DOWN, SDL_SCANCODE_RSHIFT));
    state = defaults.Evaluate(input);
    success &= Expect(state.shiftDown && state.leftMouseDown &&
        state.rightMouseDown,
        "Right Shift plus both mouse buttons reaches both arms");
    input.ProcessEvent(KeyEvent(SDL_EVENT_KEY_UP, SDL_SCANCODE_RSHIFT));
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
        !defaults.Evaluate(input).rightMouseDown &&
        !defaults.Evaluate(input).shiftDown,
        "Losing focus releases mouse attacks and modifiers");
    state = defaults.Evaluate(input);
    success &= Expect(!state.changeBackground.pressed && !state.quit.pressed,
        "Non-keyboard events must not create key actions");
    SDL_Event focusGained{};
    focusGained.type = SDL_EVENT_WINDOW_FOCUS_GAINED;
    input.ProcessEvent(focusGained);

    SDL_VirtualJoystickDesc virtualDesc{};
    virtualDesc.version = sizeof(virtualDesc);
    virtualDesc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    virtualDesc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    virtualDesc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    virtualDesc.button_mask = (1u << SDL_GAMEPAD_BUTTON_COUNT) - 1u;
    virtualDesc.axis_mask = (1u << SDL_GAMEPAD_AXIS_COUNT) - 1u;
    virtualDesc.name = "Input mapping test gamepad";
    const SDL_JoystickID virtualId = SDL_AttachVirtualJoystick(&virtualDesc);
    SDL_Gamepad* gamepad = virtualId ? SDL_OpenGamepad(virtualId) : nullptr;
    success &= Expect(gamepad != nullptr,
        "A virtual gamepad must be available for input mapping tests");
    if (gamepad) {
        SDL_Joystick* joystick = SDL_GetGamepadJoystick(gamepad);
        const auto setButton = [&](SDL_GamepadButton button, bool down) {
            SDL_SetJoystickVirtualButton(joystick, button, down);
            SDL_UpdateJoysticks();
        };
        const auto setAxis = [&](SDL_GamepadAxis axis, Sint16 value) {
            SDL_SetJoystickVirtualAxis(joystick, axis, value);
            SDL_UpdateJoysticks();
        };
        setAxis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
        state = defaults.Evaluate(input, gamepad);
        success &= Expect(!state.leftPunchKeyDown,
            "A released virtual trigger must stay below the threshold");
        setAxis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_JOYSTICK_AXIS_MAX);
        setAxis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_JOYSTICK_AXIS_MAX);
        state = defaults.Evaluate(input, gamepad);
        success &= Expect(state.leftPunchKeyDown && state.rightPunchKeyDown,
            "Both triggers above threshold must charge independently");
        setButton(SDL_GAMEPAD_BUTTON_BACK, true);
        setButton(SDL_GAMEPAD_BUTTON_RIGHT_STICK, true);
        setButton(SDL_GAMEPAD_BUTTON_START, true);
        setButton(SDL_GAMEPAD_BUTTON_SOUTH, true);
        setButton(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, true);
        setButton(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, true);
        state = defaults.Evaluate(input, gamepad);
        success &= Expect(state.toggleCombatData.pressed &&
            state.switchView.pressed && state.battleStart.pressed &&
            state.skip.pressed && state.confirm.pressed &&
            state.dodge.pressed && state.leftGamepadRocketDown &&
            state.rightGamepadRocketDown && !state.leftMouseDown &&
            !state.rightMouseDown,
            "View, R3, Start, A and both shoulders keep distinct mappings");
        state = defaults.Evaluate(input, gamepad);
        success &= Expect(!state.toggleCombatData.pressed &&
            !state.switchView.pressed && !state.battleStart.pressed,
            "Held gamepad buttons must not repeat press edges");
        input.ProcessEvent(focusLost);
        state = defaults.Evaluate(input, gamepad);
        success &= Expect(!state.toggleCombatData.pressed &&
            !state.leftPunchKeyDown,
            "Losing focus must clear gamepad actions");
        input.ProcessEvent(focusGained);
        setButton(SDL_GAMEPAD_BUTTON_BACK, false);
        setButton(SDL_GAMEPAD_BUTTON_RIGHT_STICK, false);
        setButton(SDL_GAMEPAD_BUTTON_START, false);
        setButton(SDL_GAMEPAD_BUTTON_SOUTH, false);
        setButton(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, false);
        setButton(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, false);
        setAxis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
        setAxis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, SDL_JOYSTICK_AXIS_MIN);
        defaults.Evaluate(input, gamepad);
        SDL_CloseGamepad(gamepad);
    }
    if (virtualId) SDL_DetachVirtualJoystick(virtualId);

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
