#pragma once

#include "World/ModelScene.h"

#include <optional>

class FirstPersonGame;
class PunchArm;

class FirstPersonScenePresenter {
public:
    explicit FirstPersonScenePresenter(ModelScene initialScene);
    void Sync(const FirstPersonGame& game);
    const ModelScene& GetScene() const { return scene_; }

private:
    void UpdateFistPose(const char* id, float sideSign,
        const FirstPersonGame& game, const PunchArm& arm);

    ModelScene scene_;
    std::optional<ModelInstance> initialTarget_;
};
