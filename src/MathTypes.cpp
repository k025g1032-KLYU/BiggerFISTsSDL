#include "MathTypes.h"

#include <cmath>

Matrix4x4 MakeIdentityMatrix() {
    Matrix4x4 result{};
    for (int index = 0; index < 4; ++index) {
        result.elements[index][index] = 1.0f;
    }
    return result;
}

Matrix4x4 MakeTranslationMatrix(float x, float y, float z) {
    Matrix4x4 result = MakeIdentityMatrix();
    result.elements[3][0] = x;
    result.elements[3][1] = y;
    result.elements[3][2] = z;
    return result;
}

Matrix4x4 MakeRotationZMatrix(float radians) {
    Matrix4x4 result = MakeIdentityMatrix();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.elements[0][0] = cosine;
    result.elements[0][1] = sine;
    result.elements[1][0] = -sine;
    result.elements[1][1] = cosine;
    return result;
}

Matrix4x4 MakeScaleMatrix(float x, float y, float z) {
    Matrix4x4 result = MakeIdentityMatrix();
    result.elements[0][0] = x;
    result.elements[1][1] = y;
    result.elements[2][2] = z;
    return result;
}

Matrix4x4 MultiplyMatrices(const Matrix4x4& left, const Matrix4x4& right) {
    Matrix4x4 result{};
    for (int row = 0; row < 4; ++row) {
        for (int column = 0; column < 4; ++column) {
            for (int index = 0; index < 4; ++index) {
                result.elements[row][column] +=
                    left.elements[row][index] * right.elements[index][column];
            }
        }
    }
    return result;
}

Matrix4x4 MakeAspectCorrectionMatrix(float aspectRatio) {
    if (!std::isfinite(aspectRatio) || aspectRatio <= 0.0f) {
        return MakeIdentityMatrix();
    }
    // Use the shorter output dimension as the reference for both axes.
    if (aspectRatio >= 1.0f) {
        return MakeScaleMatrix(1.0f / aspectRatio, 1.0f, 1.0f);
    }
    return MakeScaleMatrix(1.0f, aspectRatio, 1.0f);
}
