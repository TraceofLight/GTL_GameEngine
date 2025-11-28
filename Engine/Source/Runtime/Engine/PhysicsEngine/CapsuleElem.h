#pragma once
#include "ShapeComponent.h"
#include "ShapeElem.h"

struct FKCapsuleElem : public FKShapeElem
{
public:
	/** Position of the capsule's origin */
	UPROPERTY(Category=Capsule, EditAnywhere)
	FVector Center;

	/** Rotation of the capsule */
	UPROPERTY(Category = Capsule, EditAnywhere, meta = (ClampMin = "-360", ClampMax = "360"))
	FVector Rotation;

	/** Radius of the capsule */
	UPROPERTY(Category= Capsule, EditAnywhere)
	float Radius;

	/** This is of line-segment ie. add Radius to both ends to find total length. */
	UPROPERTY(Category= Capsule, EditAnywhere)
	float Length;

	FKCapsuleElem()
	: FKShapeElem(EAggCollisionShape::Capsule)
	, Center( FVector(0, 0, 0) )
	, Rotation( FVector(0, 0, 0) )
	, Radius(1), Length(1)
	{

	}

	FKCapsuleElem( float InRadius, float InLength )
	: FKShapeElem(EAggCollisionShape::Capsule)
	, Center( FVector(0, 0, 0) )
	, Rotation( FVector(0, 0, 0) )
	, Radius(InRadius), Length(InLength)
	{

	}

	friend bool operator==( const FKCapsuleElem& LHS, const FKCapsuleElem& RHS )
	{
		return ( LHS.Center == RHS.Center &&
			LHS.Rotation == RHS.Rotation &&
			LHS.Radius == RHS.Radius &&
			LHS.Length == RHS.Length );
	};

	// FBoxShape CalcAABB(const FTransform& BoneTM, float Scale) const;
};
