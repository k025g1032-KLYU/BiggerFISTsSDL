#pragma once

#include <KamataEngine.h>

#include <algorithm>
#include <cmath>

namespace GameMath {

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTwoPi = kPi * 2.0f;
inline constexpr float kEpsilon = 0.00001f;

inline KamataEngine::Vector3 Add(const KamataEngine::Vector3& left, const KamataEngine::Vector3& right) {
	return {left.x + right.x, left.y + right.y, left.z + right.z};
}

inline KamataEngine::Vector3 Subtract(const KamataEngine::Vector3& left, const KamataEngine::Vector3& right) {
	return {left.x - right.x, left.y - right.y, left.z - right.z};
}

inline KamataEngine::Vector3 Multiply(const KamataEngine::Vector3& value, float scalar) {
	return {value.x * scalar, value.y * scalar, value.z * scalar};
}

inline float Dot(const KamataEngine::Vector3& left, const KamataEngine::Vector3& right) {
	return left.x * right.x + left.y * right.y + left.z * right.z;
}

inline float LengthSquared(const KamataEngine::Vector3& value) { return Dot(value, value); }

inline float Length(const KamataEngine::Vector3& value) { return std::sqrt(LengthSquared(value)); }

inline KamataEngine::Vector3 Normalize(const KamataEngine::Vector3& value) {
	const float length = Length(value);
	if (length <= kEpsilon) {
		return {0.0f, 0.0f, 0.0f};
	}
	return Multiply(value, 1.0f / length);
}

inline KamataEngine::Vector3 Lerp(const KamataEngine::Vector3& start, const KamataEngine::Vector3& end, float amount) {
	return Add(start, Multiply(Subtract(end, start), std::clamp(amount, 0.0f, 1.0f)));
}

inline KamataEngine::Vector3 ForwardFromYaw(float yaw) { return {std::sin(yaw), 0.0f, std::cos(yaw)}; }

inline KamataEngine::Vector3 RightFromYaw(float yaw) { return {std::cos(yaw), 0.0f, -std::sin(yaw)}; }

inline KamataEngine::Vector3 ForwardFromYawPitch(float yaw, float pitch) {
	const float cosPitch = std::cos(pitch);
	return Normalize({std::sin(yaw) * cosPitch, -std::sin(pitch), std::cos(yaw) * cosPitch});
}

inline float NormalizeAngle(float angle) {
	while (angle > kPi) {
		angle -= kTwoPi;
	}
	while (angle < -kPi) {
		angle += kTwoPi;
	}
	return angle;
}

inline float MoveTowards(float current, float target, float maximumDelta) {
	const float difference = target - current;
	if (std::abs(difference) <= maximumDelta) {
		return target;
	}
	return current + std::copysign(maximumDelta, difference);
}

inline float MoveTowardsAngle(float current, float target, float maximumDelta) {
	const float difference = NormalizeAngle(target - current);
	return NormalizeAngle(current + std::clamp(difference, -maximumDelta, maximumDelta));
}

inline float ApplyDeadZone(float value, float deadZone) {
	const float magnitude = std::abs(value);
	if (magnitude <= deadZone) {
		return 0.0f;
	}
	const float normalizedMagnitude = (magnitude - deadZone) / (1.0f - deadZone);
	return std::copysign(normalizedMagnitude, value);
}

} // namespace GameMath
