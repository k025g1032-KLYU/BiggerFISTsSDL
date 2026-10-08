#pragma once

#include "World/ModelScene.h"

#include <array>

class FirstPersonGame;
class PunchArm;
class CombatSession;
class BattleAssetCatalog;
struct BattleVisualEvent;
enum class BattleVisualEffect;

class FirstPersonScenePresenter {
public:
    explicit FirstPersonScenePresenter(ModelScene initialScene);
    void Sync(const FirstPersonGame& game, float interpolationAlpha = 1.0f);
    void SyncBattle(const CombatSession& battle, const BattleAssetCatalog& assets,
        float interpolationAlpha = 1.0f);
    void QueueVisualEvents(const std::vector<BattleVisualEvent>& events);
    void AdvanceVisualTime(double seconds);
    void SyncTutorial(const FirstPersonGame& game, const BattleAssetCatalog& assets,
        bool showTargets);
    void ClearDynamic();
    void SyncCockpit(const FirstPersonGame& game, const BattleAssetCatalog& assets,
        bool visible, float startupRemainingRatio);
    const ModelScene& GetScene() const { return scene_; }

private:
    void SyncEnvironment(const FirstPersonGame& game,
        const BattleAssetCatalog& assets, bool tutorial);
    void SyncRocketFlames(const FirstPersonGame& game,
        const BattleAssetCatalog& assets);
    void UpdateFistPose(const char* id, float sideSign,
        const FirstPersonGame& game, const PunchArm& arm);
    void UpdateUpperArmPose(const char* id, float sideSign,
        const FirstPersonGame& game, const PunchArm& arm);

    ModelScene scene_;
    float interpolationAlpha_ = 1.0f;
    std::vector<ModelInstance> initialModels_;
    struct ActiveVisualEffect {
        int id = 0;
        BattleVisualEffect kind;
        Vector3 position{};
        float scale = 1.0f;
        int shieldLayers = 0;
        double startTime = 0.0;
    };
    std::vector<ActiveVisualEffect> visualEffects_;
    int nextVisualEffectId_ = 1;
    double visualTime_ = 0.0;
    struct CloudPose {
        Vector3 position{};
        Vector3 scale{};
        float yaw = 0.0f;
        float speed = 0.0f;
    };
    std::array<CloudPose, 12> clouds_{};
    std::uint32_t cloudRandomState_ = 0x63A9D2E1u;
    double lastCloudTime_ = 0.0;
    bool cloudsInitialized_ = false;
};
