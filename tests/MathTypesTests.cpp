#include "Math/MathTypes.h"

#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <numbers>

namespace {
int failures = 0;

void ExpectTrue(bool condition, const char* name) {
    if (!condition) {
        std::printf("FAIL: %s\n", name);
        ++failures;
    }
}

void ExpectNear(float actual, float expected, const char* name) {
    if (!std::isfinite(actual) || std::abs(actual - expected) > 0.0001f) {
        std::printf("FAIL: %s: expected %.6f, got %.6f\n", name, expected, actual);
        ++failures;
    }
}

void ExpectPoint(const Matrix4x4& matrix, float x, float y, float z,
    float expectedX, float expectedY, float expectedZ, const char* name) {
    ExpectNear(x * matrix.elements[0][0] + y * matrix.elements[1][0] +
        z * matrix.elements[2][0] + matrix.elements[3][0], expectedX, name);
    ExpectNear(x * matrix.elements[0][1] + y * matrix.elements[1][1] +
        z * matrix.elements[2][1] + matrix.elements[3][1], expectedY, name);
    ExpectNear(x * matrix.elements[0][2] + y * matrix.elements[1][2] +
        z * matrix.elements[2][2] + matrix.elements[3][2], expectedZ, name);
    ExpectNear(x * matrix.elements[0][3] + y * matrix.elements[1][3] +
        z * matrix.elements[2][3] + matrix.elements[3][3], 1.0f, name);
}

void ExpectProjectedPoint(const Matrix4x4& matrix, float x, float y, float z,
    float expectedX, float expectedY, float expectedZ, const char* name) {
    const float w = x * matrix.elements[0][3] + y * matrix.elements[1][3] +
        z * matrix.elements[2][3] + matrix.elements[3][3];
    ExpectTrue(std::isfinite(w) && w > 0.0f, name);
    if (!std::isfinite(w) || w <= 0.0f) return;
    ExpectNear((x * matrix.elements[0][0] + y * matrix.elements[1][0] +
        z * matrix.elements[2][0] + matrix.elements[3][0]) / w, expectedX, name);
    ExpectNear((x * matrix.elements[0][1] + y * matrix.elements[1][1] +
        z * matrix.elements[2][1] + matrix.elements[3][1]) / w, expectedY, name);
    ExpectNear((x * matrix.elements[0][2] + y * matrix.elements[1][2] +
        z * matrix.elements[2][2] + matrix.elements[3][2]) / w, expectedZ, name);
}
}

int main() {
    const Matrix4x4 identity = MakeIdentityMatrix();
    ExpectPoint(identity, 2.0f, -3.0f, 4.0f, 2.0f, -3.0f, 4.0f, "identity");
    const Matrix4x4 translation = MakeTranslationMatrix(3.0f, -2.0f, 5.0f);
    ExpectPoint(translation, 1.0f, 2.0f, 3.0f, 4.0f, 0.0f, 8.0f, "translation");
    const Matrix4x4 scale = MakeScaleMatrix(2.0f, 3.0f, 4.0f);
    ExpectPoint(scale, 1.0f, -2.0f, 3.0f, 2.0f, -6.0f, 12.0f, "scale");
    const Matrix4x4 rotation = MakeRotationZMatrix(std::numbers::pi_v<float> / 2.0f);
    ExpectPoint(rotation, 1.0f, 0.0f, 2.0f, 0.0f, 1.0f, 2.0f, "positive quarter turn");
    ExpectPoint(MakeRotationZMatrix(-std::numbers::pi_v<float> / 2.0f),
        1.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, "negative quarter turn");

    const Matrix4x4 combined = MultiplyMatrices(MultiplyMatrices(scale, rotation), translation);
    ExpectPoint(combined, 1.0f, 2.0f, 3.0f, -3.0f, 0.0f, 17.0f, "scale then rotate then translate");
    ExpectPoint(MultiplyMatrices(translation, scale), 1.0f, 2.0f, 3.0f,
        8.0f, 0.0f, 32.0f, "translation before scale has a different result");
    ExpectPoint(MultiplyMatrices(identity, combined), 1.0f, 2.0f, 3.0f,
        -3.0f, 0.0f, 17.0f, "left identity multiplication");
    ExpectPoint(MultiplyMatrices(combined, identity), 1.0f, 2.0f, 3.0f,
        -3.0f, 0.0f, 17.0f, "right identity multiplication");

    ExpectPoint(MakeAspectCorrectionMatrix(2.0f), 0.6f, 0.6f, 0.0f,
        0.3f, 0.6f, 0.0f, "wide output correction");
    ExpectPoint(MakeAspectCorrectionMatrix(0.5f), 0.6f, 0.6f, 0.0f,
        0.6f, 0.3f, 0.0f, "tall output correction");
    for (const float aspect : { 1.0f, 0.0f, -1.0f,
            std::numeric_limits<float>::infinity(), std::numeric_limits<float>::quiet_NaN() }) {
        ExpectPoint(MakeAspectCorrectionMatrix(aspect), 2.0f, -3.0f, 4.0f,
            2.0f, -3.0f, 4.0f, "square or invalid aspect uses identity");
    }

    ExpectPoint(MakeRotationXMatrix(std::numbers::pi_v<float> / 2.0f),
        0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, "X rotation");
    ExpectPoint(MakeRotationYMatrix(std::numbers::pi_v<float> / 2.0f),
        0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, "Y rotation");
    ExpectNear(DotVectors({ 1.0f, 2.0f, 3.0f }, { 4.0f, -5.0f, 6.0f }), 12.0f, "dot product");
    const Vector3 cross = CrossVectors({ 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f });
    ExpectNear(cross.x, 0.0f, "cross X");
    ExpectNear(cross.y, 0.0f, "cross Y");
    ExpectNear(cross.z, 1.0f, "cross Z");
    Vector3 normalized{ 0.0f, 3.0f, 4.0f };
    ExpectTrue(TryNormalizeVector(normalized, normalized), "in-place normalization");
    ExpectNear(normalized.y, 0.6f, "normalized Y");
    ExpectNear(normalized.z, 0.8f, "normalized Z");
    ExpectTrue(!TryNormalizeVector({}, normalized), "zero vector rejected");
    ExpectTrue(!TryNormalizeVector({ std::numeric_limits<float>::infinity(), 0.0f, 0.0f }, normalized),
        "nonfinite vector rejected");

    Matrix4x4 view{};
    ExpectTrue(TryMakeLookAtLHMatrix({ 0.0f, 0.0f, -3.0f }, {}, { 0.0f, 1.0f, 0.0f }, view), "fixed camera");
    ExpectPoint(view, 0.0f, 0.0f, -3.0f, 0.0f, 0.0f, 0.0f, "camera maps to origin");
    ExpectPoint(view, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 3.0f, "target is in front");
    ExpectTrue(TryMakeLookAtLHMatrix({ 3.0f, 0.0f, 0.0f }, {}, { 0.0f, 2.0f, 0.0f }, view), "side camera");
    ExpectPoint(view, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 3.0f, "side camera target");
    ExpectPoint(view, 3.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, "side camera right direction");
    ExpectTrue(!TryMakeLookAtLHMatrix({}, {}, { 0.0f, 1.0f, 0.0f }, view), "coincident camera rejected");
    ExpectTrue(!TryMakeLookAtLHMatrix({}, { 0.0f, 0.0f, 1.0f }, { 0.0f, 0.0f, 1.0f }, view),
        "parallel up rejected");

    Matrix4x4 projection{};
    const float halfPi = std::numbers::pi_v<float> / 2.0f;
    ExpectTrue(TryMakePerspectiveLHMatrix(halfPi, 2.0f, 1.0f, 11.0f, projection), "valid projection");
    ExpectProjectedPoint(projection, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, "near maps to depth zero");
    ExpectProjectedPoint(projection, 0.0f, 0.0f, 11.0f, 0.0f, 0.0f, 1.0f, "far maps to depth one");
    ExpectProjectedPoint(projection, 2.0f, 1.0f, 2.0f, 0.5f, 0.5f, 0.55f, "perspective and aspect");
    ExpectProjectedPoint(projection, 2.0f, 1.0f, 4.0f, 0.25f, 0.25f, 0.825f, "distant point appears smaller");
    ExpectTrue(!TryMakePerspectiveLHMatrix(0.0f, 1.0f, 1.0f, 11.0f, projection), "zero FOV rejected");
    ExpectTrue(!TryMakePerspectiveLHMatrix(std::numbers::pi_v<float>, 1.0f, 1.0f, 11.0f, projection), "180 FOV rejected");
    ExpectTrue(!TryMakePerspectiveLHMatrix(halfPi, 0.0f, 1.0f, 11.0f, projection), "zero aspect rejected");
    ExpectTrue(!TryMakePerspectiveLHMatrix(halfPi, 1.0f, 0.0f, 11.0f, projection), "zero near rejected");
    ExpectTrue(!TryMakePerspectiveLHMatrix(halfPi, 1.0f, 11.0f, 11.0f, projection), "equal near and far rejected");
    ExpectTrue(!TryMakePerspectiveLHMatrix(halfPi, 1.0f, 12.0f, 11.0f, projection), "reversed near and far rejected");
    ExpectTrue(!TryMakePerspectiveLHMatrix(std::numeric_limits<float>::quiet_NaN(), 1.0f, 1.0f, 11.0f, projection),
        "nonfinite projection rejected");

    if (failures != 0) return 1;
    std::printf("PASS: transforms, vectors, camera conventions and perspective projection\n");
    return 0;
}
