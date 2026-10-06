#pragma once

#include "RobotArm.h"
#include "UltimateRocketPunch.h"

#include <KamataEngine.h>

struct PlayerInput {
	float moveRight = 0.0f;
	float moveForward = 0.0f;
	bool dodgePressed = false;
	ArmInput leftArm;
	ArmInput rightArm;
};

enum class DodgeState {
	Ready,
	Boosting,
	Sliding,
	Recovery,
};

enum class PlayerHitType {
	Projectile,
	ChargeUnit,
	BossSpreadBurst,
};

enum class PlayerImpactState {
	None,
	Holding,
	Knockback,
};

struct PlayerHitResult {
	bool wasApplied = false;
	int shieldLayersBeforeHit = 0;
	int shieldLayersLost = 0;
	bool didDamageHitPoints = false;
};

class Player {
public:
	void Initialize(
	    KamataEngine::Model* cubeModel, KamataEngine::Model* bodyModel, KamataEngine::Model* ultimateFistModel, KamataEngine::Model* upperArmModel, KamataEngine::Model* leftFistModel,
	    KamataEngine::Model* rightFistModel, KamataEngine::Model* rocketFlameModel);
	void Reset();
	void Update(float deltaTime, const PlayerInput& input, float cameraYaw);
	void SetStartupVisualActivation(float body, float camera, float leftArm, float rightArm);
	void SetSystemOfflineVisualActivation(float body, float camera, float leftArm, float rightArm);
	void Draw();
	bool ApplyDamage(int damage);
	PlayerHitResult ReceiveHit(PlayerHitType hitType);
	void StartChargeImpact(const KamataEngine::Vector3& chargeDirection, bool wasShielded);
	void StartBossBodyPush(const KamataEngine::Vector3& pushDirection);
	void StartBossNeedleComboStun();
	void StartBossNeedleComboFinisher(const KamataEngine::Vector3& pushDirection);
	void StartBossSpreadBurstImpact(const KamataEngine::Vector3& pushDirection);

	KamataEngine::Vector3 GetCameraAnchor() const;
	KamataEngine::WorldTransform& GetCockpitMountWorldTransform() { return cockpitMount_.GetWorldTransform(); }
	float GetMoveSpeed() const;
	const KamataEngine::Vector3& GetPosition() const { return position_; }
	const KamataEngine::Vector3& GetPreviousPosition() const { return previousPosition_; }
	const KamataEngine::Vector3& GetVelocity() const { return velocity_; }
	int GetHitPoints() const { return hitPoints_; }
	int GetMaximumHitPoints() const;
	int GetShieldLayers() const { return shieldLayers_; }
	int GetMaximumShieldLayers() const;
	float GetShieldRechargeRatio() const;
	PlayerImpactState GetImpactState() const { return impactState_; }
	const char* GetImpactStateName() const;
	float GetImpactStateTimer() const { return impactStateTimer_; }
	const KamataEngine::Vector3& GetImpactDirection() const { return impactDirection_; }
	bool IsImpactReactionActive() const { return impactState_ != PlayerImpactState::None; }
	Collision::Sphere GetCollisionSphere() const;
	KamataEngine::Vector3 GetPreviousCollisionCenter() const;
	float GetBodyYaw() const { return bodyYaw_; }
	float GetBodyRoll() const { return bodyRoll_; }
	DodgeState GetDodgeState() const { return dodgeState_; }
	const char* GetDodgeStateName() const;
	float GetDodgeStateTimer() const { return dodgeStateTimer_; }
	float GetDodgeCooldownRemaining() const { return dodgeCooldownTimer_; }
	const KamataEngine::Vector3& GetDodgeDirection() const { return dodgeDirection_; }
	float GetDodgeLateralAmount() const { return dodgeLateralAmount_; }
	bool IsDodgeReady() const;
	bool IsDodgeMovementActive() const { return dodgeState_ == DodgeState::Boosting || dodgeState_ == DodgeState::Sliding; }
	bool DidStartDodgeThisFrame() const { return didStartDodgeThisFrame_; }
	bool IsSystemOffline() const { return isSystemOffline_; }
	bool IsUltimateRecoilActive() const { return isUltimateRecoilActive_; }
	float GetSystemOfflineRemaining() const { return systemOfflineTimer_; }
	bool DidRestartSystemThisFrame() const { return didRestartSystemThisFrame_; }
	bool IsRestartPulseActive() const { return restartPulseTimer_ > 0.0f; }
#ifndef NDEBUG
	void DebugBeginSystemOffline();
#endif
	UltimateRocketPunch& GetUltimateRocketPunch() { return ultimateRocketPunch_; }
	const UltimateRocketPunch& GetUltimateRocketPunch() const { return ultimateRocketPunch_; }
	RobotArm& GetLeftArm() { return leftArm_; }
	RobotArm& GetRightArm() { return rightArm_; }
	const RobotArm& GetLeftArm() const { return leftArm_; }
	const RobotArm& GetRightArm() const { return rightArm_; }

private:
	ArmAnchor CreateArmAnchor();
	KamataEngine::Vector3 CalculateNormalMoveDirection(const PlayerInput& input, float cameraYaw) const;
	KamataEngine::Vector3 CalculateDodgeDirection(const PlayerInput& input) const;
	void StartDodge(const PlayerInput& input);
	void UpdateNormalMovement(float deltaTime, const KamataEngine::Vector3& moveDirection);
	void UpdateDodgeMovement(float deltaTime, const KamataEngine::Vector3& normalMoveDirection);
	void UpdateDodgeBodyLean(float deltaTime);
	void UpdateShield(float deltaTime);
	void UpdateImpactMovement(float deltaTime);
	void UpdateArmsAndUltimate(float deltaTime, const PlayerInput& input, const ArmAnchor& anchor, bool allowNewArmCharge);
	void UpdateUltimateRecoil(float deltaTime);
	void UpdateUltimateFlightAndArms(float deltaTime);
	void UpdateSystemOffline(float deltaTime);
	void BeginSystemOffline(bool isUltimateShutdown = false);
	void CancelAllChargeActions();
	void EndSystemOffline();
	void SetVisualActivation(float body, float camera, float leftArm, float rightArm, float bodySinkDistance, float cameraSinkDistance, float armSinkDistance);
	bool AreBothArmsChargedForRocket() const;
	KamataEngine::Vector3 CalculateUltimateFormationPosition() const;
	void ClampToBattlefield();
	void ResolveResearchInstituteCollision();
	void UpdateBodyTransform();

	KamataEngine::Object3d bodyYawRoot_;
	KamataEngine::Object3d bodyLeanRoot_;
	KamataEngine::Object3d body_;
	KamataEngine::Object3d cockpitMount_;
	RobotArm leftArm_;
	RobotArm rightArm_;
	UltimateRocketPunch ultimateRocketPunch_;
	KamataEngine::Vector3 position_{};
	KamataEngine::Vector3 previousPosition_{};
	KamataEngine::Vector3 velocity_{};
	KamataEngine::Vector3 cockpitMountWorldPosition_{};
	KamataEngine::Vector3 dodgeDirection_{0.0f, 0.0f, 1.0f};
	KamataEngine::Vector3 impactDirection_{0.0f, 0.0f, 1.0f};
	float bodyYaw_ = 0.0f;
	float bodyRoll_ = 0.0f;
	float dodgeLateralAmount_ = 0.0f;
	DodgeState dodgeState_ = DodgeState::Ready;
	float dodgeStateTimer_ = 0.0f;
	float dodgeCooldownTimer_ = 0.0f;
	float shieldRechargeTimer_ = 0.0f;
	float impactStateTimer_ = 0.0f;
	float impactKnockbackInitialSpeed_ = 0.0f;
	float impactKnockbackDuration_ = 0.0f;
	float bossFinisherShutdownDelayTimer_ = -1.0f;
	float systemOfflineTimer_ = 0.0f;
	float ultimateRecoilTimer_ = 0.0f;
	float restartPulseTimer_ = 0.0f;
	float startupBodyVisualOffsetY_ = 0.0f;
	float startupCameraVisualOffsetY_ = 0.0f;
	int hitPoints_ = 0;
	int shieldLayers_ = 0;
	PlayerImpactState impactState_ = PlayerImpactState::None;
	bool didStartDodgeThisFrame_ = false;
	bool isSystemOffline_ = false;
	bool isUltimateShutdown_ = false;
	bool isUltimateRecoilActive_ = false;
	bool didRestartSystemThisFrame_ = false;
};
