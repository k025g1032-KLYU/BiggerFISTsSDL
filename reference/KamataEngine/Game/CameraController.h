#pragma once

#include <KamataEngine.h>

enum class CameraControlMode {
	Driving,
	Observation,
	AligningToDriving,
};

class CameraController {
public:
	void Initialize();
	void Reset(const KamataEngine::Vector3& cameraAnchor);
	void UpdateLook(float deltaTime, float bodyYaw);
	void UpdateDodgeRoll(float dodgeLateralAmount, bool isDodgeBoosting, float deltaTime);
	void UpdateCamera(const KamataEngine::Vector3& cameraAnchor, const KamataEngine::Vector3& playerPosition, float moveSpeed, bool suppressFootstepFeedback, float deltaTime);
	void AddShake(float strength, float duration);
	void AddDirectionalImpulse(const KamataEngine::Vector3& movementDirection, float strength, float duration);
	void ToggleObservationMode(float bodyYaw);
	void BeginTutorialVisualCheck();
	void BeginTutorialCockpitAlignment(float bodyYaw);
	void ConnectTutorialCockpit(float bodyYaw);
	void ToggleDebugThirdPerson() { isDebugThirdPerson_ = !isDebugThirdPerson_; }

	KamataEngine::Camera& GetCamera() { return camera_; }
	float GetYaw() const { return yaw_; }
	float GetPitch() const { return pitch_; }
	float GetDodgeRoll() const { return dodgeRoll_; }
	bool DidStepThisFrame() const { return didStepThisFrame_; }
	bool IsDebugThirdPerson() const { return isDebugThirdPerson_; }
	CameraControlMode GetControlMode() const { return controlMode_; }
	bool ShouldBodyFollowCamera() const { return controlMode_ == CameraControlMode::Driving; }
	const char* GetControlModeName() const;
	const char* GetViewModeName() const;

private:
	static float NormalizeStickAxis(int16_t rawValue);

	KamataEngine::Camera camera_;
	KamataEngine::Input* input_ = nullptr;
	float yaw_ = 0.0f;
	float pitch_ = 0.0f;
	float dodgeRoll_ = 0.0f;
	float shakeStrength_ = 0.0f;
	float shakeDuration_ = 0.0f;
	float shakeTimer_ = 0.0f;
	float shakePhase_ = 0.0f;
	KamataEngine::Vector3 directionalImpulseOffset_{};
	float directionalImpulseDuration_ = 0.0f;
	float directionalImpulseTimer_ = 0.0f;
	float stepBobPhase_ = 0.0f;
	float stepBobWeight_ = 0.0f;
	float stepImpactTimer_ = 0.0f;
	KamataEngine::Vector3 followedCameraAnchor_{};
	CameraControlMode controlMode_ = CameraControlMode::Driving;
	bool didStepThisFrame_ = false;
	bool isDebugThirdPerson_ = false;
	bool isCameraAnchorFollowInitialized_ = false;
};
