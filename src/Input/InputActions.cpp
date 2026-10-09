#include "Input/InputActions.h"

#include <algorithm>
#include <cmath>
#include <initializer_list>

InputActions::InputActions(const InputBindings& bindings)
    : bindings_(bindings) {
}

GameInput InputActions::Evaluate(const Input& input, SDL_Gamepad* gamepad,
    double deltaSeconds) {
    GameInput result{};

    if (input.IsKeyDown(bindings_.moveLeft)) result.moveX -= 1.0f;
    if (input.IsKeyDown(bindings_.moveRight)) result.moveX += 1.0f;
    if (input.IsKeyDown(bindings_.moveUp)) result.moveY -= 1.0f;
    if (input.IsKeyDown(bindings_.moveDown)) result.moveY += 1.0f;

    result.changeBackground = {
        input.IsKeyDown(bindings_.changeBackground),
        input.IsKeyPressed(bindings_.changeBackground),
        input.IsKeyReleased(bindings_.changeBackground)
    };

    result.quit = {
        input.IsKeyDown(bindings_.quit),
        input.IsKeyPressed(bindings_.quit),
        input.IsKeyReleased(bindings_.quit)
    };
    result.resetTarget = {
        input.IsKeyDown(bindings_.resetTarget),
        input.IsKeyPressed(bindings_.resetTarget),
        input.IsKeyReleased(bindings_.resetTarget)
    };
    result.battleStart = {input.IsKeyDown(SDL_SCANCODE_RETURN),
        input.IsKeyPressed(SDL_SCANCODE_RETURN),
        input.IsKeyReleased(SDL_SCANCODE_RETURN)};
    result.confirm = {input.IsKeyDown(SDL_SCANCODE_RETURN) || input.IsKeyDown(SDL_SCANCODE_SPACE),
        input.IsKeyPressed(SDL_SCANCODE_RETURN) || input.IsKeyPressed(SDL_SCANCODE_SPACE),
        input.IsKeyReleased(SDL_SCANCODE_RETURN) || input.IsKeyReleased(SDL_SCANCODE_SPACE)};
    result.skip = {input.IsKeyDown(SDL_SCANCODE_K), input.IsKeyPressed(SDL_SCANCODE_K),
        input.IsKeyReleased(SDL_SCANCODE_K)};
    result.skipPhase.pressed = input.IsKeyPressed(SDL_SCANCODE_F4);
    result.switchView.pressed = input.IsKeyPressed(SDL_SCANCODE_V);
    result.dodge = {input.IsKeyDown(SDL_SCANCODE_SPACE),
        input.IsKeyPressed(SDL_SCANCODE_SPACE),
        input.IsKeyReleased(SDL_SCANCODE_SPACE)};
    result.testProjectileHit.pressed = input.IsKeyPressed(SDL_SCANCODE_H);
    result.testChargeHit.pressed = input.IsKeyPressed(SDL_SCANCODE_J);
    result.testBurstHit.pressed = input.IsKeyPressed(SDL_SCANCODE_K);

    result.leftPunchKeyDown = input.IsKeyDown(bindings_.leftPunch);
    result.rightPunchKeyDown = input.IsKeyDown(bindings_.rightPunch);
    result.leftMouseDown = input.IsMouseButtonDown(SDL_BUTTON_LEFT);
    result.rightMouseDown = input.IsMouseButtonDown(SDL_BUTTON_RIGHT);
    result.shiftDown = input.IsKeyDown(SDL_SCANCODE_LSHIFT) ||
        input.IsKeyDown(SDL_SCANCODE_RSHIFT);
    const bool ctrl = input.IsKeyDown(SDL_SCANCODE_LCTRL) ||
        input.IsKeyDown(SDL_SCANCODE_RCTRL);
    if (!ctrl) {
        const auto pressed = [&](SDL_Scancode key) {
            return input.IsKeyPressed(key) &&
                (input.GetPressModifiers(key) & SDL_KMOD_CTRL) == 0;
        };
        bool shiftedCommand = result.shiftDown;
        for (SDL_Scancode key : {SDL_SCANCODE_0, SDL_SCANCODE_KP_0,
                SDL_SCANCODE_1,
                SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
                SDL_SCANCODE_5, SDL_SCANCODE_6, SDL_SCANCODE_7,
                SDL_SCANCODE_8, SDL_SCANCODE_9, SDL_SCANCODE_B,
                SDL_SCANCODE_C, SDL_SCANCODE_N, SDL_SCANCODE_M}) {
            shiftedCommand |= (input.GetPressModifiers(key) & SDL_KMOD_SHIFT) != 0;
        }
        if (pressed(SDL_SCANCODE_F1)) result.practiceCommand = PracticeCommand::ThirdPerson;
        else if (pressed(SDL_SCANCODE_F2)) result.practiceCommand = PracticeCommand::CycleCockpitModel;
        else if (pressed(SDL_SCANCODE_F3)) result.practiceCommand = PracticeCommand::ToggleData;
        else if (pressed(SDL_SCANCODE_F5)) result.practiceCommand = PracticeCommand::FullStartup;
        else if (pressed(SDL_SCANCODE_F6)) result.practiceCommand = PracticeCommand::QuickStartup;
        else if (pressed(SDL_SCANCODE_F7)) result.practiceCommand = PracticeCommand::BossStartup;
        else if (pressed(SDL_SCANCODE_F8)) result.practiceCommand = PracticeCommand::SystemOffline;
        else if (shiftedCommand) {
            if (pressed(SDL_SCANCODE_1)) result.practiceCommand = PracticeCommand::SingleMissile;
            else if (pressed(SDL_SCANCODE_2)) result.practiceCommand = PracticeCommand::MissileLine;
            else if (pressed(SDL_SCANCODE_3)) result.practiceCommand = PracticeCommand::FloatingBombs;
            else if (pressed(SDL_SCANCODE_4)) result.practiceCommand = PracticeCommand::MixedProjectiles;
        } else {
            if (pressed(SDL_SCANCODE_1)) result.practiceCommand = PracticeCommand::MissileUnit;
            else if (pressed(SDL_SCANCODE_2)) result.practiceCommand = PracticeCommand::Bomber;
            else if (pressed(SDL_SCANCODE_3)) result.practiceCommand = PracticeCommand::ChargeUnit;
            else if (pressed(SDL_SCANCODE_4)) result.practiceCommand = PracticeCommand::MissileUnits;
            else if (pressed(SDL_SCANCODE_5)) result.practiceCommand = PracticeCommand::Bombers;
            else if (pressed(SDL_SCANCODE_6)) result.practiceCommand = PracticeCommand::ChargeUnits;
            else if (pressed(SDL_SCANCODE_7)) result.practiceCommand = PracticeCommand::MixedEnemies;
            else if (pressed(SDL_SCANCODE_8)) result.practiceCommand = PracticeCommand::BossCycleNeedles;
            else if (pressed(SDL_SCANCODE_9)) result.practiceCommand = PracticeCommand::BossConeCharge;
            else if (pressed(SDL_SCANCODE_0) || pressed(SDL_SCANCODE_KP_0))
                result.practiceCommand = PracticeCommand::BossSpawn;
            else if (pressed(SDL_SCANCODE_B)) result.practiceCommand = PracticeCommand::BossBoxer;
            else if (pressed(SDL_SCANCODE_C)) result.practiceCommand = PracticeCommand::BossNeedleCombo;
            else if (pressed(SDL_SCANCODE_N)) result.practiceCommand = PracticeCommand::BossFullBarrage;
            else if (pressed(SDL_SCANCODE_M)) result.practiceCommand = PracticeCommand::BossSpreadBurst;
        }
    }

    if (!input.HasFocus() || gamepad == nullptr || !SDL_GamepadConnected(gamepad)) {
        previousButtons_.fill(false);
        previousLeftTrigger_ = previousRightTrigger_ = false;
        previousGamepad_ = nullptr;
        return result;
    }
    if (gamepad != previousGamepad_) {
        previousButtons_.fill(false);
        previousLeftTrigger_ = previousRightTrigger_ = false;
        previousGamepad_ = gamepad;
    }
    const auto axis = [&](SDL_GamepadAxis id) {
        const float value = std::clamp(SDL_GetGamepadAxis(gamepad, id) / 32767.0f,
            -1.0f, 1.0f);
        constexpr float deadZone = 0.2f;
        return std::abs(value) <= deadZone ? 0.0f :
            std::copysign((std::abs(value) - deadZone) / (1.0f - deadZone), value);
    };
    const auto button = [&](SDL_GamepadButton id) {
        const auto index = static_cast<std::size_t>(id);
        const bool down = SDL_GetGamepadButton(gamepad, id);
        const ActionState state{down, down && !previousButtons_[index],
            !down && previousButtons_[index]};
        previousButtons_[index] = down;
        return state;
    };
    const ActionState south = button(SDL_GAMEPAD_BUTTON_SOUTH);
    const ActionState start = button(SDL_GAMEPAD_BUTTON_START);
    const ActionState rightStick = button(SDL_GAMEPAD_BUTTON_RIGHT_STICK);
    const ActionState leftShoulder = button(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER);
    const ActionState rightShoulder = button(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
    result.moveX = std::clamp(result.moveX + axis(SDL_GAMEPAD_AXIS_LEFTX), -1.0f, 1.0f);
    result.moveY = std::clamp(result.moveY + axis(SDL_GAMEPAD_AXIS_LEFTY), -1.0f, 1.0f);
    constexpr float lookRadiansPerSecond = 2.2f;
    constexpr float mouseSensitivity = 0.0025f;
    result.lookX = axis(SDL_GAMEPAD_AXIS_RIGHTX) * lookRadiansPerSecond *
        static_cast<float>(deltaSeconds) / mouseSensitivity;
    result.lookY = axis(SDL_GAMEPAD_AXIS_RIGHTY) * lookRadiansPerSecond *
        static_cast<float>(deltaSeconds) / mouseSensitivity;
    const bool leftTrigger = axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > 0.35f;
    const bool rightTrigger = axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > 0.35f;
    result.leftPunchKeyDown |= leftTrigger;
    result.rightPunchKeyDown |= rightTrigger;
    result.leftMouseDown |= leftShoulder.down;
    result.rightMouseDown |= rightShoulder.down;
    result.dodge.pressed |= south.pressed;
    result.dodge.down |= south.down;
    result.confirm.pressed |= south.pressed;
    result.confirm.down |= south.down;
    result.battleStart.pressed |= start.pressed;
    result.battleStart.down |= start.down;
    result.skip.pressed |= start.pressed;
    result.skip.down |= start.down;
    result.switchView.pressed |= rightStick.pressed;
    previousLeftTrigger_ = leftTrigger;
    previousRightTrigger_ = rightTrigger;

    return result;
}
