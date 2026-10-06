#include "Collision/Collision.h"

#include <cmath>
#include <cstdio>
#include <limits>

namespace {
bool Expect(bool condition, const char* description) {
    if (!condition) {
        std::printf("FAIL: %s\n", description);
    }
    return condition;
}

bool Near(float first, float second) {
    return std::abs(first - second) < 0.0001f;
}
}

int main() {
    bool success = true;
    const Collision::Sphere target{{0.0f, 0.0f, 0.0f}, 1.0f};

    success &= Expect(Collision::Intersects({{2.0f, 0.0f, 0.0f}, 1.0f}, target) &&
        !Collision::Intersects({{2.01f, 0.0f, 0.0f}, 1.0f}, target),
        "Touching spheres intersect, separated spheres do not");
    success &= Expect(!Collision::Intersects({{0.0f, 0.0f, 0.0f}, -1.0f}, target),
        "An invalid radius does not create a collision");

    const Collision::SweepHit crossing = Collision::SweepSphereAgainstSphere(
        {-5.0f, 0.0f, 0.0f}, {5.0f, 0.0f, 0.0f}, 0.5f, target);
    success &= Expect(crossing.didHit && Near(crossing.time, 0.35f) &&
        !Collision::Intersects({{-5.0f, 0.0f, 0.0f}, 0.5f}, target) &&
        !Collision::Intersects({{5.0f, 0.0f, 0.0f}, 0.5f}, target),
        "A sweep catches a hit between two nonoverlapping endpoints");
    success &= Expect(Collision::SweepSphereAgainstSphere(
        {0.0f, 0.0f, 0.0f}, {5.0f, 0.0f, 0.0f}, 0.5f, target).didHit,
        "A sweep beginning inside the target hits immediately");
    success &= Expect(!Collision::SweepSphereAgainstSphere(
        {-5.0f, 0.0f, 0.0f}, {-5.0f, 0.0f, 0.0f}, 0.5f, target).didHit,
        "A stationary sphere outside the target does not hit");
    success &= Expect(Collision::SweepSphereAgainstSphere(
        {-5.0f, 1.5f, 0.0f}, {5.0f, 1.5f, 0.0f}, 0.5f, target).didHit &&
        !Collision::SweepSphereAgainstSphere(
            {-5.0f, 1.51f, 0.0f}, {5.0f, 1.51f, 0.0f}, 0.5f, target).didHit,
        "A tangent sweep hits while a nearby parallel sweep misses");
    const float invalid = std::numeric_limits<float>::quiet_NaN();
    success &= Expect(!Collision::SweepSphereAgainstSphere(
        {invalid, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, 0.5f, target).didHit,
        "Nonfinite positions cannot create a hit");

    if (success) {
        std::puts("PASS: sphere overlap and continuous sphere sweep");
    }
    return success ? 0 : 1;
}
