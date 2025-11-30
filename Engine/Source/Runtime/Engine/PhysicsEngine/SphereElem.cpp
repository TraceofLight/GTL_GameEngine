#include "pch.h"
#include "SphereElem.h"
#include "Source/Runtime/Engine/Components/ShapeComponent.h"

FBoxShape FKSphereElem::CalcAABB(const FTransform& /*BoneTM*/, float Scale) const
{
    FBoxShape Out;
    const float r = Radius * Scale;
    Out.BoxExtent = FVector(r, r, r);
    return Out;
}

