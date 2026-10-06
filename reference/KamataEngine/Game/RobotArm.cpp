#include "RobotArm.h"

#include "CombatColors.h"
#include "CombatTuning.h"
#include "GameMath.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {

float SmoothStep(float progress) {
	const float clampedProgress = std::clamp(progress, 0.0f, 1.0f);
	return clampedProgress * clampedProgress * (3.0f - 2.0f * clampedProgress);
}

float FlightYaw(const KamataEngine::Vector3& direction, float fallbackYaw) {
	if (direction.x * direction.x + direction.z * direction.z <= GameMath::kEpsilon * GameMath::kEpsilon) {
		return fallbackYaw;
	}
	return std::atan2(direction.x, direction.z);
}

float FlightPitch(const KamataEngine::Vector3& direction) { return -std::asin(std::clamp(direction.y, -1.0f, 1.0f)); }

} // namespace

void RobotArm::Initialize(ArmSide side, KamataEngine::Model* cubeModel, KamataEngine::Model* upperArmModel, KamataEngine::Model* fistModel, KamataEngine::Model* rocketFlameModel) {
	side_ = side;
	upperArm_.Initialize(upperArmModel);
	forearmPoseRoot_.Initialize(cubeModel);
	forearmFist_.Initialize(fistModel);
	rocketFlameObject_.Initialize(rocketFlameModel);
	rocketFlameObject_.SetColor(CombatColors::kMissileTail);
	forearmFist_.SetScale({CombatTuning::kFistModelScale, CombatTuning::kFistModelScale, CombatTuning::kFistModelScale});
}

void RobotArm::Reset(const ArmAnchor& anchor) {
	state_ = ArmState::Ready;
	activeAttackType_ = ArmAttackType::None;
	stateTimer_ = 0.0f;
	chargeTimer_ = 0.0f;
	chargedVisualShakeTimer_ = 0.0f;
	rocketTravelDistance_ = 0.0f;
	rocketMaximumRange_ = CombatTuning::kRocketMaximumRange;
	rocketBaseAvoidanceDistance_ = std::numeric_limits<float>::infinity();
	punchDamageActiveThisFrame_ = false;
	rocketDamageActiveThisFrame_ = false;
	didBeginPunchThisFrame_ = false;
	didLaunchRocketThisFrame_ = false;
	didReconnectThisFrame_ = false;
	isFistVisible_ = true;
	isUltimateReturn_ = false;
	startupVisualOffsetY_ = 0.0f;
	ultimateFormationPoseProgress_ = 0.0f;
	isUltimateFormationPoseActive_ = false;
	attackDirection_ = GameMath::ForwardFromYaw(anchor.bodyYaw);
	rocketPosePitch_ = 0.0f;
	rocketPoseYaw_ = anchor.bodyYaw;
	returnPhaseStartPitch_ = 0.0f;
	returnPhaseStartYaw_ = anchor.bodyYaw;
	returnAscentDistance_ = 0.0f;
	rocketReturnPhase_ = RocketReturnPhase::TurnUp;
	UpdateReadyPose(anchor);
	previousFistCenter_ = fistCenter_;
	returnPhaseStartPosition_ = fistCenter_;
	UpdateTransforms(anchor);
}

void RobotArm::RefreshVisualTransforms(const ArmAnchor& anchor) { UpdateTransforms(anchor); }

void RobotArm::Update(float deltaTime, const ArmAnchor& anchor, const ArmInput& input, bool allowNewCharge) {
	isUltimateFormationPoseActive_ = false;
	ultimateFormationPoseProgress_ = 0.0f;
	previousFistCenter_ = fistCenter_;
	punchDamageActiveThisFrame_ = false;
	rocketDamageActiveThisFrame_ = false;
	didBeginPunchThisFrame_ = false;
	didLaunchRocketThisFrame_ = false;
	didReconnectThisFrame_ = false;

	switch (state_) {
	case ArmState::Ready:
		UpdateReadyPose(anchor);
		if (!allowNewCharge) {
			break;
		}
		if (input.punchHeld) {
			BeginCharge(ArmAttackType::Punch);
		} else if (input.rocketHeld) {
			BeginCharge(ArmAttackType::RocketPunch);
		}
		break;

	case ArmState::Charging: {
		const bool isHeld = activeAttackType_ == ArmAttackType::Punch ? input.punchHeld : input.rocketHeld;
		if (!isHeld) {
			state_ = ArmState::Ready;
			activeAttackType_ = ArmAttackType::None;
			chargeTimer_ = 0.0f;
			UpdateReadyPose(anchor);
			break;
		}

		chargeTimer_ = (std::min)(chargeTimer_ + deltaTime, CombatTuning::kChargeTime);
		fistCenter_ = CalculateChargePosePosition(anchor, GetChargeRatio());
		if (chargeTimer_ >= CombatTuning::kChargeTime) {
			state_ = ArmState::Charged;
		}
		break;
	}

	case ArmState::Charged: {
		const bool isHeld = activeAttackType_ == ArmAttackType::Punch ? input.punchHeld : input.rocketHeld;
		chargeTimer_ = CombatTuning::kChargeTime;
		fistCenter_ = CalculateChargePosePosition(anchor, 1.0f);
		if (!isHeld) {
			CommitAttack(anchor);
		}
		break;
	}

	case ArmState::Punching: {
		punchDamageActiveThisFrame_ = true;
		stateTimer_ += deltaTime;
		const float progress = std::clamp(stateTimer_ / CombatTuning::kPunchDuration, 0.0f, 1.0f);
		const float extension = std::sin(progress * GameMath::kPi) * CombatTuning::kPunchDistance;
		fistCenter_ = GameMath::Add(CalculateHomeFistPosition(anchor), GameMath::Multiply(attackDirection_, extension));
		if (stateTimer_ >= CombatTuning::kPunchDuration) {
			EnterRecovery();
		}
		break;
	}

	case ArmState::RocketFlying: {
		rocketDamageActiveThisFrame_ = true;
		const float remainingDistance = rocketMaximumRange_ - rocketTravelDistance_;
		const float remainingBaseAvoidanceDistance = (std::max)(0.0f, rocketBaseAvoidanceDistance_ - rocketTravelDistance_);
		const float travelDistance = (std::min)(CombatTuning::kRocketSpeed * deltaTime, (std::min)(remainingDistance, remainingBaseAvoidanceDistance));
		fistCenter_ = GameMath::Add(fistCenter_, GameMath::Multiply(attackDirection_, travelDistance));
		rocketTravelDistance_ += travelDistance;
		if (rocketTravelDistance_ >= rocketBaseAvoidanceDistance_ - GameMath::kEpsilon) {
			BeginRocketReturn(true);
			rocketDamageActiveThisFrame_ = false;
		} else if (rocketTravelDistance_ >= rocketMaximumRange_) {
			BeginRocketReturn(false);
		}
		break;
	}

	case ArmState::RocketEvacuating:
	case ArmState::Returning:
		UpdateRocketReturn(deltaTime, anchor);
		break;

	case ArmState::UltimateLocked:
		fistCenter_ = CalculateHomeFistPosition(anchor);
		break;

	case ArmState::Recovery:
		stateTimer_ += deltaTime;
		fistCenter_ = CalculateHomeFistPosition(anchor);
		if (stateTimer_ >= CombatTuning::kRecoveryDuration) {
			state_ = ArmState::Ready;
			activeAttackType_ = ArmAttackType::None;
			chargeTimer_ = 0.0f;
		}
		break;
	}

	if (state_ == ArmState::Charged) {
		chargedVisualShakeTimer_ += deltaTime;
	} else {
		chargedVisualShakeTimer_ = 0.0f;
	}

	UpdateTransforms(anchor);
}

void RobotArm::Draw() {
	upperArm_.Draw();
	if (IsRocketFlameActive()) {
		rocketFlameObject_.Draw();
	}
	if (isFistVisible_ && state_ != ArmState::UltimateLocked) {
		forearmFist_.Draw();
	}
}

void RobotArm::CancelCharge(const ArmAnchor& anchor) {
	if (state_ != ArmState::Charging && state_ != ArmState::Charged) {
		return;
	}

	state_ = ArmState::Ready;
	activeAttackType_ = ArmAttackType::None;
	stateTimer_ = 0.0f;
	chargeTimer_ = 0.0f;
	chargedVisualShakeTimer_ = 0.0f;
	isFistVisible_ = true;
	isUltimateFormationPoseActive_ = false;
	ultimateFormationPoseProgress_ = 0.0f;
	UpdateReadyPose(anchor);
	previousFistCenter_ = fistCenter_;
	UpdateTransforms(anchor);
}

void RobotArm::SetUltimateFormationPose(const ArmAnchor& anchor, const KamataEngine::Vector3& formationPosition, float progress) {
	if (state_ != ArmState::Charged) {
		return;
	}

	ultimateFormationPoseProgress_ = std::clamp(progress, 0.0f, 1.0f);
	isUltimateFormationPoseActive_ = ultimateFormationPoseProgress_ > GameMath::kEpsilon;
	if (!isUltimateFormationPoseActive_) {
		return;
	}

	fistCenter_ = GameMath::Lerp(CalculateRocketDockFistPosition(anchor), formationPosition, ultimateFormationPoseProgress_);
	UpdateTransforms(anchor);
}

void RobotArm::BeginUltimateLock(const ArmAnchor& anchor) {
	isFistVisible_ = false;
	state_ = ArmState::UltimateLocked;
	activeAttackType_ = ArmAttackType::RocketPunch;
	stateTimer_ = 0.0f;
	chargeTimer_ = 0.0f;
	fistCenter_ = CalculateHomeFistPosition(anchor);
	previousFistCenter_ = fistCenter_;
	UpdateTransforms(anchor);
}

void RobotArm::BeginUltimateReturn(const ArmAnchor& anchor, const KamataEngine::Vector3& splitPosition, const KamataEngine::Vector3& flightDirection) {
	isFistVisible_ = true;
	activeAttackType_ = ArmAttackType::RocketPunch;
	fistCenter_ = splitPosition;
	previousFistCenter_ = fistCenter_;
	rocketPosePitch_ = FlightPitch(flightDirection);
	rocketPoseYaw_ = FlightYaw(flightDirection, anchor.bodyYaw);
	BeginRocketReturn(false);
	isUltimateReturn_ = true;
	UpdateTransforms(anchor);
}

float RobotArm::GetChargeRatio() const {
	if (state_ == ArmState::Charged) {
		return 1.0f;
	}
	if (state_ != ArmState::Charging) {
		return 0.0f;
	}
	return std::clamp(chargeTimer_ / CombatTuning::kChargeTime, 0.0f, 1.0f);
}

const char* RobotArm::GetStateName() const {
	switch (state_) {
	case ArmState::Ready:
		return "Ready";
	case ArmState::Charging:
		return "Charging";
	case ArmState::Charged:
		return "Charged";
	case ArmState::Punching:
		return "Punching";
	case ArmState::RocketFlying:
		return "RocketFlying";
	case ArmState::RocketEvacuating:
		return "RocketEvacuating";
	case ArmState::Returning:
		return "Returning";
	case ArmState::UltimateLocked:
		return "UltimateLocked";
	case ArmState::Recovery:
		return "Recovery";
	}
	return "Unknown";
}

Collision::Sphere RobotArm::GetFistSphere() const { return {fistCenter_, CombatTuning::kFistRadius}; }

RocketTrajectory RobotArm::GetRocketPreviewTrajectory(float bodyYaw) const {
	const KamataEngine::Vector3 direction = GameMath::ForwardFromYaw(bodyYaw);
	const float maximumRange = CalculateRocketMaximumRange(fistCenter_, direction);
	const float baseAvoidanceDistance = CalculateRocketBaseAvoidanceDistance(fistCenter_, direction);
	return {fistCenter_, direction, (std::min)(maximumRange, baseAvoidanceDistance)};
}

void RobotArm::BeginCharge(ArmAttackType attackType) {
	state_ = ArmState::Charging;
	activeAttackType_ = attackType;
	chargeTimer_ = 0.0f;
	stateTimer_ = 0.0f;
}

void RobotArm::CommitAttack(const ArmAnchor& anchor) {
	const KamataEngine::Vector3 bodyForward = GameMath::ForwardFromYaw(anchor.bodyYaw);
	stateTimer_ = 0.0f;
	if (activeAttackType_ == ArmAttackType::Punch) {
		const KamataEngine::Vector3 punchTarget =
		    GameMath::Add(GameMath::Add(anchor.playerPosition, {0.0f, CombatTuning::kShoulderHeight, 0.0f}), GameMath::Multiply(bodyForward, CombatTuning::kPunchConvergenceTargetForwardDistance));
		attackDirection_ = GameMath::Normalize(GameMath::Subtract(punchTarget, CalculateHomeFistPosition(anchor)));
		if (GameMath::LengthSquared(attackDirection_) <= GameMath::kEpsilon) {
			attackDirection_ = bodyForward;
		}
		state_ = ArmState::Punching;
		didBeginPunchThisFrame_ = true;
	} else {
		attackDirection_ = bodyForward;
		rocketPosePitch_ = 0.0f;
		rocketPoseYaw_ = anchor.bodyYaw;
		state_ = ArmState::RocketFlying;
		rocketTravelDistance_ = 0.0f;
		rocketMaximumRange_ = CalculateRocketMaximumRange(fistCenter_, attackDirection_);
		rocketBaseAvoidanceDistance_ = CalculateRocketBaseAvoidanceDistance(fistCenter_, attackDirection_);
		didLaunchRocketThisFrame_ = true;
	}
}

void RobotArm::BeginRocketReturn(bool avoidBase) {
	state_ = avoidBase ? ArmState::RocketEvacuating : ArmState::Returning;
	isUltimateReturn_ = false;
	returnAscentDistance_ = avoidBase ? CombatTuning::kRocketBaseEvacuationDistance : CombatTuning::kRocketReturnAscentDistance;
	rocketReturnPhase_ = RocketReturnPhase::TurnUp;
	stateTimer_ = 0.0f;
	returnPhaseStartPosition_ = fistCenter_;
	returnPhaseStartPitch_ = rocketPosePitch_;
	returnPhaseStartYaw_ = rocketPoseYaw_;
}

void RobotArm::AdvanceRocketReturnPhase(RocketReturnPhase phase) {
	rocketReturnPhase_ = phase;
	stateTimer_ = 0.0f;
	returnPhaseStartPosition_ = fistCenter_;
	returnPhaseStartPitch_ = rocketPosePitch_;
	returnPhaseStartYaw_ = rocketPoseYaw_;
	if (phase == RocketReturnPhase::TurnHome) {
		state_ = ArmState::Returning;
	}
}

void RobotArm::UpdateRocketReturn(float deltaTime, const ArmAnchor& anchor) {
	switch (rocketReturnPhase_) {
	case RocketReturnPhase::TurnUp: {
		stateTimer_ += deltaTime;
		const float progress = SmoothStep(stateTimer_ / CombatTuning::kRocketReturnTurnUpDuration);
		rocketPosePitch_ = returnPhaseStartPitch_ + (-GameMath::kPi * 0.5f - returnPhaseStartPitch_) * progress;
		const float turnAscentDistance = (std::min)(returnAscentDistance_, CombatTuning::kRocketReturnTurnUpAscentDistance);
		fistCenter_.y = returnPhaseStartPosition_.y + turnAscentDistance * progress;
		if (stateTimer_ >= CombatTuning::kRocketReturnTurnUpDuration) {
			AdvanceRocketReturnPhase(RocketReturnPhase::Ascend);
		}
		break;
	}
	case RocketReturnPhase::Ascend: {
		const float ascendedDistance = fistCenter_.y - returnPhaseStartPosition_.y;
		const float turnAscentDistance = (std::min)(returnAscentDistance_, CombatTuning::kRocketReturnTurnUpAscentDistance);
		const float remainingDistance = (std::max)(0.0f, returnAscentDistance_ - turnAscentDistance - ascendedDistance);
		const float step = (std::min)(CombatTuning::kRocketReturnAscentSpeed * deltaTime, remainingDistance);
		fistCenter_.y += step;
		if (step >= remainingDistance - GameMath::kEpsilon) {
			AdvanceRocketReturnPhase(RocketReturnPhase::TurnHome);
		}
		break;
	}
	case RocketReturnPhase::TurnHome: {
		stateTimer_ += deltaTime;
		const float progress = SmoothStep(stateTimer_ / CombatTuning::kRocketReturnTurnHomeDuration);
		const KamataEngine::Vector3 direction = GameMath::Normalize(GameMath::Subtract(CalculateReturnEntryPosition(anchor), fistCenter_));
		if (GameMath::LengthSquared(direction) > GameMath::kEpsilon) {
			const float targetYaw = FlightYaw(direction, returnPhaseStartYaw_);
			const float targetPitch = FlightPitch(direction);
			rocketPoseYaw_ = GameMath::NormalizeAngle(returnPhaseStartYaw_ + GameMath::NormalizeAngle(targetYaw - returnPhaseStartYaw_) * progress);
			rocketPosePitch_ = returnPhaseStartPitch_ + (targetPitch - returnPhaseStartPitch_) * progress;
		}
		if (stateTimer_ >= CombatTuning::kRocketReturnTurnHomeDuration) {
			AdvanceRocketReturnPhase(RocketReturnPhase::FlyHome);
		}
		break;
	}
	case RocketReturnPhase::FlyHome: {
		const KamataEngine::Vector3 toEntry = GameMath::Subtract(CalculateReturnEntryPosition(anchor), fistCenter_);
		const float distance = GameMath::Length(toEntry);
		if (distance <= GameMath::kEpsilon) {
			AdvanceRocketReturnPhase(RocketReturnPhase::Align);
			break;
		}
		const KamataEngine::Vector3 direction = GameMath::Multiply(toEntry, 1.0f / distance);
		const float step = (std::min)(CombatTuning::kRocketReturnFlightSpeed * deltaTime, distance);
		fistCenter_ = GameMath::Add(fistCenter_, GameMath::Multiply(direction, step));
		const float maximumTurn = CombatTuning::kRocketReturnTrackingTurnSpeed * deltaTime;
		rocketPoseYaw_ = GameMath::MoveTowardsAngle(rocketPoseYaw_, FlightYaw(direction, rocketPoseYaw_), maximumTurn);
		rocketPosePitch_ = GameMath::MoveTowards(rocketPosePitch_, FlightPitch(direction), maximumTurn);
		if (step >= distance - GameMath::kEpsilon) {
			AdvanceRocketReturnPhase(RocketReturnPhase::Align);
		}
		break;
	}
	case RocketReturnPhase::Align: {
		stateTimer_ += deltaTime;
		const float progress = SmoothStep(stateTimer_ / CombatTuning::kRocketReturnAlignDuration);
		fistCenter_ = returnPhaseStartPosition_;
		rocketPosePitch_ = returnPhaseStartPitch_ + (-GameMath::kPi * 0.5f - returnPhaseStartPitch_) * progress;
		if (stateTimer_ >= CombatTuning::kRocketReturnAlignDuration) {
			rocketPosePitch_ = -GameMath::kPi * 0.5f;
			AdvanceRocketReturnPhase(isUltimateReturn_ ? RocketReturnPhase::UltimateDock : RocketReturnPhase::Dock);
		}
		break;
	}
	case RocketReturnPhase::Dock:
	case RocketReturnPhase::UltimateDock: {
		stateTimer_ += deltaTime;
		const float duration = rocketReturnPhase_ == RocketReturnPhase::UltimateDock ? CombatTuning::kRocketUltimateDockDuration : CombatTuning::kRocketReturnDockDuration;
		const float progress = SmoothStep(stateTimer_ / duration);
		const KamataEngine::Vector3 homePosition = CalculateHomeFistPosition(anchor);
		fistCenter_ = GameMath::Lerp(returnPhaseStartPosition_, homePosition, progress);
		if (rocketReturnPhase_ == RocketReturnPhase::UltimateDock) {
			rocketPosePitch_ = returnPhaseStartPitch_ + (-GameMath::kPi * 0.5f - returnPhaseStartPitch_) * progress;
		} else {
			rocketPosePitch_ = -GameMath::kPi * 0.5f;
		}
		if (stateTimer_ >= duration) {
			fistCenter_ = homePosition;
			didReconnectThisFrame_ = true;
			activeAttackType_ = ArmAttackType::None;
			isUltimateReturn_ = false;
			EnterRecovery();
		}
		break;
	}
	}
}

void RobotArm::EnterRecovery() {
	state_ = ArmState::Recovery;
	stateTimer_ = 0.0f;
}

void RobotArm::UpdateReadyPose(const ArmAnchor& anchor) { fistCenter_ = CalculateHomeFistPosition(anchor); }

KamataEngine::Vector3 RobotArm::CalculateChargePosePosition(const ArmAnchor& anchor, float chargeRatio) const {
	const KamataEngine::Vector3 homePosition = CalculateHomeFistPosition(anchor);
	if (activeAttackType_ == ArmAttackType::RocketPunch) {
		return GameMath::Lerp(homePosition, CalculateRocketDockFistPosition(anchor), std::clamp(chargeRatio, 0.0f, 1.0f));
	}

	const float retractDistance = CombatTuning::kChargeRetractDistance * std::clamp(chargeRatio, 0.0f, 1.0f);
	return GameMath::Add(homePosition, GameMath::Multiply(GameMath::ForwardFromYaw(anchor.bodyYaw), -retractDistance));
}

void RobotArm::UpdateTransforms(const ArmAnchor& anchor) {
	const KamataEngine::Vector3 shoulderPosition = CalculateShoulderPosition(anchor);
	const bool usesRocketExtendedPose = UsesRocketExtendedPose();
	const KamataEngine::Vector3 upperArmTarget = usesRocketExtendedPose ? CalculateRocketDockFistPosition(anchor) : fistCenter_;
	const KamataEngine::Vector3 elbowPosition = CalculateElbowPosition(anchor, shoulderPosition, upperArmTarget);
	const KamataEngine::Vector3 visualOffset{0.0f, startupVisualOffsetY_, 0.0f};
	const KamataEngine::Vector3 visualShoulderPosition = GameMath::Add(shoulderPosition, visualOffset);
	const KamataEngine::Vector3 visualElbowPosition = GameMath::Add(elbowPosition, visualOffset);
	const KamataEngine::Vector3 visualFistCenter = GameMath::Add(fistCenter_, visualOffset);
	KamataEngine::Vector3 upperArmDirection = GameMath::Normalize(GameMath::Subtract(elbowPosition, shoulderPosition));
	if (GameMath::LengthSquared(upperArmDirection) <= GameMath::kEpsilon) {
		upperArmDirection = GameMath::ForwardFromYaw(anchor.bodyYaw);
	}
	const KamataEngine::Vector3 upperArmWorldPosition = GameMath::Add(visualShoulderPosition, GameMath::Multiply(upperArmDirection, CombatTuning::kUpperArmModelShoulderToOriginDistance));
	upperArm_.SetParentWorldTransform(anchor.visualRoot);
	upperArm_.SetScale({
	    CombatTuning::kUpperArmModelScale,
	    CombatTuning::kUpperArmModelScale,
	    CombatTuning::kUpperArmModelScale,
	});
	upperArm_.SetTranslation(anchor.visualRoot != nullptr ? ConvertWorldPositionToVisualLocal(anchor, upperArmWorldPosition) : upperArmWorldPosition);
	upperArm_.SetRotation(CalculateSegmentRotation(anchor, visualShoulderPosition, visualElbowPosition));
	upperArm_.Update();

	const bool isVisuallyAttached = !IsFistAway() && anchor.visualRoot != nullptr;
	forearmFist_.SetScale({
	    CombatTuning::kFistModelScale,
	    CombatTuning::kFistModelScale,
	    CombatTuning::kFistModelScale,
	});
	if (isVisuallyAttached) {
		forearmPoseRoot_.SetParentWorldTransform(anchor.visualRoot);
		forearmPoseRoot_.SetTranslation(ConvertWorldPositionToVisualLocal(anchor, visualFistCenter));
		forearmPoseRoot_.SetRotation(CalculateSegmentRotation(anchor, visualElbowPosition, visualFistCenter));
		forearmPoseRoot_.Update();

		forearmFist_.SetParentWorldTransform(&forearmPoseRoot_.GetWorldTransform());
		forearmFist_.SetTranslation(CalculateChargedVisualShakeOffset());
		if (isUltimateFormationPoseActive_) {
			const float sideSign = side_ == ArmSide::Left ? -1.0f : 1.0f;
			forearmFist_.SetRotation({
			    0.0f,
			    0.0f,
			    -sideSign * CombatTuning::kUltimateFormationInwardRoll * ultimateFormationPoseProgress_,
			});
		} else if (state_ == ArmState::Ready || state_ == ArmState::Recovery) {
			const float sideSign = side_ == ArmSide::Left ? -1.0f : 1.0f;
			forearmFist_.SetRotation({0.0f, 0.0f, -sideSign * CombatTuning::kIdleFistInwardRoll});
		} else {
			forearmFist_.SetRotation({});
		}
	} else {
		forearmPoseRoot_.SetParentWorldTransform(nullptr);
		forearmFist_.SetParentWorldTransform(nullptr);
		forearmFist_.SetTranslation(visualFistCenter);
		forearmFist_.SetRotation({rocketPosePitch_, rocketPoseYaw_, 0.0f});
	}
	forearmFist_.Update();
	UpdateRocketFlameTransform();
}

KamataEngine::Vector3 RobotArm::CalculateChargedVisualShakeOffset() const {
	if (state_ != ArmState::Charged) {
		return {};
	}

	const float sidePhase = side_ == ArmSide::Left ? 0.0f : GameMath::kPi;
	const float phase = chargedVisualShakeTimer_ * CombatTuning::kChargedFistShakeFrequency;
	const float amplitude = CombatTuning::kChargedFistShakeAmplitude;
	return {
	    std::sin(phase + sidePhase) * amplitude,
	    std::sin(phase * 1.71f + sidePhase + 0.8f) * amplitude * 0.65f,
	    std::sin(phase * 1.29f + sidePhase + 1.6f) * amplitude * 0.8f,
	};
}

KamataEngine::Vector3 RobotArm::ConvertWorldPositionToVisualLocal(const ArmAnchor& anchor, const KamataEngine::Vector3& worldPosition) const {
	const KamataEngine::Vector3 offset = GameMath::Subtract(worldPosition, anchor.visualRootWorldPosition);
	return {
	    GameMath::Dot(offset, GameMath::RightFromYaw(anchor.bodyYaw)),
	    offset.y,
	    GameMath::Dot(offset, GameMath::ForwardFromYaw(anchor.bodyYaw)),
	};
}

KamataEngine::Vector3 RobotArm::CalculateShoulderPosition(const ArmAnchor& anchor) const {
	const float sideSign = side_ == ArmSide::Left ? -1.0f : 1.0f;
	const KamataEngine::Vector3 sideOffset = GameMath::Multiply(GameMath::RightFromYaw(anchor.bodyYaw), CombatTuning::kShoulderHalfWidth * sideSign);
	return GameMath::Add(GameMath::Add(anchor.playerPosition, {0.0f, CombatTuning::kShoulderHeight, 0.0f}), sideOffset);
}

KamataEngine::Vector3 RobotArm::CalculateHomeFistPosition(const ArmAnchor& anchor) const {
	return GameMath::Add(CalculateShoulderPosition(anchor), GameMath::Multiply(GameMath::ForwardFromYaw(anchor.bodyYaw), CombatTuning::kReadyFistForwardOffset));
}

KamataEngine::Vector3 RobotArm::CalculateRocketDockFistPosition(const ArmAnchor& anchor) const {
	return GameMath::Add(CalculateShoulderPosition(anchor), GameMath::Multiply(GameMath::ForwardFromYaw(anchor.bodyYaw), CombatTuning::kRocketChargeFinalForwardOffset));
}

KamataEngine::Vector3 RobotArm::CalculateElbowPosition(const ArmAnchor& anchor, const KamataEngine::Vector3& shoulderPosition, const KamataEngine::Vector3& upperArmTarget) const {
	const auto calculateIdleElbow = [&](const KamataEngine::Vector3& targetPosition) {
		const KamataEngine::Vector3 midpoint = GameMath::Multiply(GameMath::Add(shoulderPosition, targetPosition), 0.5f);
		const KamataEngine::Vector3 horizontalOffset{targetPosition.x - shoulderPosition.x, 0.0f, targetPosition.z - shoulderPosition.z};
		const float halfHorizontalDistance = GameMath::Length(horizontalOffset) * 0.5f;
		const float elbowDrop = std::sqrt((std::max)(0.0f, CombatTuning::kUpperArmFixedLength * CombatTuning::kUpperArmFixedLength - halfHorizontalDistance * halfHorizontalDistance));
		return KamataEngine::Vector3{midpoint.x, shoulderPosition.y - elbowDrop, midpoint.z};
	};

	if (activeAttackType_ == ArmAttackType::RocketPunch) {
		if (state_ == ArmState::Returning) {
			const float heightAboveShoulder = (std::max)(0.0f, fistCenter_.y - shoulderPosition.y);
			if (heightAboveShoulder <= CombatTuning::kRocketReloadUpperArmFollowStartHeight) {
				const float followProgress = std::clamp(1.0f - heightAboveShoulder / CombatTuning::kRocketReloadUpperArmFollowStartHeight, 0.0f, 1.0f);
				const float easedProgress = followProgress * followProgress * (3.0f - 2.0f * followProgress);
				const KamataEngine::Vector3 rocketElbow = GameMath::Lerp(shoulderPosition, CalculateRocketDockFistPosition(anchor), CombatTuning::kRocketExtendedElbowRatio);
				KamataEngine::Vector3 blendedDirection =
				    GameMath::Normalize(GameMath::Lerp(GameMath::Subtract(rocketElbow, shoulderPosition), GameMath::Subtract(calculateIdleElbow(fistCenter_), shoulderPosition), easedProgress));
				if (GameMath::LengthSquared(blendedDirection) <= GameMath::kEpsilon) {
					blendedDirection = GameMath::ForwardFromYaw(anchor.bodyYaw);
				}
				return GameMath::Add(shoulderPosition, GameMath::Multiply(blendedDirection, CombatTuning::kUpperArmFixedLength));
			}
		}
		return GameMath::Lerp(shoulderPosition, upperArmTarget, CombatTuning::kRocketExtendedElbowRatio);
	}

	if ((state_ == ArmState::Charging || state_ == ArmState::Charged) && activeAttackType_ == ArmAttackType::Punch) {
		const float sideSign = side_ == ArmSide::Left ? -1.0f : 1.0f;
		const KamataEngine::Vector3 midpoint = GameMath::Multiply(GameMath::Add(shoulderPosition, upperArmTarget), 0.5f);
		const KamataEngine::Vector3 horizontalElbow = GameMath::Add(midpoint, GameMath::Multiply(GameMath::RightFromYaw(anchor.bodyYaw), sideSign * CombatTuning::kPunchChargeElbowOutwardOffset));
		const KamataEngine::Vector3 horizontalOffset{
		    horizontalElbow.x - shoulderPosition.x,
		    0.0f,
		    horizontalElbow.z - shoulderPosition.z,
		};
		const float elbowDrop = std::sqrt((std::max)(0.0f, CombatTuning::kUpperArmFixedLength * CombatTuning::kUpperArmFixedLength - GameMath::LengthSquared(horizontalOffset)));
		return {horizontalElbow.x, shoulderPosition.y - elbowDrop, horizontalElbow.z};
	}

	return calculateIdleElbow(upperArmTarget);
}

KamataEngine::Vector3 RobotArm::CalculateSegmentRotation(const ArmAnchor& anchor, const KamataEngine::Vector3& startPosition, const KamataEngine::Vector3& endPosition) const {
	const KamataEngine::Vector3 direction = GameMath::Normalize(GameMath::Subtract(endPosition, startPosition));
	if (GameMath::LengthSquared(direction) <= GameMath::kEpsilon) {
		return anchor.visualRoot != nullptr ? KamataEngine::Vector3{} : KamataEngine::Vector3{0.0f, anchor.bodyYaw, 0.0f};
	}

	const float worldYaw = std::atan2(direction.x, direction.z);
	const float pitch = -std::asin(std::clamp(direction.y, -1.0f, 1.0f));
	return {
	    pitch,
	    anchor.visualRoot != nullptr ? GameMath::NormalizeAngle(worldYaw - anchor.bodyYaw) : worldYaw,
	    0.0f,
	};
}

bool RobotArm::UsesRocketExtendedPose() const {
	return activeAttackType_ == ArmAttackType::RocketPunch && (state_ == ArmState::RocketFlying || state_ == ArmState::RocketEvacuating || state_ == ArmState::Returning);
}

bool RobotArm::IsRocketFlameActive() const { return state_ == ArmState::RocketFlying || state_ == ArmState::RocketEvacuating || state_ == ArmState::Returning; }

void RobotArm::UpdateRocketFlameTransform() {
	if (!IsRocketFlameActive()) {
		return;
	}

	const KamataEngine::Vector3 flameDirection = GameMath::Multiply(GameMath::ForwardFromYawPitch(rocketPoseYaw_, rocketPosePitch_), -1.0f);
	float flameScale = CombatTuning::kRocketReturnFlameScale;
	if (state_ == ArmState::RocketFlying) {
		const float remainingFlightDistance = (std::max)(0.0f, (std::min)(rocketMaximumRange_, rocketBaseAvoidanceDistance_) - rocketTravelDistance_);
		const float fadeProgress = SmoothStep(1.0f - remainingFlightDistance / CombatTuning::kRocketFlamePreReturnFadeDistance);
		flameScale = CombatTuning::kRocketFlightFlameScale + (CombatTuning::kRocketReturnTurnFlameScale - CombatTuning::kRocketFlightFlameScale) * fadeProgress;
	} else if (rocketReturnPhase_ == RocketReturnPhase::TurnUp) {
		flameScale = CombatTuning::kRocketReturnTurnFlameScale;
	} else if (rocketReturnPhase_ == RocketReturnPhase::Ascend) {
		const float ascendedDistance = (std::max)(0.0f, fistCenter_.y - returnPhaseStartPosition_.y);
		const float restoreProgress = SmoothStep(ascendedDistance / CombatTuning::kRocketReturnFlameRestoreAscentDistance);
		flameScale = CombatTuning::kRocketReturnTurnFlameScale + (CombatTuning::kRocketReturnFlameScale - CombatTuning::kRocketReturnTurnFlameScale) * restoreProgress;
	}
	const float flameLengthScale = CombatTuning::kRocketFlameLengthScale * flameScale;
	const float flameOriginDistance = CombatTuning::kFistRadius + CombatTuning::kRocketFlameNozzleOffset + CombatTuning::kRocketFlameModelNegativeAxisExtent * flameLengthScale;
	const KamataEngine::Vector3 flamePosition = GameMath::Add(GameMath::Add(fistCenter_, {0.0f, startupVisualOffsetY_, 0.0f}), GameMath::Multiply(flameDirection, flameOriginDistance));
	rocketFlameObject_.SetScale({
	    CombatTuning::kRocketFlameRadialScale * flameScale,
	    flameLengthScale,
	    CombatTuning::kRocketFlameRadialScale * flameScale,
	});
	rocketFlameObject_.SetTranslation(flamePosition);
	if (state_ == ArmState::Returning && rocketReturnPhase_ == RocketReturnPhase::UltimateDock) {
		rocketFlameObject_.SetRotation({GameMath::kPi, 0.0f, 0.0f});
	} else {
		rocketFlameObject_.SetRotation({GameMath::kPi * 0.5f - rocketPosePitch_, GameMath::NormalizeAngle(rocketPoseYaw_ + GameMath::kPi), 0.0f});
	}
	rocketFlameObject_.Update();
}

KamataEngine::Vector3 RobotArm::CalculateReturnEntryPosition(const ArmAnchor& anchor) const {
	const float entryHeight = isUltimateReturn_ ? CombatTuning::kRocketUltimateReturnEntryHeightAboveShoulder : CombatTuning::kRocketReturnEntryHeightAboveShoulder;
	return GameMath::Add(CalculateHomeFistPosition(anchor), {0.0f, entryHeight, 0.0f});
}

float RobotArm::CalculateRocketMaximumRange(const KamataEngine::Vector3& start, const KamataEngine::Vector3& direction) const {
	float entryDistance = -std::numeric_limits<float>::infinity();
	float exitDistance = std::numeric_limits<float>::infinity();
	const auto clipAxis = [&](float startValue, float directionValue, float minimum, float maximum) {
		if (std::abs(directionValue) <= GameMath::kEpsilon) {
			return startValue >= minimum && startValue <= maximum;
		}
		float nearDistance = (minimum - startValue) / directionValue;
		float farDistance = (maximum - startValue) / directionValue;
		if (nearDistance > farDistance) {
			std::swap(nearDistance, farDistance);
		}
		entryDistance = (std::max)(entryDistance, nearDistance);
		exitDistance = (std::min)(exitDistance, farDistance);
		return entryDistance <= exitDistance;
	};

	if (!clipAxis(start.x, direction.x, CombatTuning::kBattlefieldMinimumX, CombatTuning::kBattlefieldMaximumX) ||
	    !clipAxis(start.z, direction.z, CombatTuning::kBattlefieldMinimumZ, CombatTuning::kBattlefieldMaximumZ)) {
		return CombatTuning::kRocketBattlefieldExitMargin;
	}
	return std::clamp(exitDistance + CombatTuning::kRocketBattlefieldExitMargin, 0.0f, CombatTuning::kRocketMaximumRange);
}

float RobotArm::CalculateRocketBaseAvoidanceDistance(const KamataEngine::Vector3& start, const KamataEngine::Vector3& direction) const {
	const KamataEngine::Vector3 baseCenter{0.0f, 0.0f, CombatTuning::kResearchInstitutePositionZ};
	const KamataEngine::Vector3 fromBase = GameMath::Subtract(start, baseCenter);
	const float avoidanceRadius = CombatTuning::kResearchInstituteColliderRadius + CombatTuning::kFistRadius + CombatTuning::kRocketBaseAvoidanceMargin;
	const float projection = GameMath::Dot(fromBase, direction);
	const float perpendicularDistanceSquared = GameMath::LengthSquared(fromBase) - projection * projection;
	const float radiusSquared = avoidanceRadius * avoidanceRadius;
	if (perpendicularDistanceSquared > radiusSquared) {
		return std::numeric_limits<float>::infinity();
	}

	const float halfChordLength = std::sqrt((std::max)(0.0f, radiusSquared - perpendicularDistanceSquared));
	const float exitDistance = -projection + halfChordLength;
	if (exitDistance < 0.0f) {
		return std::numeric_limits<float>::infinity();
	}
	return (std::max)(0.0f, -projection - halfChordLength);
}
