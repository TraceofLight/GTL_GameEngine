#include "pch.h"
#include "BoxElem.h"
#include "Source/Runtime/Engine/Components/ShapeComponent.h"

FBoxShape FKBoxElem::CalcAABB(const FTransform& /*BoneTM*/, float Scale) const
{
    FBoxShape Out;
    Out.BoxExtent = FVector(X * Scale, Y * Scale, Z * Scale);
    return Out;
}

