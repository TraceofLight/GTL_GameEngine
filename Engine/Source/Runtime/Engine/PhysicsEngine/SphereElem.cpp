#include "pch.h"
#include "SphereElem.h"
#include "Source/Runtime/Engine/Components/ShapeComponent.h"
#include "Source/Runtime/Core/Misc/JsonSerializer.h"

FBoxShape FKSphereElem::CalcAABB(const FTransform& /*BoneTM*/, float Scale) const
{
    FBoxShape Out;
    const float r = Radius * Scale;
    Out.BoxExtent = FVector(r, r, r);
    return Out;
}

void FKSphereElem::Serialize(bool bIsLoading, JSON& Json)
{
    // Serialize base class data
    FKShapeElem::Serialize(bIsLoading, Json);

    if (bIsLoading)
    {
        FJsonSerializer::ReadVector(Json, "Center", Center, FVector(0, 0, 0));
        FJsonSerializer::ReadFloat(Json, "Radius", Radius, 1.0f);
    }
    else
    {
        Json["Center"] = FJsonSerializer::VectorToJson(Center);
        Json["Radius"] = Radius;
    }
}
