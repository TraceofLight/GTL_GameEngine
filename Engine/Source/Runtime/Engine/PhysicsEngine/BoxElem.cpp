#include "pch.h"
#include "BoxElem.h"
#include "Source/Runtime/Engine/Components/ShapeComponent.h"
#include "Source/Runtime/Core/Misc/JsonSerializer.h"

FBoxShape FKBoxElem::CalcAABB(const FTransform& /*BoneTM*/, float Scale) const
{
    FBoxShape Out;
    Out.BoxExtent = FVector(X * Scale, Y * Scale, Z * Scale);
    return Out;
}

void FKBoxElem::Serialize(bool bIsLoading, JSON& Json)
{
    // Serialize base class data
    FKShapeElem::Serialize(bIsLoading, Json);

    if (bIsLoading)
    {
        FJsonSerializer::ReadVector(Json, "Center", Center, FVector(0, 0, 0));
        FJsonSerializer::ReadVector(Json, "Rotation", Rotation, FVector(0, 0, 0));
        FJsonSerializer::ReadFloat(Json, "X", X, 1.0f);
        FJsonSerializer::ReadFloat(Json, "Y", Y, 1.0f);
        FJsonSerializer::ReadFloat(Json, "Z", Z, 1.0f);
    }
    else
    {
        Json["Center"] = FJsonSerializer::VectorToJson(Center);
        Json["Rotation"] = FJsonSerializer::VectorToJson(Rotation);
        Json["X"] = X;
        Json["Y"] = Y;
        Json["Z"] = Z;
    }
}
