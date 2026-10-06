#include "Rendering/FirstPersonScenePresenter.h"

#include "Game/FirstPersonGame.h"
#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

FirstPersonScenePresenter::FirstPersonScenePresenter(ModelScene initialScene)
    : scene_(std::move(initialScene)) {
    if (const ModelInstance* target = FindModelById(scene_, "target")) {
        initialTarget_ = *target;
    }
}

void FirstPersonScenePresenter::Sync(const FirstPersonGame& game) {
    scene_.cameraPosition = game.GetCamera().GetPosition();
    scene_.cameraTarget = game.GetCamera().GetTarget();
    scene_.elapsedSeconds = game.GetElapsedSeconds();
    scene_.warmBackground = game.IsWarmBackground();

    if (game.GetTargetHitPoints() == 0) {
        scene_.models.erase(std::remove_if(scene_.models.begin(), scene_.models.end(),
            [](const ModelInstance& model) { return model.id == "target"; }),
            scene_.models.end());
    } else if (initialTarget_ && FindModelById(scene_, "target") == nullptr) {
        scene_.models.insert(scene_.models.begin(), *initialTarget_);
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
