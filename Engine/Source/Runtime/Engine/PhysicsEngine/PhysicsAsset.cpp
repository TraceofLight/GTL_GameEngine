#include "pch.h"
#include "PhysicsAsset.h"
#include "PhysicsConstraintSetup.h"
#include "Source/Runtime/Core/Misc/JsonSerializer.h"

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

// ===== Constraint 관리 =====

int32 UPhysicsAsset::FindConstraintIndexByBoneNames(FName Bone1, FName Bone2) const
{
	for (int32 i = 0; i < ConstraintSetups.Num(); ++i)
	{
		if (ConstraintSetups[i] &&
			ConstraintSetups[i]->ConstraintBone1 == Bone1 &&
			ConstraintSetups[i]->ConstraintBone2 == Bone2)
		{
			return i;
		}
	}
	return -1;
}

UPhysicsConstraintSetup* UPhysicsAsset::FindConstraintByBoneNames(FName Bone1, FName Bone2)
{
	const int32 Idx = FindConstraintIndexByBoneNames(Bone1, Bone2);
	return (Idx != -1) ? ConstraintSetups[Idx] : nullptr;
}

int32 UPhysicsAsset::AddConstraintSetup(UPhysicsConstraintSetup* NewSetup)
{
	if (!NewSetup)
	{
		return -1;
	}

	const int32 Idx = ConstraintSetups.Num();
	ConstraintSetups.Add(NewSetup);
	return Idx;
}

// ===== Serialization =====

void UPhysicsAsset::Serialize(bool bIsLoading, JSON& Json)
{
	if (bIsLoading)
	{
		// Source Skeletal Path
		FJsonSerializer::ReadString(Json, "SourceSkeletalPath", SourceSkeletalPath, "");

		// BodySetups
		BodySetups.Empty();
		BoneNameToBodyIndex.clear();

		if (Json.hasKey("BodySetups") && Json["BodySetups"].JSONType() == JSON::Class::Array)
		{
			for (JSON& BodyJson : Json["BodySetups"].ArrayRange())
			{
				UBodySetup* NewSetup = NewObject<UBodySetup>();
				NewSetup->Serialize(true, BodyJson);
				AddBodySetup(NewSetup);
			}
		}

		// ConstraintSetups
		for (UPhysicsConstraintSetup* Constraint : ConstraintSetups)
		{
			if (Constraint)
			{
				ObjectFactory::DeleteObject(Constraint);
			}
		}
		ConstraintSetups.Empty();

		if (Json.hasKey("ConstraintSetups") && Json["ConstraintSetups"].JSONType() == JSON::Class::Array)
		{
			for (JSON& ConstraintJson : Json["ConstraintSetups"].ArrayRange())
			{
				UPhysicsConstraintSetup* NewConstraint = NewObject<UPhysicsConstraintSetup>();
				NewConstraint->Serialize(true, ConstraintJson);

				// BodyIndex 재설정 (BoneName 기반)
				NewConstraint->BodyIndex1 = FindBodyIndexByBoneName(NewConstraint->ConstraintBone1);
				NewConstraint->BodyIndex2 = FindBodyIndexByBoneName(NewConstraint->ConstraintBone2);

				AddConstraintSetup(NewConstraint);
			}
		}

		UE_LOG("[PhysicsAsset] Loaded: %d bodies, %d constraints from '%s'",
			   BodySetups.Num(), ConstraintSetups.Num(), SourceSkeletalPath.c_str());
	}
	else
	{
		// Source Skeletal Path
		Json["SourceSkeletalPath"] = SourceSkeletalPath;

		// BodySetups
		Json["BodySetups"] = JSON::Make(JSON::Class::Array);
		for (UBodySetup* Setup : BodySetups)
		{
			if (Setup)
			{
				JSON BodyJson;
				Setup->Serialize(false, BodyJson);
				Json["BodySetups"].append(BodyJson);
			}
		}

		// ConstraintSetups
		Json["ConstraintSetups"] = JSON::Make(JSON::Class::Array);
		for (UPhysicsConstraintSetup* Constraint : ConstraintSetups)
		{
			if (Constraint)
			{
				JSON ConstraintJson;
				Constraint->Serialize(false, ConstraintJson);
				Json["ConstraintSetups"].append(ConstraintJson);
			}
		}

		UE_LOG("[PhysicsAsset] Saved: %d bodies, %d constraints",
			   BodySetups.Num(), ConstraintSetups.Num());
	}
}
