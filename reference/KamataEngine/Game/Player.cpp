#include "Player.h"

#include "CombatTuning.h"
#include "GameMath.h"

#include <algorithm>
#include <cmath>

void Player::Initialize(
    KamataEngine::Model* cubeModel, KamataEngine::Model* bodyModel, KamataEngine::Model* ultimateFistModel, KamataEngine::Model* upperArmModel, KamataEngine::Model* leftFistModel,
    KamataEngine::Model* rightFistModel, KamataEngine::Model* rocketFlameModel) {
	bodyYawRoot_.Initialize(cubeModel);
	bodyLeanRoot_.Initialize(cubeModel);
	body_.Initialize(bodyModel);
	cockpitMount_.Initialize(cubeModel);
	bodyLeanRoot_.SetParentWorldTransform(&bodyYawRoot_.GetWorldTransform());
	body_.SetParentWorldTransform(&bodyLeanRoot_.GetWorldTransform());
	cockpitMount_.SetParentWorldTransform(&bodyLeanRoot_.GetWorldTransform());
	body_.SetScale({1.0f, 1.0f, 1.0f});
	leftArm_.Initialize(ArmSide::Left, cubeModel, upperArmModel, leftFistModel, rocketFlameModel);
	rightArm_.Initialize(ArmSide::Right, cubeModel, upperArmModel, rightFistModel, rocketFlameModel);
	ultimateRocketPunch_.Initialize(ultimateFistModel, rocketFlameModel);
	Reset();
}

void Player::Reset() {
	position_ = {0.0f, CombatTuning::kPlayerInitialHeight, 0.0f};
	previousPosition_ = position_;
	velocity_ = {0.0f, 0.0f, 0.0f};
	dodgeDirection_ = {0.0f, 0.0f, 1.0f};
	impactDirection_ = {0.0f, 0.0f, 1.0f};
	bodyYaw_ = 0.0f;
	bodyRoll_ = 0.0f;
	dodgeLateralAmount_ = 0.0f;
	dodgeState_ = DodgeState::Ready;
	dodgeStateTimer_ = 0.0f;
	dodgeCooldownTimer_ = 0.0f;
	shieldRechargeTimer_ = 0.0f;
	impactStateTimer_ = 0.0f;
	impactKnockbackInitialSpeed_ = 0.0f;
	impactKnockbackDuration_ = 0.0f;
	bossFinisherShutdownDelayTimer_ = -1.0f;
	systemOfflineTimer_ = 0.0f;
	ultimateRecoilTimer_ = 0.0f;
	restartPulseTimer_ = 0.0f;
	hitPoints_ = CombatTuning::kPlayerMaximumHitPoints;
	shieldLayers_ = CombatTuning::kPlayerMaximumShieldLayers;
	impactState_ = PlayerImpactState::None;
	didStartDodgeThisFrame_ = false;
	isSystemOffline_ = false;
	isUltimateShutdown_ = false;
	isUltimateRecoilActive_ = false;
	didRestartSystemThisFrame_ = false;
	startupBodyVisualOffsetY_ = 0.0f;
	startupCameraVisualOffsetY_ = 0.0f;
	ultimateRocketPunch_.Reset();
	UpdateBodyTransform();
	const ArmAnchor anchor = CreateArmAnchor();
	leftArm_.Reset(anchor);
	rightArm_.Reset(anchor);
}

void Player::SetStartupVisualActivation(float body, float camera, float leftArm, float rightArm) {
	SetVisualActivation(body, camera, leftArm, rightArm, CombatTuning::kCockpitStartupBodySinkDistance, CombatTuning::kCockpitStartupCameraSinkDistance, CombatTuning::kCockpitStartupArmSinkDistance);
}

void Player::SetSystemOfflineVisualActivation(float body, float camera, float leftArm, float rightArm) {
	SetVisualActivation(body, camera, leftArm, rightArm, CombatTuning::kSystemOfflineBodySinkDistance, CombatTuning::kSystemOfflineCameraSinkDistance, CombatTuning::kSystemOfflineArmSinkDistance);
}

void Player::SetVisualActivation(float body, float camera, float leftArm, float rightArm, float bodySinkDistance, float cameraSinkDistance, float armSinkDistance) {
	startupBodyVisualOffsetY_ = -(1.0f - body) * bodySinkDistance;
	startupCameraVisualOffsetY_ = -(1.0f - camera) * cameraSinkDistance;
	leftArm_.SetStartupVisualOffsetY(-(1.0f - leftArm) * armSinkDistance);
	rightArm_.SetStartupVisualOffsetY(-(1.0f - rightArm) * armSinkDistance);
	UpdateBodyTransform();
	const ArmAnchor anchor = CreateArmAnchor();
	leftArm_.RefreshVisualTransforms(anchor);
	rightArm_.RefreshVisualTransforms(anchor);
}

void Player::Update(float deltaTime, const PlayerInput& input, float cameraYaw) {
	previousPosition_ = position_;
	didStartDodgeThisFrame_ = false;
	didRestartSystemThisFrame_ = false;
	ultimateRocketPunch_.BeginFrame();
	restartPulseTimer_ = (std::max)(0.0f, restartPulseTimer_ - deltaTime);
	if (isSystemOffline_) {
		UpdateSystemOffline(deltaTime);
		return;
	}
	if (isUltimateRecoilActive_) {
		UpdateUltimateRecoil(deltaTime);
		return;
	}
	if (bossFinisherShutdownDelayTimer_ >= 0.0f) {
		bossFinisherShutdownDelayTimer_ -= deltaTime;
		if (bossFinisherShutdownDelayTimer_ <= 0.0f) {
			bossFinisherShutdownDelayTimer_ = -1.0f;
			BeginSystemOffline();
			return;
		}
	}

	UpdateShield(deltaTime);
	dodgeCooldownTimer_ = (std::max)(0.0f, dodgeCooldownTimer_ - deltaTime);
	const KamataEngine::Vector3 normalMoveDirection = CalculateNormalMoveDirection(input, cameraYaw);
	if (!IsImpactReactionActive() && input.dodgePressed && IsDodgeReady()) {
		StartDodge(input);
	}
	if (IsImpactReactionActive()) {
		UpdateImpactMovement(deltaTime);
	} else {
		UpdateDodgeMovement(deltaTime, normalMoveDirection);
	}
	UpdateDodgeBodyLean(deltaTime);
	position_ = GameMath::Add(position_, GameMath::Multiply(velocity_, deltaTime));
	ClampToBattlefield();
	ResolveResearchInstituteCollision();

	bodyYaw_ = GameMath::MoveTowardsAngle(bodyYaw_, cameraYaw, CombatTuning::kBodyTurnSpeed * deltaTime);
	UpdateBodyTransform();

	const ArmAnchor anchor = CreateArmAnchor();
	const bool allowNewArmCharge = dodgeState_ != DodgeState::Boosting && !IsImpactReactionActive();
	UpdateArmsAndUltimate(deltaTime, input, anchor, allowNewArmCharge);
}

void Player::Draw() {
	body_.Draw();
	leftArm_.Draw();
	rightArm_.Draw();
	ultimateRocketPunch_.Draw();
}

bool Player::ApplyDamage(int damage) {
	if (hitPoints_ <= 0 || damage <= 0) {
		return false;
	}
	hitPoints_ = (std::max)(0, hitPoints_ - damage);
	return true;
}

PlayerHitResult Player::ReceiveHit(PlayerHitType hitType) {
	PlayerHitResult result{};
	result.wasApplied = true;
	result.shieldLayersBeforeHit = shieldLayers_;
	shieldRechargeTimer_ = 0.0f;

	if (shieldLayers_ > 0) {
		const int layersBeforeHit = shieldLayers_;
		if (hitType == PlayerHitType::ChargeUnit || hitType == PlayerHitType::BossSpreadBurst) {
			shieldLayers_ = 0;
		} else {
			--shieldLayers_;
		}
		result.shieldLayersLost = layersBeforeHit - shieldLayers_;
		return result;
	}

	result.didDamageHitPoints = ApplyDamage(CombatTuning::kPrototypeDamage);
	return result;
}

void Player::StartBossSpreadBurstImpact(const KamataEngine::Vector3& pushDirection) {
	if (isSystemOffline_) {
		return;
	}

	impactDirection_ = GameMath::Normalize({pushDirection.x, 0.0f, pushDirection.z});
	if (GameMath::LengthSquared(impactDirection_) <= GameMath::kEpsilon) {
		impactDirection_ = GameMath::ForwardFromYaw(bodyYaw_);
	}
	impactState_ = PlayerImpactState::Knockback;
	impactStateTimer_ = 0.0f;
	impactKnockbackInitialSpeed_ = CombatTuning::kBossSpreadBurstKnockbackSpeed;
	impactKnockbackDuration_ = CombatTuning::kBossSpreadBurstKnockbackDuration;
	velocity_ = GameMath::Multiply(impactDirection_, impactKnockbackInitialSpeed_);
	dodgeState_ = DodgeState::Recovery;
	dodgeStateTimer_ = 0.0f;
	didStartDodgeThisFrame_ = false;
}

void Player::StartChargeImpact(const KamataEngine::Vector3& chargeDirection, bool wasShielded) {
	if (isSystemOffline_) {
		return;
	}

	impactDirection_ = GameMath::Normalize({chargeDirection.x, 0.0f, chargeDirection.z});
	if (GameMath::LengthSquared(impactDirection_) <= GameMath::kEpsilon) {
		impactDirection_ = GameMath::ForwardFromYaw(bodyYaw_);
	}

	impactStateTimer_ = 0.0f;
	if (wasShielded) {
		impactState_ = PlayerImpactState::Knockback;
		impactKnockbackInitialSpeed_ = CombatTuning::kPlayerShieldedChargeKnockbackSpeed;
		impactKnockbackDuration_ = CombatTuning::kPlayerShieldedChargeKnockbackDuration;
		velocity_ = GameMath::Multiply(impactDirection_, impactKnockbackInitialSpeed_);
	} else {
		impactState_ = PlayerImpactState::Holding;
		impactKnockbackInitialSpeed_ = CombatTuning::kPlayerUnshieldedChargeKnockbackSpeed;
		impactKnockbackDuration_ = CombatTuning::kPlayerUnshieldedChargeKnockbackDuration;
		velocity_ = {0.0f, 0.0f, 0.0f};
	}

	dodgeState_ = DodgeState::Recovery;
	dodgeStateTimer_ = 0.0f;
	didStartDodgeThisFrame_ = false;
}

void Player::StartBossBodyPush(const KamataEngine::Vector3& pushDirection) {
	if (isSystemOffline_) {
		return;
	}

	impactDirection_ = GameMath::Normalize({pushDirection.x, 0.0f, pushDirection.z});
	if (GameMath::LengthSquared(impactDirection_) <= GameMath::kEpsilon) {
		impactDirection_ = GameMath::ForwardFromYaw(bodyYaw_);
	}
	impactState_ = PlayerImpactState::Knockback;
	impactStateTimer_ = 0.0f;
	impactKnockbackInitialSpeed_ = CombatTuning::kBossConeChargeBodyPushSpeed;
	impactKnockbackDuration_ = CombatTuning::kBossConeChargeBodyPushDuration;
	velocity_ = GameMath::Multiply(impactDirection_, impactKnockbackInitialSpeed_);
	dodgeState_ = DodgeState::Recovery;
	dodgeStateTimer_ = 0.0f;
	didStartDodgeThisFrame_ = false;
}

void Player::StartBossNeedleComboStun() {
	if (isSystemOffline_) {
		return;
	}

	impactState_ = PlayerImpactState::Holding;
	impactStateTimer_ = 0.0f;
	impactKnockbackInitialSpeed_ = 0.0f;
	impactKnockbackDuration_ = 0.0f;
	velocity_ = {0.0f, 0.0f, 0.0f};
	dodgeState_ = DodgeState::Recovery;
	dodgeStateTimer_ = 0.0f;
	didStartDodgeThisFrame_ = false;
}

void Player::StartBossNeedleComboFinisher(const KamataEngine::Vector3& pushDirection) {
	if (isSystemOffline_) {
		return;
	}

	impactDirection_ = GameMath::Normalize({pushDirection.x, 0.0f, pushDirection.z});
	if (GameMath::LengthSquared(impactDirection_) <= GameMath::kEpsilon) {
		impactDirection_ = GameMath::ForwardFromYaw(bodyYaw_);
	}
	impactState_ = PlayerImpactState::Knockback;
	impactStateTimer_ = 0.0f;
	impactKnockbackInitialSpeed_ = CombatTuning::kBossNeedleComboFinisherKnockbackSpeed;
	impactKnockbackDuration_ = CombatTuning::kBossNeedleComboFinisherKnockbackDuration;
	velocity_ = GameMath::Multiply(impactDirection_, impactKnockbackInitialSpeed_);
	dodgeState_ = DodgeState::Recovery;
	dodgeStateTimer_ = 0.0f;
	didStartDodgeThisFrame_ = false;
	bossFinisherShutdownDelayTimer_ = CombatTuning::kBossNeedleComboFinisherShutdownDelay;
}

KamataEngine::Vector3 Player::GetCameraAnchor() const { return cockpitMountWorldPosition_; }

float Player::GetMoveSpeed() const { return GameMath::Length({velocity_.x, 0.0f, velocity_.z}); }

int Player::GetMaximumHitPoints() const { return CombatTuning::kPlayerMaximumHitPoints; }

int Player::GetMaximumShieldLayers() const { return CombatTuning::kPlayerMaximumShieldLayers; }

float Player::GetShieldRechargeRatio() const {
	if (shieldLayers_ >= GetMaximumShieldLayers()) {
		return 1.0f;
	}
	return std::clamp(shieldRechargeTimer_ / CombatTuning::kPlayerShieldRechargeInterval, 0.0f, 1.0f);
}

Collision::Sphere Player::GetCollisionSphere() const { return {GameMath::Add(position_, {0.0f, CombatTuning::kPlayerColliderCenterHeight, 0.0f}), CombatTuning::kPlayerColliderRadius}; }

KamataEngine::Vector3 Player::GetPreviousCollisionCenter() const { return GameMath::Add(previousPosition_, {0.0f, CombatTuning::kPlayerColliderCenterHeight, 0.0f}); }

const char* Player::GetDodgeStateName() const {
	switch (dodgeState_) {
	case DodgeState::Ready:
		return "Ready";
	case DodgeState::Boosting:
		return "Boosting";
	case DodgeState::Sliding:
		return "Sliding";
	case DodgeState::Recovery:
		return "Recovery";
	}
	return "Unknown";
}

const char* Player::GetImpactStateName() const {
	switch (impactState_) {
	case PlayerImpactState::None:
		return "None";
	case PlayerImpactState::Holding:
		return "Holding";
	case PlayerImpactState::Knockback:
		return "Knockback";
	}
	return "Unknown";
}

bool Player::IsDodgeReady() const { return dodgeState_ == DodgeState::Ready && dodgeCooldownTimer_ <= GameMath::kEpsilon; }

ArmAnchor Player::CreateArmAnchor() {
	return {
	    position_,
	    bodyYaw_,
	    &bodyLeanRoot_.GetWorldTransform(),
	    GameMath::Add(position_, {0.0f, CombatTuning::kBodyHalfHeight + startupBodyVisualOffsetY_, 0.0f}),
	};
}

KamataEngine::Vector3 Player::CalculateNormalMoveDirection(const PlayerInput& input, float cameraYaw) const {
	KamataEngine::Vector3 moveDirection =
	    GameMath::Add(GameMath::Multiply(GameMath::RightFromYaw(cameraYaw), input.moveRight), GameMath::Multiply(GameMath::ForwardFromYaw(cameraYaw), input.moveForward));
	if (GameMath::LengthSquared(moveDirection) > 1.0f) {
		moveDirection = GameMath::Normalize(moveDirection);
	}
	return moveDirection;
}

KamataEngine::Vector3 Player::CalculateDodgeDirection(const PlayerInput& input) const {
	KamataEngine::Vector3 localDirection{input.moveRight, 0.0f, input.moveForward};
	if (GameMath::LengthSquared(localDirection) <= GameMath::kEpsilon) {
		return GameMath::ForwardFromYaw(bodyYaw_);
	}
	localDirection = GameMath::Normalize(localDirection);
	return GameMath::Normalize(GameMath::Add(GameMath::Multiply(GameMath::RightFromYaw(bodyYaw_), localDirection.x), GameMath::Multiply(GameMath::ForwardFromYaw(bodyYaw_), localDirection.z)));
}

void Player::StartDodge(const PlayerInput& input) {
	dodgeState_ = DodgeState::Boosting;
	dodgeStateTimer_ = 0.0f;
	dodgeCooldownTimer_ = CombatTuning::kDodgeCooldown;
	dodgeDirection_ = CalculateDodgeDirection(input);
	dodgeLateralAmount_ = std::clamp(GameMath::Dot(dodgeDirection_, GameMath::RightFromYaw(bodyYaw_)), -1.0f, 1.0f);
	velocity_ = GameMath::Multiply(dodgeDirection_, CombatTuning::kDodgeInitialSpeed);
	didStartDodgeThisFrame_ = true;
}

void Player::UpdateDodgeBodyLean(float deltaTime) {
	const bool isBoosting = dodgeState_ == DodgeState::Boosting;
	const float targetRoll = isBoosting ? -dodgeLateralAmount_ * CombatTuning::kDodgeBodyMaximumRoll : 0.0f;
	const float transitionDuration = isBoosting ? CombatTuning::kDodgeVisualLeanInDuration : CombatTuning::kDodgeVisualLeanOutDuration;
	const float rollSpeed = CombatTuning::kDodgeBodyMaximumRoll / (std::max)(transitionDuration, GameMath::kEpsilon);
	bodyRoll_ = GameMath::MoveTowards(bodyRoll_, targetRoll, rollSpeed * deltaTime);
}

void Player::UpdateNormalMovement(float deltaTime, const KamataEngine::Vector3& moveDirection) {
	const bool hasMovementInput = GameMath::LengthSquared(moveDirection) > GameMath::kEpsilon;
	const KamataEngine::Vector3 targetVelocity = GameMath::Multiply(moveDirection, CombatTuning::kMaximumMoveSpeed);
	const float velocityChange = (hasMovementInput ? CombatTuning::kMoveAcceleration : CombatTuning::kMoveDeceleration) * deltaTime;
	velocity_.x = GameMath::MoveTowards(velocity_.x, targetVelocity.x, velocityChange);
	velocity_.z = GameMath::MoveTowards(velocity_.z, targetVelocity.z, velocityChange);
}

void Player::UpdateDodgeMovement(float deltaTime, const KamataEngine::Vector3& normalMoveDirection) {
	switch (dodgeState_) {
	case DodgeState::Ready:
		UpdateNormalMovement(deltaTime, normalMoveDirection);
		break;

	case DodgeState::Boosting:
		dodgeStateTimer_ += deltaTime;
		velocity_ = GameMath::Multiply(dodgeDirection_, CombatTuning::kDodgeInitialSpeed);
		if (dodgeStateTimer_ >= CombatTuning::kDodgeBoostDuration) {
			dodgeState_ = DodgeState::Sliding;
			dodgeStateTimer_ = 0.0f;
		}
		break;

	case DodgeState::Sliding: {
		dodgeStateTimer_ += deltaTime;
		const float speed = GameMath::MoveTowards(GetMoveSpeed(), 0.0f, CombatTuning::kDodgeSlideDeceleration * deltaTime);
		velocity_ = GameMath::Multiply(dodgeDirection_, speed);
		if (dodgeStateTimer_ >= CombatTuning::kDodgeSlideDuration) {
			dodgeState_ = DodgeState::Recovery;
			dodgeStateTimer_ = 0.0f;
		}
		break;
	}

	case DodgeState::Recovery:
		dodgeStateTimer_ += deltaTime;
		UpdateNormalMovement(deltaTime, normalMoveDirection);
		if (dodgeStateTimer_ >= CombatTuning::kDodgeRecoveryDuration) {
			dodgeState_ = DodgeState::Ready;
			dodgeStateTimer_ = 0.0f;
		}
		break;
	}
}

void Player::UpdateShield(float deltaTime) {
	if (shieldLayers_ >= GetMaximumShieldLayers()) {
		shieldRechargeTimer_ = 0.0f;
		return;
	}

	shieldRechargeTimer_ += deltaTime;
	if (shieldRechargeTimer_ < CombatTuning::kPlayerShieldRechargeInterval) {
		return;
	}

	++shieldLayers_;
	shieldRechargeTimer_ = 0.0f;
}

void Player::UpdateImpactMovement(float deltaTime) {
	impactStateTimer_ += deltaTime;
	switch (impactState_) {
	case PlayerImpactState::Holding:
		velocity_ = {0.0f, 0.0f, 0.0f};
		if (impactStateTimer_ >= CombatTuning::kPlayerUnshieldedChargeHoldDuration) {
			impactState_ = PlayerImpactState::Knockback;
			impactStateTimer_ = 0.0f;
			velocity_ = GameMath::Multiply(impactDirection_, impactKnockbackInitialSpeed_);
		}
		break;
	case PlayerImpactState::Knockback: {
		const float progress = std::clamp(impactStateTimer_ / impactKnockbackDuration_, 0.0f, 1.0f);
		const float speed = impactKnockbackInitialSpeed_ * (1.0f - progress);
		velocity_ = GameMath::Multiply(impactDirection_, speed);
		if (impactStateTimer_ >= impactKnockbackDuration_) {
			impactState_ = PlayerImpactState::None;
			impactStateTimer_ = 0.0f;
			impactKnockbackInitialSpeed_ = 0.0f;
			impactKnockbackDuration_ = 0.0f;
			velocity_ = {0.0f, 0.0f, 0.0f};
		}
		break;
	}
	case PlayerImpactState::None:
		break;
	}
}

void Player::UpdateArmsAndUltimate(float deltaTime, const PlayerInput& input, const ArmAnchor& anchor, bool allowNewArmCharge) {
	const bool isLeftRocketHeld = input.leftArm.rocketHeld;
	const bool isRightRocketHeld = input.rightArm.rocketHeld;
	const bool areBothRocketsHeld = isLeftRocketHeld && isRightRocketHeld;

	if (ultimateRocketPunch_.IsForming()) {
		if (!ultimateRocketPunch_.IsReady() && !areBothRocketsHeld) {
			ultimateRocketPunch_.CancelCharging();
			leftArm_.Update(deltaTime, anchor, input.leftArm, allowNewArmCharge);
			rightArm_.Update(deltaTime, anchor, input.rightArm, allowNewArmCharge);
			return;
		}

		ArmInput lockedLeftInput = input.leftArm;
		ArmInput lockedRightInput = input.rightArm;
		lockedLeftInput.rocketHeld = true;
		lockedRightInput.rocketHeld = true;
		leftArm_.Update(deltaTime, anchor, lockedLeftInput, false);
		rightArm_.Update(deltaTime, anchor, lockedRightInput, false);
		const bool wasUltimateReady = ultimateRocketPunch_.IsReady();
		const KamataEngine::Vector3 formationPosition = CalculateUltimateFormationPosition();
		ultimateRocketPunch_.UpdateCharging(deltaTime, formationPosition, bodyYaw_);
		const float formationPoseRatio = ultimateRocketPunch_.GetFormationPoseRatio();
		if (formationPoseRatio > GameMath::kEpsilon) {
			leftArm_.SetUltimateFormationPose(anchor, formationPosition, formationPoseRatio);
			rightArm_.SetUltimateFormationPose(anchor, formationPosition, formationPoseRatio);
		}
		if (!wasUltimateReady && ultimateRocketPunch_.IsReady()) {
			leftArm_.SetFistVisible(false);
			rightArm_.SetFistVisible(false);
		}

		if (ultimateRocketPunch_.IsReady() && !isLeftRocketHeld && !isRightRocketHeld) {
			ultimateRocketPunch_.Launch(GameMath::ForwardFromYaw(bodyYaw_));
			leftArm_.BeginUltimateLock(anchor);
			rightArm_.BeginUltimateLock(anchor);
			isUltimateRecoilActive_ = true;
			ultimateRecoilTimer_ = 0.0f;
			velocity_ = GameMath::Multiply(GameMath::ForwardFromYaw(bodyYaw_), -CombatTuning::kUltimateRecoilInitialSpeed);
			dodgeState_ = DodgeState::Recovery;
			dodgeStateTimer_ = 0.0f;
		}
		return;
	}

	leftArm_.Update(deltaTime, anchor, input.leftArm, allowNewArmCharge);
	rightArm_.Update(deltaTime, anchor, input.rightArm, allowNewArmCharge);
	if (areBothRocketsHeld && AreBothArmsChargedForRocket()) {
		ultimateRocketPunch_.BeginCharging(CalculateUltimateFormationPosition(), bodyYaw_);
	}
}

void Player::UpdateUltimateFlightAndArms(float deltaTime) {
	ultimateRocketPunch_.UpdateFlying(deltaTime);
	const ArmAnchor anchor = CreateArmAnchor();
	if (ultimateRocketPunch_.DidSplitThisFrame()) {
		const KamataEngine::Vector3 splitCenter = ultimateRocketPunch_.GetCollisionSphere().center;
		const KamataEngine::Vector3 flightDirection = ultimateRocketPunch_.GetFlightDirection();
		const KamataEngine::Vector3 splitRight{flightDirection.z, 0.0f, -flightDirection.x};
		const KamataEngine::Vector3 splitOffset = GameMath::Multiply(splitRight, CombatTuning::kUltimateReturnSplitHalfWidth);
		leftArm_.BeginUltimateReturn(anchor, GameMath::Subtract(splitCenter, splitOffset), flightDirection);
		rightArm_.BeginUltimateReturn(anchor, GameMath::Add(splitCenter, splitOffset), flightDirection);
	}
	leftArm_.Update(deltaTime, anchor, {}, false);
	rightArm_.Update(deltaTime, anchor, {}, false);
}

void Player::UpdateUltimateRecoil(float deltaTime) {
	if (IsImpactReactionActive()) {
		UpdateBodyTransform();
		UpdateUltimateFlightAndArms(deltaTime);
		BeginSystemOffline(true);
		return;
	}

	ultimateRecoilTimer_ += deltaTime;
	const float progress = std::clamp(ultimateRecoilTimer_ / CombatTuning::kUltimateRecoilDuration, 0.0f, 1.0f);
	const float recoilSpeed = CombatTuning::kUltimateRecoilInitialSpeed * (1.0f - progress);
	velocity_ = GameMath::Multiply(GameMath::ForwardFromYaw(bodyYaw_), -recoilSpeed);
	position_ = GameMath::Add(position_, GameMath::Multiply(velocity_, deltaTime));
	ClampToBattlefield();
	ResolveResearchInstituteCollision();
	UpdateDodgeBodyLean(deltaTime);
	UpdateBodyTransform();
	UpdateUltimateFlightAndArms(deltaTime);

	if (progress >= 1.0f || (recoilSpeed > GameMath::kEpsilon && GameMath::LengthSquared(GameMath::Subtract(position_, previousPosition_)) <= GameMath::kEpsilon)) {
		BeginSystemOffline(true);
	}
}

void Player::UpdateSystemOffline(float deltaTime) {
	systemOfflineTimer_ = (std::max)(0.0f, systemOfflineTimer_ - deltaTime);
	velocity_ = {0.0f, 0.0f, 0.0f};
	impactState_ = PlayerImpactState::None;
	impactStateTimer_ = 0.0f;
	UpdateDodgeBodyLean(deltaTime);
	UpdateBodyTransform();
	UpdateUltimateFlightAndArms(deltaTime);
	if (isUltimateShutdown_ && (leftArm_.IsFistAway() || rightArm_.IsFistAway())) {
		systemOfflineTimer_ = (std::max)(systemOfflineTimer_, CombatTuning::kCockpitEmergencyRebootDuration + CombatTuning::kUltimateReturnPostDockPause);
	}

	if (systemOfflineTimer_ <= GameMath::kEpsilon && !leftArm_.IsFistAway() && !rightArm_.IsFistAway()) {
		EndSystemOffline();
	}
}

void Player::BeginSystemOffline(bool isUltimateShutdown) {
	CancelAllChargeActions();
	isUltimateRecoilActive_ = false;
	ultimateRecoilTimer_ = 0.0f;
	bossFinisherShutdownDelayTimer_ = -1.0f;
	isSystemOffline_ = true;
	isUltimateShutdown_ = isUltimateShutdown;
	systemOfflineTimer_ = isUltimateShutdown ? CombatTuning::kUltimateSystemOfflineDuration : CombatTuning::kSystemOfflineDuration;
	velocity_ = {0.0f, 0.0f, 0.0f};
	impactState_ = PlayerImpactState::None;
	impactStateTimer_ = 0.0f;
	dodgeState_ = DodgeState::Recovery;
	dodgeStateTimer_ = 0.0f;
	didStartDodgeThisFrame_ = false;
}

void Player::CancelAllChargeActions() {
	ultimateRocketPunch_.CancelCharging();
	const ArmAnchor anchor = CreateArmAnchor();
	leftArm_.CancelCharge(anchor);
	rightArm_.CancelCharge(anchor);
}

void Player::EndSystemOffline() {
	isSystemOffline_ = false;
	isUltimateShutdown_ = false;
	systemOfflineTimer_ = 0.0f;
	restartPulseTimer_ = CombatTuning::kUltimateRestartPulseDuration;
	didRestartSystemThisFrame_ = true;
	dodgeState_ = DodgeState::Ready;
	dodgeStateTimer_ = 0.0f;
	dodgeCooldownTimer_ = 0.0f;
}

#ifndef NDEBUG
void Player::DebugBeginSystemOffline() {
	if (!isSystemOffline_) {
		BeginSystemOffline();
	}
}
#endif

bool Player::AreBothArmsChargedForRocket() const {
	return leftArm_.GetState() == ArmState::Charged && rightArm_.GetState() == ArmState::Charged && leftArm_.GetAttackType() == ArmAttackType::RocketPunch &&
	       rightArm_.GetAttackType() == ArmAttackType::RocketPunch;
}

KamataEngine::Vector3 Player::CalculateUltimateFormationPosition() const {
	const KamataEngine::Vector3 midpoint = GameMath::Multiply(GameMath::Add(leftArm_.GetFistSphere().center, rightArm_.GetFistSphere().center), 0.5f);
	return GameMath::Add(midpoint, GameMath::Multiply(GameMath::ForwardFromYaw(bodyYaw_), CombatTuning::kUltimateFormationForwardOffset));
}

void Player::ClampToBattlefield() {
	if (!CombatTuning::kEnablePlayerBattlefieldBounds) {
		return;
	}

	const float minimumX = CombatTuning::kBattlefieldMinimumX + CombatTuning::kPlayerColliderRadius;
	const float maximumX = CombatTuning::kBattlefieldMaximumX - CombatTuning::kPlayerColliderRadius;
	const float minimumZ = CombatTuning::kBattlefieldMinimumZ + CombatTuning::kPlayerColliderRadius;
	const float maximumZ = CombatTuning::kBattlefieldMaximumZ - CombatTuning::kPlayerColliderRadius;
	if (position_.x < minimumX) {
		position_.x = minimumX;
		velocity_.x = (std::max)(0.0f, velocity_.x);
	} else if (position_.x > maximumX) {
		position_.x = maximumX;
		velocity_.x = (std::min)(0.0f, velocity_.x);
	}
	if (position_.z < minimumZ) {
		position_.z = minimumZ;
		velocity_.z = (std::max)(0.0f, velocity_.z);
	} else if (position_.z > maximumZ) {
		position_.z = maximumZ;
		velocity_.z = (std::min)(0.0f, velocity_.z);
	}
}

void Player::ResolveResearchInstituteCollision() {
	const float minimumDistance = CombatTuning::kResearchInstituteColliderRadius + CombatTuning::kPlayerColliderRadius + CombatTuning::kPlayerResearchInstituteCollisionMargin;
	KamataEngine::Vector3 outwardDirection{position_.x, 0.0f, position_.z - CombatTuning::kResearchInstitutePositionZ};
	const float distanceSquared = GameMath::LengthSquared(outwardDirection);
	if (distanceSquared >= minimumDistance * minimumDistance) {
		return;
	}

	if (distanceSquared > GameMath::kEpsilon) {
		outwardDirection = GameMath::Multiply(outwardDirection, 1.0f / std::sqrt(distanceSquared));
	} else {
		outwardDirection = {previousPosition_.x, 0.0f, previousPosition_.z - CombatTuning::kResearchInstitutePositionZ};
		outwardDirection = GameMath::Normalize(outwardDirection);
		if (GameMath::LengthSquared(outwardDirection) <= GameMath::kEpsilon) {
			outwardDirection = {0.0f, 0.0f, 1.0f};
		}
	}

	position_.x = outwardDirection.x * minimumDistance;
	position_.z = CombatTuning::kResearchInstitutePositionZ + outwardDirection.z * minimumDistance;
	const float inwardSpeed = velocity_.x * outwardDirection.x + velocity_.z * outwardDirection.z;
	if (inwardSpeed < 0.0f) {
		velocity_.x -= outwardDirection.x * inwardSpeed;
		velocity_.z -= outwardDirection.z * inwardSpeed;
	}
}

void Player::UpdateBodyTransform() {
	bodyYawRoot_.SetTranslation(GameMath::Add(position_, {0.0f, CombatTuning::kBodyHalfHeight + startupBodyVisualOffsetY_, 0.0f}));
	bodyYawRoot_.SetRotation({0.0f, bodyYaw_, 0.0f});
	bodyYawRoot_.Update();

	bodyLeanRoot_.SetTranslation({0.0f, 0.0f, 0.0f});
	bodyLeanRoot_.SetRotation({0.0f, 0.0f, bodyRoll_});
	bodyLeanRoot_.Update();

	body_.SetScale({1.0f, 1.0f, 1.0f});
	body_.SetTranslation({0.0f, 0.0f, 0.0f});
	body_.SetRotation({0.0f, 0.0f, 0.0f});
	body_.Update();

	cockpitMount_.SetTranslation({
	    CombatTuning::kCockpitMountOffsetX,
	    CombatTuning::kCockpitMountOffsetY + startupCameraVisualOffsetY_,
	    CombatTuning::kCockpitMountOffsetZ,
	});
	cockpitMount_.SetRotation({0.0f, 0.0f, 0.0f});
	cockpitMount_.Update();
	cockpitMountWorldPosition_ = cockpitMount_.TransformPoint({0.0f, 0.0f, 0.0f});
}
