#include "pch.h"
#include "PhysicsAsset.h"
#include "Source/Runtime/Core/Misc/JsonSerializer.h"
#include <fstream>

IMPLEMENT_CLASS(UPhysicsAsset)

int32 UPhysicsAsset::FindBodyIndexByBoneName(FName BoneName) const
{
    if (const int32* Found = BoneNameToBodyIndex.Find(BoneName))
    {
        return *Found;
    }
    for (int32 i = 0; i < BodySetups.Num(); ++i)
    {
        if (BodySetups[i] && BodySetups[i]->BoneName == BoneName)
        {
            return i;
        }
    }
    return -1;
}

UBodySetup* UPhysicsAsset::FindBodySetupByBoneName(FName BoneName)
{
    const int32 idx = FindBodyIndexByBoneName(BoneName);
    return (idx >= 0 && idx < BodySetups.Num()) ? BodySetups[idx] : nullptr;
}

const UBodySetup* UPhysicsAsset::FindBodySetupByBoneName(FName BoneName) const
{
    const int32 idx = FindBodyIndexByBoneName(BoneName);
    return (idx >= 0 && idx < BodySetups.Num()) ? BodySetups[idx] : nullptr;
}

void UPhysicsAsset::RebuildNameToIndexMap()
{
    BoneNameToBodyIndex.clear();
    for (int32 i = 0; i < BodySetups.Num(); ++i)
    {
        if (!BodySetups[i]) continue;
        const FName& Name = BodySetups[i]->BoneName;
        if (Name.IsValid())
        {
            // Last write wins if duplicates exist
            BoneNameToBodyIndex[Name] = i;
        }
    }
}

int32 UPhysicsAsset::AddBodySetup(UBodySetup* NewSetup)
{
    if (!NewSetup) return -1;
    const int32 idx = BodySetups.Num();
    BodySetups.Add(NewSetup);
    if (NewSetup->BoneName.IsValid())
    {
        BoneNameToBodyIndex[NewSetup->BoneName] = idx;
    }
    return idx;
}

bool UPhysicsAsset::RemoveBodyByBoneName(FName BoneName)
{
    const int32 idx = FindBodyIndexByBoneName(BoneName);
    if (idx < 0) return false;
    // Note: In a full UE implementation, you'd also handle UObject cleanup
    BodySetups.erase(BodySetups.begin() + idx);
    RebuildNameToIndexMap();
    return true;
}

FAABB UPhysicsAsset::CalcAABB(const FTransform& ComponentTM, float Scale) const
{
    bool bHasAny = false;
    const float FMAX = std::numeric_limits<float>::max();
    FVector GlobalMin(FMAX, FMAX, FMAX);
    FVector GlobalMax(-FMAX, -FMAX, -FMAX);

    for (const UBodySetup* BS : BodySetups)
    {
        if (!BS) continue;
        const FAABB B = BS->CalcAABB(ComponentTM, Scale);
        if (!bHasAny)
        {
            GlobalMin = B.Min; GlobalMax = B.Max; bHasAny = true;
        }
        else
        {
            GlobalMin = GlobalMin.ComponentMin(B.Min);
            GlobalMax = GlobalMax.ComponentMax(B.Max);
        }
    }

    if (!bHasAny)
    {
        return FAABB(FVector(0, 0, 0), FVector(0, 0, 0));
    }
    return FAABB(GlobalMin, GlobalMax);
}

FAABB UPhysicsAsset::CalcAABB(const TMap<FName, FTransform>& BoneWorldTMs, float Scale) const
{
    bool bHasAny = false;
    const float FMAX = std::numeric_limits<float>::max();
    FVector GlobalMin(FMAX, FMAX, FMAX);
    FVector GlobalMax(-FMAX, -FMAX, -FMAX);

    for (const UBodySetup* BS : BodySetups)
    {
        if (!BS) continue;
        FTransform BoneTM; // identity default
        if (const FTransform* Found = BoneWorldTMs.Find(BS->BoneName))
        {
            BoneTM = *Found;
        }
        const FAABB B = BS->CalcAABB(BoneTM, Scale);
        if (!bHasAny)
        {
            GlobalMin = B.Min; GlobalMax = B.Max; bHasAny = true;
        }
        else
        {
            GlobalMin = GlobalMin.ComponentMin(B.Min);
            GlobalMax = GlobalMax.ComponentMax(B.Max);
        }
    }

    if (!bHasAny)
    {
        return FAABB(FVector(0, 0, 0), FVector(0, 0, 0));
    }
    return FAABB(GlobalMin, GlobalMax);
}

bool UPhysicsAsset::SaveToFile(const FString& FilePath) const
{
    JSON Root = JSON::Make(JSON::Class::Object);

    // Source skeletal mesh path
    Root["SourceSkeletalPath"] = SourceSkeletalPath;

    // Bodies array
    JSON BodiesArray = JSON::Make(JSON::Class::Array);
    for (const UBodySetup* Body : BodySetups)
    {
        if (!Body) continue;

        JSON BodyJson = JSON::Make(JSON::Class::Object);
        BodyJson["BoneName"] = Body->BoneName.ToString();

        // Spheres
        JSON SpheresArray = JSON::Make(JSON::Class::Array);
        for (const FKSphereElem& Sphere : Body->AggGeom.SphereElems)
        {
            JSON SphereJson = JSON::Make(JSON::Class::Object);
            SphereJson["Center"] = FJsonSerializer::VectorToJson(Sphere.Center);
            SphereJson["Radius"] = Sphere.Radius;
            SpheresArray.append(SphereJson);
        }
        BodyJson["Spheres"] = SpheresArray;

        // Boxes
        JSON BoxesArray = JSON::Make(JSON::Class::Array);
        for (const FKBoxElem& Box : Body->AggGeom.BoxElems)
        {
            JSON BoxJson = JSON::Make(JSON::Class::Object);
            BoxJson["Center"] = FJsonSerializer::VectorToJson(Box.Center);
            BoxJson["Rotation"] = FJsonSerializer::VectorToJson(Box.Rotation);
            BoxJson["X"] = Box.X;
            BoxJson["Y"] = Box.Y;
            BoxJson["Z"] = Box.Z;
            BoxesArray.append(BoxJson);
        }
        BodyJson["Boxes"] = BoxesArray;

        // Capsules
        JSON CapsulesArray = JSON::Make(JSON::Class::Array);
        for (const FKCapsuleElem& Capsule : Body->AggGeom.SphylElems)
        {
            JSON CapsuleJson = JSON::Make(JSON::Class::Object);
            CapsuleJson["Center"] = FJsonSerializer::VectorToJson(Capsule.Center);
            CapsuleJson["Rotation"] = FJsonSerializer::VectorToJson(Capsule.Rotation);
            CapsuleJson["Radius"] = Capsule.Radius;
            CapsuleJson["Length"] = Capsule.Length;
            CapsulesArray.append(CapsuleJson);
        }
        BodyJson["Capsules"] = CapsulesArray;

        BodiesArray.append(BodyJson);
    }
    Root["Bodies"] = BodiesArray;

    // Constraints (placeholder for future)
    JSON ConstraintsArray = JSON::Make(JSON::Class::Array);
    Root["Constraints"] = ConstraintsArray;

    // Convert to wide string for file path
    FWideString WidePath(FilePath.begin(), FilePath.end());
    return FJsonSerializer::SaveJsonToFile(Root, WidePath);
}

bool UPhysicsAsset::LoadFromFile(const FString& FilePath)
{
    FWideString WidePath(FilePath.begin(), FilePath.end());
    JSON Root;
    if (!FJsonSerializer::LoadJsonFromFile(Root, WidePath))
    {
        UE_LOG("[PhysicsAsset] Failed to load file: %s", FilePath.c_str());
        return false;
    }

    // Clear existing data
    BodySetups.clear();
    BoneNameToBodyIndex.clear();

    // Source skeletal path
    FJsonSerializer::ReadString(Root, "SourceSkeletalPath", SourceSkeletalPath, "", false);

    // Bodies
    JSON BodiesArray;
    if (FJsonSerializer::ReadArray(Root, "Bodies", BodiesArray, nullptr, false))
    {
        for (int i = 0; i < BodiesArray.size(); ++i)
        {
            const JSON& BodyJson = BodiesArray.at(i);

            UBodySetup* Body = NewObject<UBodySetup>();
            if (!Body) continue;

            // BoneName
            FString BoneNameStr;
            FJsonSerializer::ReadString(BodyJson, "BoneName", BoneNameStr, "", false);
            Body->BoneName = FName(BoneNameStr);

            // Spheres
            JSON SpheresArray;
            if (FJsonSerializer::ReadArray(BodyJson, "Spheres", SpheresArray, nullptr, false))
            {
                for (int j = 0; j < SpheresArray.size(); ++j)
                {
                    const JSON& SphereJson = SpheresArray.at(j);
                    FKSphereElem Sphere;
                    FJsonSerializer::ReadVector(SphereJson, "Center", Sphere.Center, FVector::Zero(), false);
                    FJsonSerializer::ReadFloat(SphereJson, "Radius", Sphere.Radius, 1.0f, false);
                    Body->AggGeom.SphereElems.Add(Sphere);
                }
            }

            // Boxes
            JSON BoxesArray;
            if (FJsonSerializer::ReadArray(BodyJson, "Boxes", BoxesArray, nullptr, false))
            {
                for (int j = 0; j < BoxesArray.size(); ++j)
                {
                    const JSON& BoxJson = BoxesArray.at(j);
                    FKBoxElem Box;
                    FJsonSerializer::ReadVector(BoxJson, "Center", Box.Center, FVector::Zero(), false);
                    FJsonSerializer::ReadVector(BoxJson, "Rotation", Box.Rotation, FVector::Zero(), false);
                    FJsonSerializer::ReadFloat(BoxJson, "X", Box.X, 1.0f, false);
                    FJsonSerializer::ReadFloat(BoxJson, "Y", Box.Y, 1.0f, false);
                    FJsonSerializer::ReadFloat(BoxJson, "Z", Box.Z, 1.0f, false);
                    Body->AggGeom.BoxElems.Add(Box);
                }
            }

            // Capsules
            JSON CapsulesArray;
            if (FJsonSerializer::ReadArray(BodyJson, "Capsules", CapsulesArray, nullptr, false))
            {
                for (int j = 0; j < CapsulesArray.size(); ++j)
                {
                    const JSON& CapsuleJson = CapsulesArray.at(j);
                    FKCapsuleElem Capsule;
                    FJsonSerializer::ReadVector(CapsuleJson, "Center", Capsule.Center, FVector::Zero(), false);
                    FJsonSerializer::ReadVector(CapsuleJson, "Rotation", Capsule.Rotation, FVector::Zero(), false);
                    FJsonSerializer::ReadFloat(CapsuleJson, "Radius", Capsule.Radius, 1.0f, false);
                    FJsonSerializer::ReadFloat(CapsuleJson, "Length", Capsule.Length, 1.0f, false);
                    Body->AggGeom.SphylElems.Add(Capsule);
                }
            }

            AddBodySetup(Body);
        }
    }

    UE_LOG("[PhysicsAsset] Loaded %d bodies from: %s", BodySetups.Num(), FilePath.c_str());
    return true;
}
