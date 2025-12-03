#include "pch.h"
#include "AggregateGeom.h"
#include "Source/Runtime/Core/Misc/JsonSerializer.h"

static inline FVector AbsVec(const FVector& V)
{
    return FVector(std::fabs(V.X), std::fabs(V.Y), std::fabs(V.Z));
}

int32 FKAggregateGeom::GetElementCount() const
{
    return static_cast<int32>(SphereElems.Num() + BoxElems.Num() + SphylElems.Num());
}

int32 FKAggregateGeom::GetElementCount(EAggCollisionShape::Type InType) const
{
    switch (InType)
    {
    case EAggCollisionShape::Sphere:
    	return static_cast<int32>(SphereElems.Num());
    case EAggCollisionShape::Box:
    	return static_cast<int32>(BoxElems.Num());
    case EAggCollisionShape::Capsule:
    	return static_cast<int32>(SphylElems.Num());
    default:
    	return 0;
    }
}

FKShapeElem* FKAggregateGeom::GetElement(EAggCollisionShape::Type InType, int32 Index)
{
    switch (InType)
    {
    case EAggCollisionShape::Sphere:
        if (0 <= Index && Index < SphereElems.Num())
        	return &SphereElems[Index];
        break;
    case EAggCollisionShape::Box:
        if (0 <= Index && Index < BoxElems.Num())
        	return &BoxElems[Index];
        break;
    case EAggCollisionShape::Capsule:
        if (0 <= Index && Index < SphylElems.Num())
        	return &SphylElems[Index];
        break;
    default:
        break;
    }
    return nullptr;
}

const FKShapeElem* FKAggregateGeom::GetElement(EAggCollisionShape::Type InType, int32 Index) const
{
    switch (InType)
    {
    case EAggCollisionShape::Sphere:
        if (0 <= Index && Index < SphereElems.Num())
        	return &SphereElems[Index];
        break;
    case EAggCollisionShape::Box:
        if (0 <= Index && Index < BoxElems.Num())
        	return &BoxElems[Index];
        break;
    case EAggCollisionShape::Capsule:
        if (0 <= Index && Index < SphylElems.Num())
        	return &SphylElems[Index];
        break;
    default:
        break;
    }
    return nullptr;
}

FKShapeElem* FKAggregateGeom::GetElementByName(const FName& InName)
{
    for (auto& E : SphereElems)
    {
	    if (E.GetName() == InName) return &E;
    }
    for (auto& E : BoxElems)
    {
	    if (E.GetName() == InName) return &E;
    }
    for (auto& E : SphylElems)
    {
	    if (E.GetName() == InName) return &E;
    }
    return nullptr;
}

const FKShapeElem* FKAggregateGeom::GetElementByName(const FName& InName) const
{
    for (auto& E : SphereElems)
    {
	    if (E.GetName() == InName) return &E;
    }
    for (auto& E : BoxElems)
    {
	    if (E.GetName() == InName) return &E;
    }
    for (auto& E : SphylElems)
    {
	    if (E.GetName() == InName) return &E;
    }
    return nullptr;
}

FAABB FKAggregateGeom::CalcAABB(const FTransform& BoneTM, float Scale) const
{
    bool bHasAny = false;
    const float FMAX = std::numeric_limits<float>::max();
    FVector GlobalMin(FMAX, FMAX, FMAX);
    FVector GlobalMax(-FMAX, -FMAX, -FMAX);

    auto ExpandBy = [&](const FVector& CenterWS, const FVector& HalfExtentWS)
    {
        const FVector LocalMin = CenterWS - HalfExtentWS;
        const FVector LocalMax = CenterWS + HalfExtentWS;
        GlobalMin = GlobalMin.ComponentMin(LocalMin);
        GlobalMax = GlobalMax.ComponentMax(LocalMax);
        bHasAny = true;
    };

    const FQuat WorldRot = BoneTM.Rotation;
    const FVector WorldScale = BoneTM.Scale3D;
    const FVector WorldTrans = BoneTM.Translation;

    for (const FKSphereElem& S : SphereElems)
    {
        const FVector ScaledCenter(S.Center.X * WorldScale.X, S.Center.Y * WorldScale.Y, S.Center.Z * WorldScale.Z);
        const FVector CenterWS = WorldTrans + WorldRot.RotateVector(ScaledCenter);

        const float r = S.Radius * Scale;
        const FVector Half(r, r, r);
        ExpandBy(CenterWS, Half);
    }

    for (const FKBoxElem& B : BoxElems)
    {
        const FVector ScaledCenter(B.Center.X * WorldScale.X, B.Center.Y * WorldScale.Y, B.Center.Z * WorldScale.Z);
        const FVector CenterWS = WorldTrans + WorldRot.RotateVector(ScaledCenter);

        const FVector HalfLocal(B.X * Scale, B.Y * Scale, B.Z * Scale);

        const FQuat LocalRot = FQuat::MakeFromEulerZYX(B.Rotation);
        FQuat OBBRot = WorldRot * LocalRot;
        OBBRot.Normalize();

        const FVector ex = OBBRot.RotateVector(FVector(HalfLocal.X, 0, 0));
        const FVector ey = OBBRot.RotateVector(FVector(0, HalfLocal.Y, 0));
        const FVector ez = OBBRot.RotateVector(FVector(0, 0, HalfLocal.Z));
        const FVector HalfWS = AbsVec(ex) + AbsVec(ey) + AbsVec(ez);

        ExpandBy(CenterWS, HalfWS);
    }

    if (!bHasAny)
    {
        return FAABB(FVector(0, 0, 0), FVector(0, 0, 0));
    }
    return FAABB(GlobalMin, GlobalMax);
}

void FKAggregateGeom::Serialize(bool bIsLoading, JSON& Json)
{
    if (bIsLoading)
    {
        // Clear existing data
        SphereElems.Empty();
        BoxElems.Empty();
        SphylElems.Empty();

        // Load SphereElems
        if (Json.hasKey("SphereElems") && Json["SphereElems"].JSONType() == JSON::Class::Array)
        {
            for (JSON& ElemJson : Json["SphereElems"].ArrayRange())
            {
                FKSphereElem NewElem;
                NewElem.Serialize(true, ElemJson);
                SphereElems.Add(NewElem);
            }
        }

        // Load BoxElems
        if (Json.hasKey("BoxElems") && Json["BoxElems"].JSONType() == JSON::Class::Array)
        {
            for (JSON& ElemJson : Json["BoxElems"].ArrayRange())
            {
                FKBoxElem NewElem;
                NewElem.Serialize(true, ElemJson);
                BoxElems.Add(NewElem);
            }
        }

        // Load CapsuleElems (SphylElems)
        if (Json.hasKey("CapsuleElems") && Json["CapsuleElems"].JSONType() == JSON::Class::Array)
        {
            for (JSON& ElemJson : Json["CapsuleElems"].ArrayRange())
            {
                FKCapsuleElem NewElem;
                NewElem.Serialize(true, ElemJson);
                SphylElems.Add(NewElem);
            }
        }
    }
    else
    {
        // Save SphereElems
        Json["SphereElems"] = JSON::Make(JSON::Class::Array);
        for (FKSphereElem& Elem : SphereElems)
        {
            JSON ElemJson;
            Elem.Serialize(false, ElemJson);
            Json["SphereElems"].append(ElemJson);
        }

        // Save BoxElems
        Json["BoxElems"] = JSON::Make(JSON::Class::Array);
        for (FKBoxElem& Elem : BoxElems)
        {
            JSON ElemJson;
            Elem.Serialize(false, ElemJson);
            Json["BoxElems"].append(ElemJson);
        }

        // Save CapsuleElems (SphylElems)
        Json["CapsuleElems"] = JSON::Make(JSON::Class::Array);
        for (FKCapsuleElem& Elem : SphylElems)
        {
            JSON ElemJson;
            Elem.Serialize(false, ElemJson);
            Json["CapsuleElems"].append(ElemJson);
        }
    }
}
