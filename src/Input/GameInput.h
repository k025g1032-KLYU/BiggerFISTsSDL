#pragma once

struct ActionState {
    bool down = false;
    bool pressed = false;
    bool released = false;
};

struct GameInput {
    float moveX = 0.0f;
    float moveY = 0.0f;

    ActionState changeBackground{};
    ActionState quit{};
    ActionState resetTarget{};
    bool leftPunchKeyDown = false;
    bool rightPunchKeyDown = false;
    bool leftMouseDown = false;
    bool rightMouseDown = false;
    bool shiftDown = false;
};
