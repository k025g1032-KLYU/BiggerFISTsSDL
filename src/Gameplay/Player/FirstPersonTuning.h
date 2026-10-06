#pragma once

namespace FirstPersonTuning {
// Values ported from the original game's CombatTuning.h.
inline constexpr float kMoveAcceleration = 6.0f;
inline constexpr float kMoveDeceleration = 7.0f;
inline constexpr float kMaximumMoveSpeed = 4.8f;
inline constexpr float kBodyTurnSpeed = 1.8f;
inline constexpr float kPlayerInitialHeight = 1.7f;
inline constexpr float kEyeHeight = 4.15f;
inline constexpr float kShoulderHeight = 2.85f;
inline constexpr float kShoulderHalfWidth = 1.55f;
inline constexpr float kUpperArmFixedLength = 2.0f;
inline constexpr float kReadyFistForwardOffset = 1.85f;
inline constexpr float kPunchChargeTime = 0.55f;
inline constexpr float kPunchChargeRetractDistance = 0.3f;
inline constexpr float kPunchDuration = 0.22f;
inline constexpr float kPunchDistance = 3.0f;
inline constexpr float kPunchConvergenceTargetForwardDistance = 4.4f;
inline constexpr float kPunchRecoveryDuration = 0.3f;
inline constexpr float kFistRadius = 0.55f;
inline constexpr float kTargetRadius = 1.25f;
inline constexpr int kPunchDamage = 1;
inline constexpr int kTargetHitPoints = 1;
inline constexpr float kPlayerColliderRadius = 1.35f;
inline constexpr float kBattlefieldMinimumX = -24.0f;
inline constexpr float kBattlefieldMaximumX = 24.0f;
inline constexpr float kBattlefieldMinimumZ = -16.0f;
inline constexpr float kBattlefieldMaximumZ = 42.0f;
inline constexpr float kMouseLookSensitivity = 0.0025f;
inline constexpr float kMaximumPitch = 1.15f;
inline constexpr float kCameraFollowResponsiveness = 20.0f;
inline constexpr float kCameraFollowSnapDistance = 3.0f;
}
