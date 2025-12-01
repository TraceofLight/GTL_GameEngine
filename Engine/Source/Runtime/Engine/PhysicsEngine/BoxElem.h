#pragma once
#include "ShapeElem.h"

struct FBoxShape;

class FKBoxElem : public FKShapeElem
{
public:
	/** Position of the box's origin */
	UPROPERTY(Category=Box, EditAnywhere)
	FVector Center;

	/** Rotation of the box */
	UPROPERTY(Category=Box, EditAnywhere, meta = (ClampMin = "-360", ClampMax = "360"))
	FVector Rotation;

	/** Extent of the box along the y-axis */
	UPROPERTY(Category= Box, EditAnywhere, meta =(DisplayName = "X Extent"))
	float X;

	/** Extent of the box along the y-axis */
	UPROPERTY(Category= Box, EditAnywhere, meta = (DisplayName = "Y Extent"))
	float Y;

	/** Extent of the box along the z-axis */
	UPROPERTY(Category= Box, EditAnywhere, meta = (DisplayName = "Z Extent"))
	float Z;

	FKBoxElem()
	: FKShapeElem(EAggCollisionShape::Box)
	, Center( FVector(0, 0, 0) )
	, Rotation( FVector(0, 0, 0) )
	, X(1), Y(1), Z(1)
	{

	}

	FKBoxElem( float s )
	: FKShapeElem(EAggCollisionShape::Box)
	, Center( FVector(0, 0, 0) )
	, Rotation( FVector(0, 0, 0) )
	, X(s), Y(s), Z(s)
	{

	}

	FKBoxElem( float InX, float InY, float InZ )
	: FKShapeElem(EAggCollisionShape::Box)
	, Center( FVector(0, 0, 0) )
	, Rotation( FVector(0, 0, 0) )
	, X(InX), Y(InY), Z(InZ)
	{

	}

	friend bool operator==( const FKBoxElem& LHS, const FKBoxElem& RHS )
	{
		return ( LHS.Center == RHS.Center &&
			LHS.Rotation == RHS.Rotation &&
			LHS.X == RHS.X &&
			LHS.Y == RHS.Y &&
			LHS.Z == RHS.Z );
	};

	FBoxShape CalcAABB(const FTransform& BoneTM, float Scale) const;
};
