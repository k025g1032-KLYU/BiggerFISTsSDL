#include "Rendering/FirstPersonScenePresenter.h"

#include "Game/FirstPersonGame.h"
#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

FirstPersonScenePresenter::FirstPersonScenePresenter(ModelScene initialScene)
    : scene_(std::move(initialScene)), initialModels_(scene_.models) {
}

void FirstPersonScenePresenter::Sync(const FirstPersonGame& game) {
    scene_.cameraPosition = game.GetCamera().GetPosition();
    scene_.cameraTarget = game.GetCamera().GetTarget();
    scene_.elapsedSeconds = game.GetElapsedSeconds();
    scene_.warmBackground = game.IsWarmBackground();

    std::erase_if(scene_.models, [&](const ModelInstance& model) {
        if (model.id != "target" && !model.id.starts_with("target_")) {
            return false;
        }
        const auto target = std::find_if(game.GetTargets().begin(), game.GetTargets().end(),
            [&](const FirstPersonTarget& item) { return item.id == model.id; });
        return target == game.GetTargets().end() || target->hitPoints <= 0;
    });

    for (const FirstPersonTarget& target : game.GetTargets()) {
        if (target.hitPoints <= 0) {
            continue;
        }
        ModelInstance* model = FindModelById(scene_, target.id);
        if (model == nullptr) {
            auto original = std::find_if(initialModels_.begin(), initialModels_.end(),
                [&](const ModelInstance& item) { return item.id == target.id; });
            if (original == initialModels_.end()) {
                original = std::find_if(initialModels_.begin(), initialModels_.end(),
                    [](const ModelInstance& item) { return item.id == "target"; });
            }
            if (original == initialModels_.end()) {
                continue;
            }
            scene_.models.insert(scene_.models.begin(), *original);
            model = &scene_.models.front();
            model->id = target.id;
        }
        model->position = target.position;
    }

    UpdateFistPose("left_fist", -1.0f, game, game.GetLeftArm());
    UpdateFistPose("right_fist", 1.0f, game, game.GetRightArm());
}

void FirstPersonScenePresenter::UpdateFistPose(const char* id, float sideSign,
    const FirstPersonGame& game, const PunchArm& arm) {
    ModelInstance* fist = FindModelById(scene_, id);
    if (fist == nullptr) {
        return;
    }
    const Vector3& playerPosition = game.GetPlayer().GetPosition();
    const float bodyYaw = game.GetPlayer().GetBodyYaw();
    const float sine = std::sin(bodyYaw);
    const float cosine = std::cos(bodyYaw);
    const Vector3& fistPosition = arm.GetFistPosition();
    if (arm.IsFistAway()) {
        fist->position = fistPosition;
        fist->initialXDegrees = arm.GetRocketPosePitch() *
            180.0f / std::numbers::pi_v<float>;
        fist->initialYDegrees = arm.GetRocketPoseYaw() *
            180.0f / std::numbers::pi_v<float>;
        fist->initialZDegrees = 0.0f;
        return;
    }
    const float sideOffset = sideSign * FirstPersonTuning::kShoulderHalfWidth;
    const Vector3 shoulder{
        playerPosition.x + cosine * sideOffset,
        playerPosition.y + FirstPersonTuning::kShoulderHeight,
        playerPosition.z - sine * sideOffset
    };
    const float horizontalX = fistPosition.x - shoulder.x;
    const float horizontalZ = fistPosition.z - shoulder.z;
    const float halfHorizontalDistance = 0.5f * std::hypot(horizontalX, horizontalZ);
    const float upperArmLength = FirstPersonTuning::kUpperArmFixedLength;
    const float elbowDrop = std::sqrt(std::max(0.0f,
        upperArmLength * upperArmLength - halfHorizontalDistance * halfHorizontalDistance));
    const Vector3 elbow{
        (shoulder.x + fistPosition.x) * 0.5f,
        shoulder.y - elbowDrop,
        (shoulder.z + fistPosition.z) * 0.5f
    };
    const float forearmX = fistPosition.x - elbow.x;
    const float forearmY = fistPosition.y - elbow.y;
    const float forearmZ = fistPosition.z - elbow.z;
    fist->position = fistPosition;
    fist->initialXDegrees = -std::atan2(forearmY, std::hypot(forearmX, forearmZ)) *
        180.0f / std::numbers::pi_v<float>;
    fist->initialYDegrees = std::atan2(forearmX, forearmZ) *
        180.0f / std::numbers::pi_v<float>;
    fist->initialZDegrees = (arm.GetState() == PunchArmState::Ready ||
        arm.GetState() == PunchArmState::Recovery) ? -sideSign * 90.0f : 0.0f;
}
