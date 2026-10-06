#pragma once

#include "Math/MathTypes.h"

namespace Collision {

struct Sphere {
    Vector3 center{};
    float radius = 0.0f;
};

struct SweepHit {
    bool didHit = false;
    float time = 0.0f;
};

bool Intersects(const Sphere& first, const Sphere& second);
SweepHit SweepSphereAgainstSphere(const Vector3& start, const Vector3& end,
    float movingRadius, const Sphere& target);

}
