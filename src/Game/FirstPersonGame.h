#pragma once

#include "Data/GameConfig.h"
#include "Gameplay/Player/PlayerMovement.h"
#include "Gameplay/Player/PunchArm.h"
#include "Input/GameInput.h"
#include "World/FirstPersonCamera.h"
#include "World/ModelScene.h"

#include <optional>

class FirstPersonGame {
public:
    FirstPersonGame(const GameConfig& config, ModelScene initialScene);
    void Update(double deltaSeconds, const GameInput& input,
        float mouseDeltaX, float mouseDeltaY);
    const ModelScene& GetScene() const { return scene_; }
    const PlayerMovement& GetPlayer() const { return player_; }
    const FirstPersonCamera& GetCamera() const { return camera_; }
    const PunchArm& GetLeftArm() const { return leftArm_; }
    const PunchArm& GetRightArm() const { return rightArm_; }
    int GetTargetHitPoints() const { return targetHitPoints_; }
    bool DidDestroyTargetThisFrame() const { return targetDestroyedThisFrame_; }

private:
    void UpdateFistPoses();
    void ProcessTargetHits();
    void ResetTarget();

    ModelScene scene_;
    PlayerMovement player_;
    FirstPersonCamera camera_;
    PunchArm leftArm_{PunchArmSide::Left};
    PunchArm rightArm_{PunchArmSide::Right};
    std::optional<ModelInstance> initialTarget_;
    int targetHitPoints_ = 0;
    bool targetDestroyedThisFrame_ = false;
    float maxArmDeltaTime_ = 0.05f;
};
