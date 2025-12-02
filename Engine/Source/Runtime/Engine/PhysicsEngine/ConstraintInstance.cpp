#include "pch.h"
#include "ConstraintInstance.h"
#include "BodyInstance.h"
#include "PhysicsConstraintSetup.h"

FConstraintInstance::FConstraintInstance()
	: JointHandle(nullptr)
	, BodyA(nullptr)
	, BodyB(nullptr)
	, ConstraintSetup(nullptr)
	, bJointCreated(false)
{
}

FConstraintInstance::~FConstraintInstance()
{
	DestroyJoint();
}

bool FConstraintInstance::CreateJoint(PxPhysics* Physics, FBodyInstance* InBodyA, FBodyInstance* InBodyB,
									  const UPhysicsConstraintSetup* Setup)
{
	if (!Physics)
	{
		UE_LOG("[Physics] FConstraintInstance::CreateJoint: Physics is null");
		return false;
	}

	if (!Setup)
	{
		UE_LOG("[Physics] FConstraintInstance::CreateJoint: Setup is null");
		return false;
	}

	// 기존 Joint가 있으면 먼저 파괴
	DestroyJoint();

	BodyA = InBodyA;
	BodyB = InBodyB;
	ConstraintSetup = Setup;

	// Body Actor 가져오기 (null일 수 있음 - world에 고정된 경우)
	PxRigidActor* ActorA = nullptr;
	PxRigidActor* ActorB = nullptr;

	if (BodyA && BodyA->IsValid())
	{
		ActorA = BodyA->GetPhysicsActor();
	}

	if (BodyB && BodyB->IsValid())
	{
		ActorB = BodyB->GetPhysicsActor();
	}

	// PhysX Joint 생성 조건 검증
	if (!ActorA && !ActorB)
	{
		UE_LOG("[Physics] FConstraintInstance::CreateJoint: Both actors are null");
		return false;
	}

	if (ActorA && !ActorA->getScene())
	{
		UE_LOG("[Physics] FConstraintInstance::CreateJoint: ActorA is not in scene");
		return false;
	}

	if (ActorB && !ActorB->getScene())
	{
		UE_LOG("[Physics] FConstraintInstance::CreateJoint: ActorB is not in scene");
		return false;
	}

	if (ActorA && ActorB && ActorA->getScene() != ActorB->getScene())
	{
		UE_LOG("[Physics] FConstraintInstance::CreateJoint: Actors are in different scenes");
		return false;
	}

	if (ActorA && ActorB && ActorA == ActorB)
	{
		UE_LOG("[Physics] FConstraintInstance::CreateJoint: Same actor");
		return false;
	}

	// Constraint Frame 설정
	PxTransform LocalFrameA = PxTransform(PxIdentity);
	PxTransform LocalFrameB = PxTransform(PxIdentity);

	// Position offset
	LocalFrameA.p = PxVec3(Setup->ConstraintPositionInBody1.X,
						   Setup->ConstraintPositionInBody1.Y,
						   Setup->ConstraintPositionInBody1.Z);
	LocalFrameB.p = PxVec3(Setup->ConstraintPositionInBody2.X,
						   Setup->ConstraintPositionInBody2.Y,
						   Setup->ConstraintPositionInBody2.Z);

	// Rotation offset (Euler to Quaternion) - 정규화 필수
	LocalFrameA.q = EulerToQuaternion(Setup->ConstraintRotationInBody1);
	LocalFrameB.q = EulerToQuaternion(Setup->ConstraintRotationInBody2);

	// Quaternion 정규화 (PhysX는 정규화된 쿼터니언 필요)
	LocalFrameA.q.normalize();
	LocalFrameB.q.normalize();

	// PxTransform 유효성 검증
	if (!LocalFrameA.isValid())
	{
		UE_LOG("[Physics] FConstraintInstance::CreateJoint: LocalFrameA is invalid");
		return false;
	}
	if (!LocalFrameB.isValid())
	{
		UE_LOG("[Physics] FConstraintInstance::CreateJoint: LocalFrameB is invalid");
		return false;
	}

	// PxD6Joint 생성
	// Note: Actor가 nullptr이면 월드에 고정됨
	JointHandle = PxD6JointCreate(*Physics, ActorA, LocalFrameA, ActorB, LocalFrameB);

	if (!JointHandle)
	{
		UE_LOG("[Physics] FConstraintInstance::CreateJoint: Failed to create PxD6Joint");
		return false;
	}

	// Limits 설정
	SetLinearLimits(Setup->LinearXMotion, Setup->LinearYMotion, Setup->LinearZMotion, Setup->LinearLimit);
	SetAngularLimits(Setup);

	// Soft Limits
	SetSoftSwingLimit(Setup->bSoftSwingLimit, Setup->SwingStiffness, Setup->SwingDamping);
	SetSoftTwistLimit(Setup->bSoftTwistLimit, Setup->TwistStiffness, Setup->TwistDamping);

	// Break Thresholds
	SetLinearBreakThreshold(Setup->bLinearBreakable, Setup->LinearBreakThreshold);
	SetAngularBreakThreshold(Setup->bAngularBreakable, Setup->AngularBreakThreshold);

	bJointCreated = true;

	return true;
}

void FConstraintInstance::DestroyJoint()
{
	if (JointHandle)
	{
		JointHandle->release();
		JointHandle = nullptr;
	}

	BodyA = nullptr;
	BodyB = nullptr;
	ConstraintSetup = nullptr;
	bJointCreated = false;
}

// ===== Linear Limits =====

void FConstraintInstance::SetLinearLimits(ELinearConstraintMotion XMotion, ELinearConstraintMotion YMotion,
										  ELinearConstraintMotion ZMotion, float Limit)
{
	if (!JointHandle)
	{
		return;
	}

	JointHandle->setMotion(PxD6Axis::eX, ConvertLinearMotion(XMotion));
	JointHandle->setMotion(PxD6Axis::eY, ConvertLinearMotion(YMotion));
	JointHandle->setMotion(PxD6Axis::eZ, ConvertLinearMotion(ZMotion));

	if (Limit > 0.0f)
	{
		PxJointLinearLimitPair LinearLimit(-Limit, Limit, PxSpring(0.0f, 0.0f));
		JointHandle->setLinearLimit(PxD6Axis::eX, LinearLimit);
		JointHandle->setLinearLimit(PxD6Axis::eY, LinearLimit);
		JointHandle->setLinearLimit(PxD6Axis::eZ, LinearLimit);
	}
}

void FConstraintInstance::SetLinearXLimit(ELinearConstraintMotion Motion, float Limit)
{
	if (!JointHandle)
	{
		return;
	}

	JointHandle->setMotion(PxD6Axis::eX, ConvertLinearMotion(Motion));

	if (Motion == ELinearConstraintMotion::Limited && Limit > 0.0f)
	{
		PxJointLinearLimitPair LinearLimit(-Limit, Limit, PxSpring(0.0f, 0.0f));
		JointHandle->setLinearLimit(PxD6Axis::eX, LinearLimit);
	}
}

void FConstraintInstance::SetLinearYLimit(ELinearConstraintMotion Motion, float Limit)
{
	if (!JointHandle)
	{
		return;
	}

	JointHandle->setMotion(PxD6Axis::eY, ConvertLinearMotion(Motion));

	if (Motion == ELinearConstraintMotion::Limited && Limit > 0.0f)
	{
		PxJointLinearLimitPair LinearLimit(-Limit, Limit, PxSpring(0.0f, 0.0f));
		JointHandle->setLinearLimit(PxD6Axis::eY, LinearLimit);
	}
}

void FConstraintInstance::SetLinearZLimit(ELinearConstraintMotion Motion, float Limit)
{
	if (!JointHandle)
	{
		return;
	}

	JointHandle->setMotion(PxD6Axis::eZ, ConvertLinearMotion(Motion));

	if (Motion == ELinearConstraintMotion::Limited && Limit > 0.0f)
	{
		PxJointLinearLimitPair LinearLimit(-Limit, Limit, PxSpring(0.0f, 0.0f));
		JointHandle->setLinearLimit(PxD6Axis::eZ, LinearLimit);
	}
}

// ===== Angular Limits =====

void FConstraintInstance::SetAngularLimits(const UPhysicsConstraintSetup* Setup)
{
	if (!JointHandle || !Setup)
	{
		return;
	}

	// Swing1 (Y축 회전)
	SetSwing1Limit(Setup->Swing1Motion, Setup->Swing1LimitAngle);

	// Swing2 (Z축 회전)
	SetSwing2Limit(Setup->Swing2Motion, Setup->Swing2LimitAngle);

	// Twist (X축 회전)
	SetTwistLimit(Setup->TwistMotion, Setup->TwistLimitAngle);
}

void FConstraintInstance::SetSwing1Limit(EAngularConstraintMotion Motion, float LimitAngle)
{
	if (!JointHandle)
	{
		return;
	}

	JointHandle->setMotion(PxD6Axis::eSWING1, ConvertAngularMotion(Motion));

	if (Motion == EAngularConstraintMotion::Limited)
	{
		// 각도를 라디안으로 변환
		float LimitRad = DegreesToRadians(LimitAngle);
		PxJointLimitCone SwingLimit(LimitRad, LimitRad, PxSpring(0.0f, 0.0f));
		JointHandle->setSwingLimit(SwingLimit);
	}
}

void FConstraintInstance::SetSwing2Limit(EAngularConstraintMotion Motion, float LimitAngle)
{
	if (!JointHandle)
	{
		return;
	}

	JointHandle->setMotion(PxD6Axis::eSWING2, ConvertAngularMotion(Motion));

	// Swing2는 SwingLimit의 두 번째 파라미터로 설정됨
	// SetSwing1Limit에서 이미 설정되므로 여기서는 모션만 설정
}

void FConstraintInstance::SetTwistLimit(EAngularConstraintMotion Motion, float LimitAngle)
{
	if (!JointHandle)
	{
		return;
	}

	JointHandle->setMotion(PxD6Axis::eTWIST, ConvertAngularMotion(Motion));

	if (Motion == EAngularConstraintMotion::Limited)
	{
		float LimitRad = DegreesToRadians(LimitAngle);
		PxJointAngularLimitPair TwistLimit(-LimitRad, LimitRad, PxSpring(0.0f, 0.0f));
		JointHandle->setTwistLimit(TwistLimit);
	}
}

// ===== Soft Limits =====

void FConstraintInstance::SetSoftSwingLimit(bool bEnable, float Stiffness, float Damping)
{
	if (!JointHandle)
	{
		return;
	}

	if (bEnable && ConstraintSetup)
	{
		float Swing1Rad = DegreesToRadians(ConstraintSetup->Swing1LimitAngle);
		float Swing2Rad = DegreesToRadians(ConstraintSetup->Swing2LimitAngle);
		PxJointLimitCone SwingLimit(Swing1Rad, Swing2Rad, PxSpring(Stiffness, Damping));
		JointHandle->setSwingLimit(SwingLimit);
	}
}

void FConstraintInstance::SetSoftTwistLimit(bool bEnable, float Stiffness, float Damping)
{
	if (!JointHandle)
	{
		return;
	}

	if (bEnable && ConstraintSetup)
	{
		float TwistRad = DegreesToRadians(ConstraintSetup->TwistLimitAngle);
		PxJointAngularLimitPair TwistLimit(-TwistRad, TwistRad, PxSpring(Stiffness, Damping));
		JointHandle->setTwistLimit(TwistLimit);
	}
}

// ===== Break Thresholds =====

void FConstraintInstance::SetLinearBreakThreshold(bool bEnable, float Threshold)
{
	if (!JointHandle)
	{
		return;
	}

	if (bEnable)
	{
		JointHandle->setBreakForce(Threshold, PX_MAX_F32);
	}
	else
	{
		JointHandle->setBreakForce(PX_MAX_F32, PX_MAX_F32);
	}
}

void FConstraintInstance::SetAngularBreakThreshold(bool bEnable, float Threshold)
{
	if (!JointHandle)
	{
		return;
	}

	float CurrentLinearBreak, CurrentAngularBreak;
	JointHandle->getBreakForce(CurrentLinearBreak, CurrentAngularBreak);

	if (bEnable)
	{
		JointHandle->setBreakForce(CurrentLinearBreak, Threshold);
	}
	else
	{
		JointHandle->setBreakForce(CurrentLinearBreak, PX_MAX_F32);
	}
}

// ===== Transform 설정 =====

void FConstraintInstance::SetConstraintFrames(const FVector& PosInBody1, const FVector& RotInBody1,
											  const FVector& PosInBody2, const FVector& RotInBody2)
{
	if (!JointHandle)
	{
		return;
	}

	PxTransform LocalFrameA(PxVec3(PosInBody1.X, PosInBody1.Y, PosInBody1.Z),
							EulerToQuaternion(RotInBody1));
	PxTransform LocalFrameB(PxVec3(PosInBody2.X, PosInBody2.Y, PosInBody2.Z),
							EulerToQuaternion(RotInBody2));

	JointHandle->setLocalPose(PxJointActorIndex::eACTOR0, LocalFrameA);
	JointHandle->setLocalPose(PxJointActorIndex::eACTOR1, LocalFrameB);
}

// ===== Private Utilities =====

PxD6Motion::Enum FConstraintInstance::ConvertLinearMotion(ELinearConstraintMotion Motion)
{
	switch (Motion)
	{
	case ELinearConstraintMotion::Free:
		return PxD6Motion::eFREE;
	case ELinearConstraintMotion::Limited:
		return PxD6Motion::eLIMITED;
	case ELinearConstraintMotion::Locked:
	default:
		return PxD6Motion::eLOCKED;
	}
}

PxD6Motion::Enum FConstraintInstance::ConvertAngularMotion(EAngularConstraintMotion Motion)
{
	switch (Motion)
	{
	case EAngularConstraintMotion::Free:
		return PxD6Motion::eFREE;
	case EAngularConstraintMotion::Limited:
		return PxD6Motion::eLIMITED;
	case EAngularConstraintMotion::Locked:
	default:
		return PxD6Motion::eLOCKED;
	}
}

PxQuat FConstraintInstance::EulerToQuaternion(const FVector& EulerDegrees)
{
	// Roll (X), Pitch (Y), Yaw (Z) 순서로 변환
	float RollRad = DegreesToRadians(EulerDegrees.X);
	float PitchRad = DegreesToRadians(EulerDegrees.Y);
	float YawRad = DegreesToRadians(EulerDegrees.Z);

	float CosR = std::cos(RollRad * 0.5f);
	float SinR = std::sin(RollRad * 0.5f);
	float CosP = std::cos(PitchRad * 0.5f);
	float SinP = std::sin(PitchRad * 0.5f);
	float CosY = std::cos(YawRad * 0.5f);
	float SinY = std::sin(YawRad * 0.5f);

	PxQuat Q;
	Q.w = CosR * CosP * CosY + SinR * SinP * SinY;
	Q.x = SinR * CosP * CosY - CosR * SinP * SinY;
	Q.y = CosR * SinP * CosY + SinR * CosP * SinY;
	Q.z = CosR * CosP * SinY - SinR * SinP * CosY;

	return Q;
}
