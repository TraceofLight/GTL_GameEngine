#pragma once
#include "Vector.h"
#include "Color.h"
/**
 * Base particle structure containing fundamental particle data
 * This represents a single particle in the system
 */
struct FBaseParticle
{
	/** Current location of the particle in world space */
	FVector Location;

	/** Previous frame location for interpolation */
	FVector OldLocation;

	/** Current velocity of the particle */
	FVector Velocity;

	/** Relative time (0.0 to 1.0) within the particle's lifetime */
	float RelativeTime;

	/** Total lifetime of the particle in seconds */
	float Lifetime;

	/** Base velocity set at spawn time */
	FVector BaseVelocity;

	/** Current rotation of the particle in radians */
	float Rotation;

	/** Rate of rotation change per second */
	float RotationRate;

	/** Current size of the particle */
	FVector Size;

	/** Current color of the particle */
	FLinearColor Color;

	int32 Flags;					// Flags indicating various particle states

	FBaseParticle()
		: Location(FVector::Zero())
		, OldLocation(FVector::Zero())
		, Velocity(FVector::Zero())
		, RelativeTime(0.0f)
		, Lifetime(0.0f)
		, BaseVelocity(FVector::Zero())
		, Rotation(0.0f)
		, RotationRate(0.0f)
		, Size(FVector::One())
		, Color(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
	{
	}
};

/**
 * 메시 파티클용 3D 회전 Payload 데이터
 * FBaseParticle 뒤에 붙는 추가 데이터 (RequiredBytes로 공간 확보)
 */
struct FMeshRotationPayloadData
{
	FVector Rotation;      // 현재 3D 회전 (라디안: Pitch, Yaw, Roll)
	FVector RotationRate;  // 3D 회전 속도 (라디안/초)

	FMeshRotationPayloadData()
		: Rotation(FVector::Zero())
		, RotationRate(FVector::Zero())
	{}
};

/**
 * 빔 파티클의 각 포인트 데이터
 * Source에서 Target까지 빔을 구성하는 점들
 */
struct FBeamPoint
{
	FVector Position;       // 월드 위치
	FVector Tangent;        // 빔 방향 벡터 (정규화됨)
	float Width;            // 해당 지점의 빔 폭
	float Parameter;        // 0.0 = Source, 1.0 = Target
	FLinearColor Color;     // 해당 지점의 색상

	FBeamPoint()
		: Position(FVector::Zero())
		, Tangent(FVector(1.0f, 0.0f, 0.0f))
		, Width(10.0f)
		, Parameter(0.0f)
		, Color(FLinearColor(1.0f, 1.0f, 1.0f, 1.0f))
	{}
};
