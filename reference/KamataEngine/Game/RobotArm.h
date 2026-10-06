#pragma once

#include "Collision.h"
#include "ColoredObject3d.h"

#include <KamataEngine.h>

#include <cstdint>

enum class ArmSide {
	Left,
	Right,
};

enum class ArmState {
	Ready,
	Charging,
	Charged,
	Punching,
	RocketFlying,
	RocketEvacuating,
	Returning,
	UltimateLocked,
	Recovery,
};

enum class ArmAttackType {
	None,
	Punch,
	RocketPunch,
};

struct ArmInput {
	bool punchHeld = false;
	bool rocketHeld = false;
};

struct ArmAnchor {
	KamataEngine::Vector3 playerPosition{};
	float bodyYaw = 0.0f;
	const KamataEngine::WorldTransform* visualRoot = nullptr;
	KamataEngine::Vector3 visualRootWorldPosition{};
};

struct RocketTrajectory {
	KamataEngine::Vector3 start{};
	KamataEngine::Vector3 direction{0.0f, 0.0f, 1.0f};
	float flightDistance = 0.0f;
};

class RobotArm {
public:
	void Initialize(ArmSide side, KamataEngine::Model* cubeModel, KamataEngine::Model* upperArmModel, KamataEngine::Model* fistModel, KamataEngine::Model* rocketFlameModel);
	void Reset(const ArmAnchor& anchor);
	void Update(float deltaTime, const ArmAnchor& anchor, const ArmInput& input, bool allowNewCharge);
	void RefreshVisualTransforms(const ArmAnchor& anchor);
	void SetStartupVisualOffsetY(float offsetY) { startupVisualOffsetY_ = offsetY; }
	void Draw();
	void SetFistVisible(bool isVisible) { isFistVisible_ = isVisible; }
	void CancelCharge(const ArmAnchor& anchor);
	void SetUltimateFormationPose(const ArmAnchor& anchor, const KamataEngine::Vector3& formationPosition, float progress);
	void BeginUltimateLock(const ArmAnchor& anchor);
	void BeginUltimateReturn(const ArmAnchor& anchor, const KamataEngine::Vector3& splitPosition, const KamataEngine::Vector3& flightDirection);

	ArmState GetState() const { return state_; }
	ArmAttackType GetAttackType() const { return activeAttackType_; }
	float GetChargeRatio() const;
	const char* GetStateName() const;
	Collision::Sphere GetFistSphere() const;
	RocketTrajectory GetRocketPreviewTrajectory(float bodyYaw) const;
	const KamataEngine::Vector3& GetAttackDirection() const { return attackDirection_; }
	const KamataEngine::Vector3& GetPreviousFistCenter() const { return previousFistCenter_; }
	bool IsPunchDamageActive() const { return punchDamageActiveThisFrame_; }
	bool IsRocketDamageActive() const { return rocketDamageActiveThisFrame_; }
	bool DidBeginPunchThisFrame() const { return didBeginPunchThisFrame_; }
	bool DidLaunchRocketThisFrame() const { return didLaunchRocketThisFrame_; }
	bool DidReconnectThisFrame() const { return didReconnectThisFrame_; }
	bool IsFistVisible() const { return isFistVisible_; }
	bool IsFistAway() const { return state_ == ArmState::RocketFlying || state_ == ArmState::RocketEvacuating || state_ == ArmState::Returning || state_ == ArmState::UltimateLocked; }

private:
	enum class RocketReturnPhase {
		TurnUp,
		Ascend,
		TurnHome,
		FlyHome,
		Align,
		Dock,
		UltimateDock,
	};

	void BeginCharge(ArmAttackType attackType);
	void CommitAttack(const ArmAnchor& anchor);
	void BeginRocketReturn(bool avoidBase);
	void AdvanceRocketReturnPhase(RocketReturnPhase phase);
	void UpdateRocketReturn(float deltaTime, const ArmAnchor& anchor);
	void EnterRecovery();
	void UpdateReadyPose(const ArmAnchor& anchor);
	void UpdateTransforms(const ArmAnchor& anchor);
	KamataEngine::Vector3 CalculateChargedVisualShakeOffset() const;
	KamataEngine::Vector3 CalculateChargePosePosition(const ArmAnchor& anchor, float chargeRatio) const;
	KamataEngine::Vector3 ConvertWorldPositionToVisualLocal(const ArmAnchor& anchor, const KamataEngine::Vector3& worldPosition) const;
	KamataEngine::Vector3 CalculateShoulderPosition(const ArmAnchor& anchor) const;
	KamataEngine::Vector3 CalculateHomeFistPosition(const ArmAnchor& anchor) const;
	KamataEngine::Vector3 CalculateRocketDockFistPosition(const ArmAnchor& anchor) const;
	KamataEngine::Vector3 CalculateElbowPosition(const ArmAnchor& anchor, const KamataEngine::Vector3& shoulderPosition, const KamataEngine::Vector3& upperArmTarget) const;
	KamataEngine::Vector3 CalculateSegmentRotation(const ArmAnchor& anchor, const KamataEngine::Vector3& startPosition, const KamataEngine::Vector3& endPosition) const;
	bool UsesRocketExtendedPose() const;
	bool IsRocketFlameActive() const;
	void UpdateRocketFlameTransform();
	KamataEngine::Vector3 CalculateReturnEntryPosition(const ArmAnchor& anchor) const;
	float CalculateRocketMaximumRange(const KamataEngine::Vector3& start, const KamataEngine::Vector3& direction) const;
	float CalculateRocketBaseAvoidanceDistance(const KamataEngine::Vector3& start, const KamataEngine::Vector3& direction) const;

	ArmSide side_ = ArmSide::Left;
	ArmState state_ = ArmState::Ready;
	ArmAttackType activeAttackType_ = ArmAttackType::None;
	KamataEngine::Object3d upperArm_;
	KamataEngine::Object3d forearmPoseRoot_;
	KamataEngine::Object3d forearmFist_;
	ColoredObject3d rocketFlameObject_;
	KamataEngine::Vector3 fistCenter_{};
	KamataEngine::Vector3 previousFistCenter_{};
	KamataEngine::Vector3 attackDirection_{0.0f, 0.0f, 1.0f};
	KamataEngine::Vector3 returnPhaseStartPosition_{};
	RocketReturnPhase rocketReturnPhase_ = RocketReturnPhase::TurnUp;
	float rocketPosePitch_ = 0.0f;
	float rocketPoseYaw_ = 0.0f;
	float returnPhaseStartPitch_ = 0.0f;
	float returnPhaseStartYaw_ = 0.0f;
	float returnAscentDistance_ = 0.0f;
	float stateTimer_ = 0.0f;
	float chargeTimer_ = 0.0f;
	float chargedVisualShakeTimer_ = 0.0f;
	float rocketTravelDistance_ = 0.0f;
	float rocketMaximumRange_ = 0.0f;
	float rocketBaseAvoidanceDistance_ = 0.0f;
	float startupVisualOffsetY_ = 0.0f;
	float ultimateFormationPoseProgress_ = 0.0f;
	bool isUltimateFormationPoseActive_ = false;
	bool punchDamageActiveThisFrame_ = false;
	bool rocketDamageActiveThisFrame_ = false;
	bool didBeginPunchThisFrame_ = false;
	bool didLaunchRocketThisFrame_ = false;
	bool didReconnectThisFrame_ = false;
	bool isFistVisible_ = true;
	bool isUltimateReturn_ = false;
};
