#pragma once

#include "World/ModelScene.h"

class FirstPersonGame;
class PunchArm;
class CombatSession;
class BattleAssetCatalog;

class FirstPersonScenePresenter {
public:
    explicit FirstPersonScenePresenter(ModelScene initialScene);
    void Sync(const FirstPersonGame& game);
    void SyncBattle(const CombatSession& battle, const BattleAssetCatalog& assets);
    const ModelScene& GetScene() const { return scene_; }

private:
    void UpdateFistPose(const char* id, float sideSign,
        const FirstPersonGame& game, const PunchArm& arm);
    void UpdateUpperArmPose(const char* id, float sideSign,
        const FirstPersonGame& game, const PunchArm& arm);

    ModelScene scene_;
    std::vector<ModelInstance> initialModels_;
};
