#include "MathTypes.h"

#include <cmath>
#include <cstdio>
#include <initializer_list>
#include <limits>
#include <numbers>

namespace {
int failures = 0;

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

    if (failures != 0) return 1;
    std::printf("PASS: matrix conventions, transform order and aspect correction\n");
    return 0;
}
