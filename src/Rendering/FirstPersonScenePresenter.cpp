#include "Rendering/FirstPersonScenePresenter.h"

#include "Game/FirstPersonGame.h"
#include "Gameplay/Player/FirstPersonTuning.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

namespace {
Vector3 ShoulderPosition(const FirstPersonGame& game, float sideSign) {
    const Vector3& player = game.GetPlayer().GetPosition();
    const float yaw = game.GetPlayer().GetBodyYaw();
    const float offset = sideSign * FirstPersonTuning::kShoulderHalfWidth;
    return {player.x + std::cos(yaw) * offset,
        player.y + FirstPersonTuning::kShoulderHeight,
        player.z - std::sin(yaw) * offset};
}

Vector3 ElbowPosition(const FirstPersonGame& game, const PunchArm& arm,
    float sideSign, const Vector3& shoulder) {
    const float yaw = game.GetPlayer().GetBodyYaw();
    const float sine = std::sin(yaw);
    const float cosine = std::cos(yaw);
    const Vector3& fist = arm.GetFistPosition();
    const bool rocket = arm.GetAttackType() == PunchArmAttackType::RocketPunch;
    if (rocket && (arm.IsFistAway() || arm.IsChargingOrCharged())) {
        const Vector3 target = arm.IsFistAway()
            ? Vector3{shoulder.x + sine * FirstPersonTuning::kRocketChargeFinalForwardOffset,
                shoulder.y,
                shoulder.z + cosine * FirstPersonTuning::kRocketChargeFinalForwardOffset}
            : fist;
        Vector3 rocketElbow{
            shoulder.x + (target.x - shoulder.x) *
                FirstPersonTuning::kRocketExtendedElbowRatio,
            shoulder.y + (target.y - shoulder.y) *
                FirstPersonTuning::kRocketExtendedElbowRatio,
            shoulder.z + (target.z - shoulder.z) *
                FirstPersonTuning::kRocketExtendedElbowRatio};
        if (arm.GetState() != PunchArmState::Returning ||
            fist.y - shoulder.y > FirstPersonTuning::kRocketReloadUpperArmFollowStartHeight) {
            return rocketElbow;
        }
        const float ratio = std::clamp(1.0f - std::max(0.0f, fist.y - shoulder.y) /
            FirstPersonTuning::kRocketReloadUpperArmFollowStartHeight, 0.0f, 1.0f);
        const float eased = ratio * ratio * (3.0f - 2.0f * ratio);
        const Vector3 idleTarget{(shoulder.x + fist.x) * 0.5f,
            shoulder.y, (shoulder.z + fist.z) * 0.5f};
        rocketElbow.x += (idleTarget.x - rocketElbow.x) * eased;
        rocketElbow.z += (idleTarget.z - rocketElbow.z) * eased;
        const float dx = rocketElbow.x - shoulder.x;
        const float dz = rocketElbow.z - shoulder.z;
        rocketElbow.y = shoulder.y - std::sqrt(std::max(0.0f,
            FirstPersonTuning::kUpperArmFixedLength *
            FirstPersonTuning::kUpperArmFixedLength - dx * dx - dz * dz));
        return rocketElbow;
    }
    Vector3 elbow{(shoulder.x + fist.x) * 0.5f,
        shoulder.y, (shoulder.z + fist.z) * 0.5f};
    if (arm.IsChargingOrCharged() &&
        arm.GetAttackType() == PunchArmAttackType::Punch) {
        const float outward = sideSign * FirstPersonTuning::kPunchChargeElbowOutwardOffset;
        elbow.x += cosine * outward;
        elbow.z -= sine * outward;
    }
    const float dx = elbow.x - shoulder.x;
    const float dz = elbow.z - shoulder.z;
    elbow.y -= std::sqrt(std::max(0.0f,
        FirstPersonTuning::kUpperArmFixedLength *
        FirstPersonTuning::kUpperArmFixedLength - dx * dx - dz * dz));
    return elbow;
}
}

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
    UpdateUpperArmPose("left_upper_arm", -1.0f, game, game.GetLeftArm());
    UpdateUpperArmPose("right_upper_arm", 1.0f, game, game.GetRightArm());
}

void FirstPersonScenePresenter::UpdateFistPose(const char* id, float sideSign,
    const FirstPersonGame& game, const PunchArm& arm) {
    ModelInstance* fist = FindModelById(scene_, id);
    if (fist == nullptr) {
        return;
    }
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
    const Vector3 shoulder = ShoulderPosition(game, sideSign);
    const Vector3 elbow = ElbowPosition(game, arm, sideSign, shoulder);
    const float forearmX = fistPosition.x - elbow.x;
    const float forearmY = fistPosition.y - elbow.y;
    const float forearmZ = fistPosition.z - elbow.z;
    const Vector3 visualShake = arm.GetChargedVisualOffset();
    fist->position = {fistPosition.x + visualShake.x,
        fistPosition.y + visualShake.y,
        fistPosition.z + visualShake.z};
    fist->initialXDegrees = -std::atan2(forearmY, std::hypot(forearmX, forearmZ)) *
        180.0f / std::numbers::pi_v<float>;
    fist->initialYDegrees = std::atan2(forearmX, forearmZ) *
        180.0f / std::numbers::pi_v<float>;
    fist->initialZDegrees = (arm.GetState() == PunchArmState::Ready ||
        arm.GetState() == PunchArmState::Recovery) ? -sideSign * 90.0f : 0.0f;
}

void FirstPersonScenePresenter::UpdateUpperArmPose(const char* id, float sideSign,
    const FirstPersonGame& game, const PunchArm& arm) {
    ModelInstance* model = FindModelById(scene_, id);
    if (model == nullptr) {
        return;
    }
    const Vector3 shoulder = ShoulderPosition(game, sideSign);
    const Vector3 elbow = ElbowPosition(game, arm, sideSign, shoulder);
    Vector3 direction{};
    if (!TryNormalizeVector(SubtractVectors(elbow, shoulder), direction)) {
        return;
    }
    model->position = {
        shoulder.x + direction.x * FirstPersonTuning::kUpperArmModelShoulderToOriginDistance,
        shoulder.y + direction.y * FirstPersonTuning::kUpperArmModelShoulderToOriginDistance,
        shoulder.z + direction.z * FirstPersonTuning::kUpperArmModelShoulderToOriginDistance
    };
    model->scale = FirstPersonTuning::kUpperArmModelScale;
    model->initialXDegrees = -std::asin(std::clamp(direction.y, -1.0f, 1.0f)) *
        180.0f / std::numbers::pi_v<float>;
    model->initialYDegrees = std::atan2(direction.x, direction.z) *
        180.0f / std::numbers::pi_v<float>;
    model->initialZDegrees = 0.0f;
}
