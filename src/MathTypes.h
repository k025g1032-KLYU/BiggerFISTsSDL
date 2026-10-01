#pragma once

struct Vector3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

// Row-major storage with row vectors: transformedPosition = position * matrix.
struct alignas(16) Matrix4x4 {
    float elements[4][4]{};
};

static_assert(sizeof(Matrix4x4) == 64);
static_assert(alignof(Matrix4x4) == 16);

Matrix4x4 MakeIdentityMatrix();
Matrix4x4 MakeTranslationMatrix(float x, float y, float z);
Matrix4x4 MakeRotationZMatrix(float radians);
Matrix4x4 MakeRotationXMatrix(float radians);
Matrix4x4 MakeRotationYMatrix(float radians);
Matrix4x4 MakeScaleMatrix(float x, float y, float z);
Matrix4x4 MultiplyMatrices(const Matrix4x4& left, const Matrix4x4& right);
Matrix4x4 MakeAspectCorrectionMatrix(float aspectRatio);

Vector3 SubtractVectors(const Vector3& left, const Vector3& right);
float DotVectors(const Vector3& left, const Vector3& right);
Vector3 CrossVectors(const Vector3& left, const Vector3& right);
bool TryNormalizeVector(const Vector3& vector, Vector3& result);
bool TryMakeLookAtLHMatrix(const Vector3& eye, const Vector3& target,
    const Vector3& up, Matrix4x4& result);
bool TryMakePerspectiveLHMatrix(float verticalFovRadians, float aspectRatio,
    float nearPlane, float farPlane, Matrix4x4& result);
