#pragma once
#include "ShapeElem.h"

struct FBoxShape;

struct FKSphereElem : public FKShapeElem
{
public:
	/** Position of the sphere's origin */
	UPROPERTY(Category=Sphere, EditAnywhere)
	FVector Center;

	/** Radius of the sphere */
	UPROPERTY(Category=Sphere, EditAnywhere)
	float Radius;

	FKSphereElem()
	: FKShapeElem(EAggCollisionShape::Sphere)
	, Center( FVector(0, 0, 0) )
	, Radius(1)
	{

	}

	FKSphereElem(float r)
	: FKShapeElem(EAggCollisionShape::Sphere)
	, Center( FVector(0, 0, 0) )
	, Radius(r)
	{

	}

	friend bool operator==( const FKSphereElem& LHS, const FKSphereElem& RHS )
	{
		return ( LHS.Center == RHS.Center &&
			LHS.Radius == RHS.Radius );
	}

	FBoxShape CalcAABB(const FTransform& BoneTM, float Scale) const;
};
