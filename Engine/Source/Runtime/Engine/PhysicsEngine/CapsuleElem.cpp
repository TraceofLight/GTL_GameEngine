#include "pch.h"
#include "CapsuleElem.h"
#include "Source/Runtime/Core/Misc/JsonSerializer.h"

void FKCapsuleElem::Serialize(bool bIsLoading, JSON& Json)
{
    // Serialize base class data
    FKShapeElem::Serialize(bIsLoading, Json);

    if (bIsLoading)
    {
        FJsonSerializer::ReadVector(Json, "Center", Center, FVector(0, 0, 0));
        FJsonSerializer::ReadVector(Json, "Rotation", Rotation, FVector(0, 0, 0));
        FJsonSerializer::ReadFloat(Json, "Radius", Radius, 1.0f);
        FJsonSerializer::ReadFloat(Json, "Length", Length, 1.0f);
    }
    else
    {
        Json["Center"] = FJsonSerializer::VectorToJson(Center);
        Json["Rotation"] = FJsonSerializer::VectorToJson(Rotation);
        Json["Radius"] = Radius;
        Json["Length"] = Length;
    }
}
