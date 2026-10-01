#include "MathTypes.h"

#include <cmath>
#include <numbers>

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

Matrix4x4 MakeRotationXMatrix(float radians) {
    Matrix4x4 result = MakeIdentityMatrix();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.elements[1][1] = cosine;
    result.elements[1][2] = sine;
    result.elements[2][1] = -sine;
    result.elements[2][2] = cosine;
    return result;
}

Matrix4x4 MakeRotationYMatrix(float radians) {
    Matrix4x4 result = MakeIdentityMatrix();
    const float cosine = std::cos(radians);
    const float sine = std::sin(radians);
    result.elements[0][0] = cosine;
    result.elements[0][2] = -sine;
    result.elements[2][0] = sine;
    result.elements[2][2] = cosine;
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

Vector3 SubtractVectors(const Vector3& left, const Vector3& right) {
    return { left.x - right.x, left.y - right.y, left.z - right.z };
}

float DotVectors(const Vector3& left, const Vector3& right) {
    return left.x * right.x + left.y * right.y + left.z * right.z;
}

Vector3 CrossVectors(const Vector3& left, const Vector3& right) {
    return {
        left.y * right.z - left.z * right.y,
        left.z * right.x - left.x * right.z,
        left.x * right.y - left.y * right.x
    };
}

bool TryNormalizeVector(const Vector3& vector, Vector3& result) {
    const float lengthSquared = DotVectors(vector, vector);
    if (!std::isfinite(lengthSquared) || lengthSquared <= 0.000000000001f) {
        result = {};
        return false;
    }
    const float inverseLength = 1.0f / std::sqrt(lengthSquared);
    result = { vector.x * inverseLength, vector.y * inverseLength, vector.z * inverseLength };
    return true;
}

bool TryMakeLookAtLHMatrix(const Vector3& eye, const Vector3& target,
    const Vector3& up, Matrix4x4& result) {
    result = {};
    Vector3 forward{};
    Vector3 right{};
    if (!TryNormalizeVector(SubtractVectors(target, eye), forward) ||
        !TryNormalizeVector(CrossVectors(up, forward), right)) {
        return false;
    }
    const Vector3 cameraUp = CrossVectors(forward, right);
    const float translationX = -DotVectors(right, eye);
    const float translationY = -DotVectors(cameraUp, eye);
    const float translationZ = -DotVectors(forward, eye);
    if (!std::isfinite(translationX) || !std::isfinite(translationY) || !std::isfinite(translationZ)) {
        return false;
    }
    result = MakeIdentityMatrix();
    result.elements[0][0] = right.x;
    result.elements[1][0] = right.y;
    result.elements[2][0] = right.z;
    result.elements[0][1] = cameraUp.x;
    result.elements[1][1] = cameraUp.y;
    result.elements[2][1] = cameraUp.z;
    result.elements[0][2] = forward.x;
    result.elements[1][2] = forward.y;
    result.elements[2][2] = forward.z;
    result.elements[3][0] = translationX;
    result.elements[3][1] = translationY;
    result.elements[3][2] = translationZ;
    return true;
}

bool TryMakePerspectiveLHMatrix(float verticalFovRadians, float aspectRatio,
    float nearPlane, float farPlane, Matrix4x4& result) {
    result = {};
    if (!std::isfinite(verticalFovRadians) || !std::isfinite(aspectRatio) ||
        !std::isfinite(nearPlane) || !std::isfinite(farPlane) ||
        verticalFovRadians <= 0.0f || verticalFovRadians >= std::numbers::pi_v<float> ||
        aspectRatio <= 0.0f || nearPlane <= 0.0f || farPlane <= nearPlane) {
        return false;
    }
    const float yScale = 1.0f / std::tan(verticalFovRadians / 2.0f);
    const float xScale = yScale / aspectRatio;
    const float depthScale = farPlane / (farPlane - nearPlane);
    const float depthOffset = -nearPlane * depthScale;
    if (!std::isfinite(xScale) || !std::isfinite(yScale) ||
        !std::isfinite(depthScale) || !std::isfinite(depthOffset)) {
        return false;
    }
    // Positive view-space Z is forward; NDC depth is 0 at near and 1 at far.
    result.elements[0][0] = xScale;
    result.elements[1][1] = yScale;
    result.elements[2][2] = depthScale;
    result.elements[2][3] = 1.0f;
    result.elements[3][2] = depthOffset;
    return true;
}
