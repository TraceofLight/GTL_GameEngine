#include "pch.h"
#include "PhysicsAsset.h"

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
