#pragma once

struct ActionState {
    bool down = false;
    bool pressed = false;
    bool released = false;
};

enum class PracticeCommand {
    None, MissileUnit, Bomber, ChargeUnit, MissileUnits, Bombers,
    ChargeUnits, MixedEnemies, BossSpawn, BossCycleNeedles,
    BossConeCharge, BossBoxer, BossNeedleCombo, BossFullBarrage,
    BossSpreadBurst, SingleMissile, MissileLine, FloatingBombs,
    MixedProjectiles, ThirdPerson, CycleCockpitModel, ToggleData,
    FullStartup, QuickStartup, BossStartup, SystemOffline
};

struct GameInput {
    float moveX = 0.0f;
    float moveY = 0.0f;

    ActionState changeBackground{};
    ActionState quit{};
    ActionState resetTarget{};
    ActionState battleStart{};
    ActionState confirm{};
    ActionState skip{};
    ActionState skipPhase{};
    ActionState switchView{};
    ActionState dodge{};
    ActionState testProjectileHit{};
    ActionState testChargeHit{};
    ActionState testBurstHit{};
    PracticeCommand practiceCommand = PracticeCommand::None;
    bool leftPunchKeyDown = false;
    bool rightPunchKeyDown = false;
    bool leftMouseDown = false;
    bool rightMouseDown = false;
    bool shiftDown = false;
    float lookX = 0.0f;
    float lookY = 0.0f;
};
