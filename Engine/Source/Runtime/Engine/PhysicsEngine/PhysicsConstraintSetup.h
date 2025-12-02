#pragma once
#include "Source/Runtime/Core/Object/Object.h"
#include "Source/Runtime/Core/Math/Vector.h"
#include "Source/Runtime/Core/Misc/Name.h"

// Linear Constraint Motion Type
enum class ELinearConstraintMotion : uint8
{
	Free,		// 자유 이동
	Limited,	// 제한된 이동
	Locked		// 고정
};

// Angular Constraint Motion Type
enum class EAngularConstraintMotion : uint8
{
	Free,		// 자유 회전
	Limited,	// 제한된 회전
	Locked		// 고정
};

/**
 * @brief Physics Constraint Setup
 * @details 두 물리 바디 사이의 제약 조건 설정 데이터
 *
 * @param ConstraintBone1 첫 번째 (Parent) 본 이름
 * @param ConstraintBone2 두 번째 (Child) 본 이름
 * @param BodyIndex1 첫 번째 본의 Body 인덱스
 * @param BodyIndex2 두 번째 본의 Body 인덱스
 */
class UPhysicsConstraintSetup : public UObject
{
public:
	DECLARE_CLASS(UPhysicsConstraintSetup, UObject)

	// ===== Bone References =====
	FName ConstraintBone1;		// Parent bone
	FName ConstraintBone2;		// Child bone
	int32 BodyIndex1 = -1;		// Parent body index in PhysicsAsset
	int32 BodyIndex2 = -1;		// Child body index in PhysicsAsset

	// ===== Constraint Frame (Local Space) =====
	FVector ConstraintPositionInBody1 = FVector(0, 0, 0);
	FVector ConstraintPositionInBody2 = FVector(0, 0, 0);
	FVector ConstraintRotationInBody1 = FVector(0, 0, 0);	// Euler (degrees)
	FVector ConstraintRotationInBody2 = FVector(0, 0, 0);	// Euler (degrees)

	// ===== Linear Limits =====
	ELinearConstraintMotion LinearXMotion = ELinearConstraintMotion::Locked;
	ELinearConstraintMotion LinearYMotion = ELinearConstraintMotion::Locked;
	ELinearConstraintMotion LinearZMotion = ELinearConstraintMotion::Locked;
	float LinearLimit = 0.0f;	// 미터 단위

	// ===== Angular Limits =====
	EAngularConstraintMotion Swing1Motion = EAngularConstraintMotion::Limited;	// Y축 회전
	EAngularConstraintMotion Swing2Motion = EAngularConstraintMotion::Limited;	// Z축 회전
	EAngularConstraintMotion TwistMotion = EAngularConstraintMotion::Limited;	// X축 회전

	float Swing1LimitAngle = 45.0f;		// degrees
	float Swing2LimitAngle = 45.0f;		// degrees
	float TwistLimitAngle = 45.0f;		// degrees

	// ===== Soft Limits =====
	bool bSoftSwingLimit = false;
	bool bSoftTwistLimit = false;
	float SwingStiffness = 0.0f;
	float SwingDamping = 0.0f;
	float TwistStiffness = 0.0f;
	float TwistDamping = 0.0f;

	// ===== Break Settings =====
	bool bLinearBreakable = false;
	bool bAngularBreakable = false;
	float LinearBreakThreshold = 0.0f;
	float AngularBreakThreshold = 0.0f;

	// ===== Serialization =====
	void Serialize(bool bIsLoading, JSON& Json);
};
