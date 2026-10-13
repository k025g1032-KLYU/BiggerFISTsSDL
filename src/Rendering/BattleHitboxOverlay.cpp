#include "Rendering/BattleHitboxOverlay.h"

#include "Gameplay/Combat/CombatSession.h"
#include "Rendering/UiRenderer.h"
#include "World/ModelScene.h"

#include <cmath>
#include <numbers>

namespace {
struct ScreenPoint {
    float x = 0.0f;
    float y = 0.0f;
    float depth = 0.0f;
    bool visible = false;
};

ScreenPoint Project(const Vector3& point, const Matrix4x4& matrix) {
    const float input[4]{point.x, point.y, point.z, 1.0f};
    float clip[4]{};
    for (int column = 0; column < 4; ++column)
        for (int row = 0; row < 4; ++row)
            clip[column] += input[row] * matrix.elements[row][column];
    if (clip[3] <= 0.00001f) return {};
    const float x = clip[0] / clip[3];
    const float y = clip[1] / clip[3];
    const float z = clip[2] / clip[3];
    return {(x + 1.0f) * 640.0f, (1.0f - y) * 360.0f, clip[3],
        x >= -1.2f && x <= 1.2f && y >= -1.2f && y <= 1.2f &&
        z >= 0.0f && z <= 1.0f};
}

void Line(UiDrawList& list, const ScreenPoint& from,
    const ScreenPoint& to, UiColor color) {
    if (!from.visible || !to.visible) return;
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    const float length = std::hypot(dx, dy);
    if (length < 0.5f) return;
    list.RotatedRectangle((from.x + to.x) * 0.5f,
        (from.y + to.y) * 0.5f, length, 2.0f,
        std::atan2(dy, dx), color);
}

UiColor Color(BattleHitboxKind kind) {
    switch (kind) {
    case BattleHitboxKind::Enemy: return {0.25f, 1.0f, 0.35f, 0.94f};
    case BattleHitboxKind::Projectile: return {1.0f, 0.75f, 0.15f, 0.94f};
    case BattleHitboxKind::BossCore: return {0.30f, 0.85f, 1.0f, 0.94f};
    case BattleHitboxKind::BossAttack: return {1.0f, 0.20f, 0.32f, 0.94f};
    case BattleHitboxKind::PlayerAttack: return {0.95f, 0.95f, 1.0f, 0.94f};
    }
    return {};
}
}

void BuildBattleHitboxOverlay(const CombatSession& battle,
    const ModelScene& scene, UiDrawList& list) {
    Matrix4x4 view{}, projection{};
    if (!TryMakeLookAtLHMatrix(scene.cameraPosition, scene.cameraTarget,
            scene.cameraUp, view) ||
        !TryMakePerspectiveLHMatrix(scene.verticalFovDegrees *
            std::numbers::pi_v<float> / 180.0f, 1280.0f / 720.0f,
            scene.nearPlane, scene.farPlane, projection)) return;
    const Matrix4x4 matrix = MultiplyMatrices(view, projection);
    list.Rectangle(10.0f, 10.0f, 314.0f, 27.0f,
        {0.0f, 0.0f, 0.0f, 0.70f});
    list.Text("HITBOXES  F9: TOGGLE", 16.0f, 15.0f, 1.0f,
        {0.90f, 0.95f, 1.0f, 1.0f});
    for (const BattleHitbox& hitbox : battle.GetHitboxes()) {
        const ScreenPoint center = Project(hitbox.center, matrix);
        if (!center.visible) continue;
        const ScreenPoint previous = Project(hitbox.previousCenter, matrix);
        const UiColor color = Color(hitbox.kind);
        const float radius = hitbox.radius * projection.elements[1][1] *
            360.0f / center.depth;
        if (!std::isfinite(radius) || radius <= 0.0f || radius > 5000.0f)
            continue;
        for (int segment = 0; segment < 20; ++segment) {
            const float first = static_cast<float>(segment) *
                2.0f * std::numbers::pi_v<float> / 20.0f;
            const float second = static_cast<float>(segment + 1) *
                2.0f * std::numbers::pi_v<float> / 20.0f;
            Line(list, {center.x + std::cos(first) * radius,
                    center.y + std::sin(first) * radius, center.depth, true},
                {center.x + std::cos(second) * radius,
                    center.y + std::sin(second) * radius, center.depth, true},
                color);
        }
        list.Rectangle(center.x - 2.0f, center.y - 2.0f, 4.0f, 4.0f,
            color);
        Line(list, previous, center, {color.r, color.g, color.b, 0.55f});
        const Vector3 tip{hitbox.center.x + hitbox.facing.x *
                hitbox.radius * 2.0f,
            hitbox.center.y + hitbox.facing.y * hitbox.radius * 2.0f,
            hitbox.center.z + hitbox.facing.z * hitbox.radius * 2.0f};
        Line(list, center, Project(tip, matrix), color);
    }
}
