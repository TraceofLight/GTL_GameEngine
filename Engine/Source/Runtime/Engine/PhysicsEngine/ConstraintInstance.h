#pragma once
#include "PxPhysicsAPI.h"
#include "PhysicsConstraintSetup.h"

using namespace physx;

struct FBodyInstance;
class UPhysicsConstraintSetup;

/**
 * @brief Physics Constraint Instance
 * @details 두 물리 바디 사이의 PxD6Joint를 관리하는 런타임 인스턴스
 *
 * @param JointHandle PhysX D6 Joint 포인터
 * @param BodyA 첫 번째 (Parent) 바디 인스턴스
 * @param BodyB 두 번째 (Child) 바디 인스턴스
 * @param ConstraintSetup 이 인스턴스의 설정 데이터
 * @param bJointCreated Joint가 성공적으로 생성되었는지 여부
 */
struct FConstraintInstance
{
public:
	FConstraintInstance();
	~FConstraintInstance();

	// Joint 생성/파괴
	bool CreateJoint(PxPhysics* Physics, FBodyInstance* InBodyA, FBodyInstance* InBodyB,
					 const UPhysicsConstraintSetup* Setup);
	void DestroyJoint();

	// Joint가 유효한지 확인
	bool IsValid() const { return JointHandle != nullptr; }

	// ===== Limit 설정 함수들 =====

	// Linear Limits 설정
	void SetLinearLimits(ELinearConstraintMotion XMotion, ELinearConstraintMotion YMotion,
						 ELinearConstraintMotion ZMotion, float Limit);
	void SetLinearXLimit(ELinearConstraintMotion Motion, float Limit);
	void SetLinearYLimit(ELinearConstraintMotion Motion, float Limit);
	void SetLinearZLimit(ELinearConstraintMotion Motion, float Limit);

	// Angular Limits 설정
	void SetAngularLimits(const UPhysicsConstraintSetup* Setup);
	void SetSwing1Limit(EAngularConstraintMotion Motion, float LimitAngle);
	void SetSwing2Limit(EAngularConstraintMotion Motion, float LimitAngle);
	void SetTwistLimit(EAngularConstraintMotion Motion, float LimitAngle);

	// Soft Limit 설정
	void SetSoftSwingLimit(bool bEnable, float Stiffness, float Damping);
	void SetSoftTwistLimit(bool bEnable, float Stiffness, float Damping);

	// Break Threshold 설정
	void SetLinearBreakThreshold(bool bEnable, float Threshold);
	void SetAngularBreakThreshold(bool bEnable, float Threshold);

	// ===== Transform 설정 =====
	void SetConstraintFrames(const FVector& PosInBody1, const FVector& RotInBody1,
							 const FVector& PosInBody2, const FVector& RotInBody2);

	// ===== Getters =====
	PxD6Joint* GetJoint() const { return JointHandle; }
	FBodyInstance* GetBodyA() const { return BodyA; }
	FBodyInstance* GetBodyB() const { return BodyB; }

private:
	// PhysX Motion 변환 유틸리티
	static PxD6Motion::Enum ConvertLinearMotion(ELinearConstraintMotion Motion);
	static PxD6Motion::Enum ConvertAngularMotion(EAngularConstraintMotion Motion);

	// Euler to Quaternion 변환 (도 단위)
	static PxQuat EulerToQuaternion(const FVector& EulerDegrees);

public:
	PxD6Joint* JointHandle = nullptr;
	FBodyInstance* BodyA = nullptr;
	FBodyInstance* BodyB = nullptr;
	const UPhysicsConstraintSetup* ConstraintSetup = nullptr;
	bool bJointCreated = false;
};
