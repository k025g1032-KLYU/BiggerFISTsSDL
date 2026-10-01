#pragma once

// Row-major storage with row vectors: transformedPosition = position * matrix.
struct alignas(16) Matrix4x4 {
    float elements[4][4]{};
};

static_assert(sizeof(Matrix4x4) == 64);
static_assert(alignof(Matrix4x4) == 16);

Matrix4x4 MakeIdentityMatrix();
Matrix4x4 MakeTranslationMatrix(float x, float y, float z);
Matrix4x4 MakeRotationZMatrix(float radians);
Matrix4x4 MakeScaleMatrix(float x, float y, float z);
Matrix4x4 MultiplyMatrices(const Matrix4x4& left, const Matrix4x4& right);
Matrix4x4 MakeAspectCorrectionMatrix(float aspectRatio);
