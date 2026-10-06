#include "CameraController.h"

#include "CombatTuning.h"
#include "GameMath.h"

#include <algorithm>
#include <cmath>

void CameraController::Initialize() {
	input_ = KamataEngine::Input::GetInstance();
	camera_.Initialize();
	Reset({0.0f, CombatTuning::kPlayerInitialHeight + CombatTuning::kEyeHeight, 0.0f});
}

void CameraController::Reset(const KamataEngine::Vector3& cameraAnchor) {
	yaw_ = 0.0f;
	pitch_ = 0.0f;
	dodgeRoll_ = 0.0f;
	shakeStrength_ = 0.0f;
	shakeDuration_ = 0.0f;
	shakeTimer_ = 0.0f;
	shakePhase_ = 0.0f;
	directionalImpulseOffset_ = {};
	directionalImpulseDuration_ = 0.0f;
	directionalImpulseTimer_ = 0.0f;
	stepBobPhase_ = 0.0f;
	stepBobWeight_ = 0.0f;
	stepImpactTimer_ = 0.0f;
	followedCameraAnchor_ = cameraAnchor;
	controlMode_ = CameraControlMode::Driving;
	didStepThisFrame_ = false;
	isDebugThirdPerson_ = false;
	isCameraAnchorFollowInitialized_ = true;
	camera_.translation_ = cameraAnchor;
	camera_.rotation_ = {pitch_, yaw_, 0.0f};
	camera_.UpdateMatrix();
}

void CameraController::UpdateLook(float deltaTime, float bodyYaw) {
	const float lookSpeedMultiplier = controlMode_ == CameraControlMode::Observation ? CombatTuning::kObservationLookSpeedMultiplier : 1.0f;
	const KamataEngine::Input::MouseMove mouseMove = input_->GetMouseMove();
	float yawDelta = static_cast<float>(mouseMove.lX) * CombatTuning::kMouseLookSensitivity * lookSpeedMultiplier;
	float pitchDelta = static_cast<float>(mouseMove.lY) * CombatTuning::kMouseLookSensitivity * lookSpeedMultiplier;

	XINPUT_STATE gamepadState{};
	bool hasXInputGamepad = false;
	for (std::size_t joystickIndex = 0; joystickIndex < input_->GetNumberOfJoysticks(); ++joystickIndex) {
		if (input_->GetJoystickState(static_cast<int32_t>(joystickIndex), gamepadState)) {
			hasXInputGamepad = true;
			break;
		}
	}
	if (hasXInputGamepad) {
		const float lookX = NormalizeStickAxis(gamepadState.Gamepad.sThumbRX);
		const float lookY = NormalizeStickAxis(gamepadState.Gamepad.sThumbRY);
		yawDelta += lookX * CombatTuning::kGamepadLookSpeed * lookSpeedMultiplier * deltaTime;
		pitchDelta -= lookY * CombatTuning::kGamepadLookSpeed * lookSpeedMultiplier * deltaTime;
	}

	if (controlMode_ == CameraControlMode::AligningToDriving) {
		yaw_ = GameMath::MoveTowardsAngle(yaw_, bodyYaw, CombatTuning::kDrivingCameraRealignSpeed * deltaTime);
		pitch_ = GameMath::MoveTowards(pitch_, 0.0f, CombatTuning::kDrivingCameraRealignSpeed * deltaTime);
		if (std::abs(GameMath::NormalizeAngle(bodyYaw - yaw_)) <= GameMath::kEpsilon && std::abs(pitch_) <= GameMath::kEpsilon) {
			yaw_ = GameMath::NormalizeAngle(bodyYaw);
			pitch_ = 0.0f;
			controlMode_ = CameraControlMode::Driving;
		}
	} else {
		yaw_ += yawDelta;
		pitch_ += pitchDelta;
	}

	yaw_ = GameMath::NormalizeAngle(yaw_);
	pitch_ = std::clamp(pitch_, -CombatTuning::kMaximumPitch, CombatTuning::kMaximumPitch);
}

void CameraController::ToggleObservationMode(float bodyYaw) {
	switch (controlMode_) {
	case CameraControlMode::Driving:
		controlMode_ = CameraControlMode::Observation;
		break;
	case CameraControlMode::Observation:
		if (std::abs(GameMath::NormalizeAngle(bodyYaw - yaw_)) <= GameMath::kEpsilon) {
			yaw_ = GameMath::NormalizeAngle(bodyYaw);
			controlMode_ = CameraControlMode::Driving;
		} else {
			controlMode_ = CameraControlMode::AligningToDriving;
		}
		break;
	case CameraControlMode::AligningToDriving:
		controlMode_ = CameraControlMode::Observation;
		break;
	}
}

void CameraController::BeginTutorialVisualCheck() {
	controlMode_ = CameraControlMode::Observation;
	isDebugThirdPerson_ = false;
}

void CameraController::BeginTutorialCockpitAlignment(float bodyYaw) {
	isDebugThirdPerson_ = false;
	if (std::abs(GameMath::NormalizeAngle(bodyYaw - yaw_)) <= GameMath::kEpsilon && std::abs(pitch_) <= GameMath::kEpsilon) {
		yaw_ = GameMath::NormalizeAngle(bodyYaw);
		pitch_ = 0.0f;
		controlMode_ = CameraControlMode::Driving;
		return;
	}
	controlMode_ = CameraControlMode::AligningToDriving;
}

void CameraController::ConnectTutorialCockpit(float bodyYaw) {
	yaw_ = GameMath::NormalizeAngle(bodyYaw);
	controlMode_ = CameraControlMode::Driving;
	isDebugThirdPerson_ = false;
}

const char* CameraController::GetControlModeName() const {
	switch (controlMode_) {
	case CameraControlMode::Driving:
		return "Driving";
	case CameraControlMode::Observation:
		return "Observation";
	case CameraControlMode::AligningToDriving:
		return "Aligning";
	}
	return "Unknown";
}

const char* CameraController::GetViewModeName() const {
	if (!isDebugThirdPerson_) {
		return "First Person";
	}
	return controlMode_ == CameraControlMode::Observation ? "Debug Third Person / Free Orbit" : "Debug Third Person / Follow";
}

void CameraController::UpdateDodgeRoll(float dodgeLateralAmount, bool isDodgeBoosting, float deltaTime) {
	const float clampedLateralAmount = std::clamp(dodgeLateralAmount, -1.0f, 1.0f);
	const float targetRoll = isDodgeBoosting ? -clampedLateralAmount * CombatTuning::kDodgeCameraMaximumRoll : 0.0f;
	const float transitionDuration = isDodgeBoosting ? CombatTuning::kDodgeVisualLeanInDuration : CombatTuning::kDodgeVisualLeanOutDuration;
	const float rollSpeed = CombatTuning::kDodgeCameraMaximumRoll / (std::max)(transitionDuration, GameMath::kEpsilon);
	dodgeRoll_ = GameMath::MoveTowards(dodgeRoll_, targetRoll, rollSpeed * deltaTime);
}

void CameraController::UpdateCamera(const KamataEngine::Vector3& cameraAnchor, const KamataEngine::Vector3& playerPosition, float moveSpeed, bool suppressFootstepFeedback, float deltaTime) {
	didStepThisFrame_ = false;
	const float speedRatio = std::clamp(moveSpeed / CombatTuning::kMaximumMoveSpeed, 0.0f, 1.0f);
	const float targetBobWeight = !suppressFootstepFeedback && moveSpeed > CombatTuning::kStepBobMinimumSpeed ? speedRatio : 0.0f;
	stepBobWeight_ = GameMath::MoveTowards(stepBobWeight_, targetBobWeight, CombatTuning::kStepBobFadeSpeed * deltaTime);
	if (targetBobWeight > 0.0f) {
		const int32_t previousStepIndex = static_cast<int32_t>(std::floor(stepBobPhase_ / GameMath::kPi));
		stepBobPhase_ += CombatTuning::kStepBobFrequency * speedRatio * deltaTime;
		const int32_t currentStepIndex = static_cast<int32_t>(std::floor(stepBobPhase_ / GameMath::kPi));
		if (currentStepIndex != previousStepIndex && stepBobWeight_ > 0.2f) {
			stepImpactTimer_ = CombatTuning::kStepImpactDuration;
			didStepThisFrame_ = true;
		}
	}
	stepImpactTimer_ = (std::max)(0.0f, stepImpactTimer_ - deltaTime);
	const float stepImpactRatio = stepImpactTimer_ / CombatTuning::kStepImpactDuration;

	KamataEngine::Vector3 stepBobOffset{};
	stepBobOffset.x = std::sin(stepBobPhase_) * CombatTuning::kStepBobHorizontalAmplitude * stepBobWeight_;
	stepBobOffset.y = -0.5f * (1.0f - std::cos(stepBobPhase_ * 2.0f)) * CombatTuning::kStepBobVerticalAmplitude * stepBobWeight_;
	stepBobOffset.y -= CombatTuning::kStepImpactAmplitude * stepImpactRatio * stepBobWeight_;
	const float stepPitchOffset = CombatTuning::kStepImpactPitch * stepImpactRatio * stepBobWeight_;

	KamataEngine::Vector3 shakeOffset{};
	if (shakeTimer_ > 0.0f && shakeDuration_ > GameMath::kEpsilon) {
		shakeTimer_ = (std::max)(0.0f, shakeTimer_ - deltaTime);
		shakePhase_ += deltaTime * CombatTuning::kShakeFrequency;
		const float attenuation = shakeTimer_ / shakeDuration_;
		shakeOffset.x = std::sin(shakePhase_) * shakeStrength_ * attenuation;
		shakeOffset.y = std::sin(shakePhase_ * CombatTuning::kSecondaryShakeFrequencyScale) * shakeStrength_ * attenuation;
	}

	KamataEngine::Vector3 directionalImpulseOffset{};
	if (directionalImpulseTimer_ > 0.0f && directionalImpulseDuration_ > GameMath::kEpsilon) {
		directionalImpulseTimer_ = (std::max)(0.0f, directionalImpulseTimer_ - deltaTime);
		directionalImpulseOffset = GameMath::Multiply(directionalImpulseOffset_, directionalImpulseTimer_ / directionalImpulseDuration_);
	}
	const KamataEngine::Vector3 feedbackOffset = GameMath::Add(shakeOffset, directionalImpulseOffset);

	if (isDebugThirdPerson_) {
		isCameraAnchorFollowInitialized_ = false;
		const bool isFreeOrbit = controlMode_ == CameraControlMode::Observation;
		const float orbitPitch = std::clamp(
		    pitch_ + CombatTuning::kDebugThirdPersonDefaultPitch, isFreeOrbit ? CombatTuning::kDebugThirdPersonObservationMinimumPitch : CombatTuning::kDebugThirdPersonMinimumPitch,
		    isFreeOrbit ? CombatTuning::kDebugThirdPersonObservationMaximumPitch : CombatTuning::kDebugThirdPersonMaximumPitch);
		const KamataEngine::Vector3 orbitDirection = GameMath::ForwardFromYawPitch(yaw_, orbitPitch);
		const KamataEngine::Vector3 targetPosition = GameMath::Add(playerPosition, {0.0f, CombatTuning::kDebugThirdPersonTargetHeight, 0.0f});
		camera_.translation_ = GameMath::Add(GameMath::Subtract(targetPosition, GameMath::Multiply(orbitDirection, CombatTuning::kDebugThirdPersonDistance)), feedbackOffset);
		camera_.rotation_ = {orbitPitch, yaw_, 0.0f};
	} else {
		if (!isCameraAnchorFollowInitialized_ || GameMath::LengthSquared(GameMath::Subtract(cameraAnchor, followedCameraAnchor_)) >=
		                                             CombatTuning::kCockpitCameraPositionFollowSnapDistance * CombatTuning::kCockpitCameraPositionFollowSnapDistance) {
			followedCameraAnchor_ = cameraAnchor;
			isCameraAnchorFollowInitialized_ = true;
		} else {
			const float followAmount = 1.0f - std::exp(-CombatTuning::kCockpitCameraPositionFollowResponsiveness * deltaTime);
			followedCameraAnchor_ = GameMath::Lerp(followedCameraAnchor_, cameraAnchor, followAmount);
		}
		camera_.translation_ = GameMath::Add(GameMath::Add(followedCameraAnchor_, stepBobOffset), feedbackOffset);
		camera_.rotation_ = {pitch_ + stepPitchOffset, yaw_, dodgeRoll_};
	}
	camera_.UpdateMatrix();
}

void CameraController::AddShake(float strength, float duration) {
	shakeStrength_ = (std::max)(shakeStrength_, strength);
	shakeDuration_ = (std::max)(duration, GameMath::kEpsilon);
	shakeTimer_ = (std::max)(shakeTimer_, duration);
}

void CameraController::AddDirectionalImpulse(const KamataEngine::Vector3& movementDirection, float strength, float duration) {
	const KamataEngine::Vector3 horizontalDirection = GameMath::Normalize({movementDirection.x, 0.0f, movementDirection.z});
	if (GameMath::LengthSquared(horizontalDirection) <= GameMath::kEpsilon) {
		return;
	}
	directionalImpulseOffset_ = GameMath::Multiply(horizontalDirection, -strength);
	directionalImpulseDuration_ = (std::max)(duration, GameMath::kEpsilon);
	directionalImpulseTimer_ = directionalImpulseDuration_;
}

float CameraController::NormalizeStickAxis(int16_t rawValue) {
	const float denominator = rawValue >= 0 ? 32767.0f : 32768.0f;
	const float value = static_cast<float>(rawValue) / denominator;
	const float magnitude = std::abs(value);
	if (magnitude <= CombatTuning::kStickDeadZone) {
		return 0.0f;
	}
	const float normalizedMagnitude = (magnitude - CombatTuning::kStickDeadZone) / (1.0f - CombatTuning::kStickDeadZone);
	return std::copysign(normalizedMagnitude, value);
}
