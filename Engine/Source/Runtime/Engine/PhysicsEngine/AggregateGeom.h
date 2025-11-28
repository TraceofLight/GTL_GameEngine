#pragma once
#include "SphereElem.h"
#include "BoxElem.h"
#include "Name.h"
#include "Vector.h"
#include "Source/Runtime/Engine/Collision/AABB.h"

class FKAggregateGeom
{
public:
	TArray<FKSphereElem> SphereElems;
	TArray<FKBoxElem> BoxElems;

    int32 GetElementCount() const;
    int32 GetElementCount(EAggCollisionShape::Type InType) const;

    FKShapeElem* GetElement(EAggCollisionShape::Type InType, int32 Index);
    const FKShapeElem* GetElement(EAggCollisionShape::Type InType, int32 Index) const;

    FKShapeElem* GetElementByName(const FName& InName);
    const FKShapeElem* GetElementByName(const FName& InName) const;

    FAABB CalcAABB(const FTransform& BoneTM, float Scale) const;
};
