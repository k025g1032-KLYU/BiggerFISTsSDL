#include "Collision/Collision.h"

#include <algorithm>
#include <cmath>

namespace Collision {
namespace {
bool IsValid(const Sphere& sphere) {
    return std::isfinite(sphere.center.x) && std::isfinite(sphere.center.y) &&
        std::isfinite(sphere.center.z) && std::isfinite(sphere.radius) &&
        sphere.radius >= 0.0f;
}

float Dot(const Vector3& first, const Vector3& second) {
    return first.x * second.x + first.y * second.y + first.z * second.z;
}
}

bool Intersects(const Sphere& first, const Sphere& second) {
    if (!IsValid(first) || !IsValid(second)) {
        return false;
    }
    const Vector3 difference{
        first.center.x - second.center.x,
        first.center.y - second.center.y,
        first.center.z - second.center.z
    };
    const float radiusSum = first.radius + second.radius;
    return Dot(difference, difference) <= radiusSum * radiusSum;
}

SweepHit SweepSphereAgainstSphere(const Vector3& start, const Vector3& end,
    float movingRadius, const Sphere& target) {
    if (!IsValid({start, movingRadius}) || !IsValid({end, movingRadius}) ||
        !IsValid(target)) {
        return {};
    }
    const Vector3 fromTarget{
        start.x - target.center.x,
        start.y - target.center.y,
        start.z - target.center.z
    };
    const Vector3 movement{
        end.x - start.x,
        end.y - start.y,
        end.z - start.z
    };
    const float radiusSum = movingRadius + target.radius;
    const float c = Dot(fromTarget, fromTarget) - radiusSum * radiusSum;
    if (c <= 0.0f) {
        return {true, 0.0f};
    }
    const float a = Dot(movement, movement);
    if (a <= 0.000000000001f) {
        return {};
    }
    const float b = Dot(fromTarget, movement);
    const float discriminant = b * b - a * c;
    if (discriminant < 0.0f) {
        return {};
    }
    const float time = (-b - std::sqrt(discriminant)) / a;
    if (time < 0.0f || time > 1.0f) {
        return {};
    }
    return {true, std::clamp(time, 0.0f, 1.0f)};
}
}
