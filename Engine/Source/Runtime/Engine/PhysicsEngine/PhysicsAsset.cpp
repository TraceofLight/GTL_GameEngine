#include "pch.h"
#include "PhysicsAsset.h"
#include "CapsuleElem.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"

IMPLEMENT_CLASS(UPhysicsAsset)

void UPhysicsAsset::BuildFromSkeletalMesh(const USkeletalMesh* InSkeletalMesh)
{
    if (!InSkeletalMesh || !InSkeletalMesh->GetSkeleton())
    {
        UE_LOG("[PhysicsAsset] BuildFromSkeletalMesh: Invalid skeletal mesh or skeleton");
        return;
    }

    const FSkeleton* Skeleton = InSkeletalMesh->GetSkeleton();
    const TArray<FBone>& Bones = Skeleton->Bones;
    const int32 NumBones = Bones.Num();

    // Clear existing data
    BodySetups.Empty();
    BoneNameToBodyIndex.clear();
    SourceSkeletalPath = InSkeletalMesh->GetPathFileName();

    // Build children list for each bone (to compute bone lengths)
    TArray<TArray<int32>> BoneChildren;
    BoneChildren.SetNum(NumBones);
    for (int32 i = 0; i < NumBones; ++i)
    {
        const int32 ParentIdx = Bones[i].ParentIndex;
        if (ParentIdx >= 0 && ParentIdx < NumBones)
        {
            BoneChildren[ParentIdx].Add(i);
        }
    }

    // Compute component-space bone positions from bind pose
    TArray<FVector> BonePositions;
    BonePositions.SetNum(NumBones);
    for (int32 i = 0; i < NumBones; ++i)
    {
        // Extract translation from bind pose matrix
        const FMatrix& BindPose = Bones[i].BindPose;
        BonePositions[i] = FVector(BindPose.M[3][0], BindPose.M[3][1], BindPose.M[3][2]);
    }

    // Create capsule for each bone (except leaves with no children)
    for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
    {
        const FBone& Bone = Bones[BoneIdx];
        const TArray<int32>& Children = BoneChildren[BoneIdx];

        // Skip bones with no children (leaf bones) - they have no length
        if (Children.IsEmpty())
        {
            continue;
        }

        // Compute average child position for bone direction/length
        FVector AvgChildPos(0, 0, 0);
        for (int32 ChildIdx : Children)
        {
            AvgChildPos = AvgChildPos + BonePositions[ChildIdx];
        }
        AvgChildPos = AvgChildPos * (1.0f / Children.Num());

        // Bone vector from this bone to average child
        const FVector& BonePos = BonePositions[BoneIdx];
        FVector BoneVec = AvgChildPos - BonePos;
        float BoneLength = BoneVec.Size();

        // Skip very short bones
        if (BoneLength < 0.01f)
        {
            continue;
        }

        // Heuristic radius: ~15% of bone length, with reasonable min/max
        // For humanoid characters, bones can be quite long, so allow larger radii
        float Radius = FMath::Clamp(BoneLength * 0.15f, 0.02f, 0.5f);

        // Capsule center is at midpoint of bone segment (in bone's local space)
        // For simplicity, we place center at half the bone length along the bone direction
        FVector LocalCenter = BoneVec * 0.5f;

        // Compute rotation to align capsule X-axis with bone direction
        // PhysX capsules are X-axis aligned by default
        FVector BoneDir = BoneVec.GetSafeNormal();

        // Compute Euler angles to rotate from X-axis to bone direction
        // Using simple atan2 for yaw/pitch
        FVector EulerRot(0, 0, 0);
        if (BoneDir.SizeSquared() > 0.001f)
        {
            // Pitch: rotation around Y to align with XZ plane
            float Pitch = std::atan2(-BoneDir.Z, std::sqrt(BoneDir.X * BoneDir.X + BoneDir.Y * BoneDir.Y));
            // Yaw: rotation around Z to align with XY plane
            float Yaw = std::atan2(BoneDir.Y, BoneDir.X);

            EulerRot = FVector(0, RadiansToDegrees(Pitch), RadiansToDegrees(Yaw));
        }

        // Create BodySetup with capsule
        UBodySetup NewSetup;
        NewSetup.BoneName = FName(Bone.Name);
        NewSetup.CollisionTraceFlag = ECollisionTraceFlag::UseDefault;

        FKCapsuleElem Capsule;
        Capsule.Center = LocalCenter;
        Capsule.Rotation = EulerRot;
        Capsule.Radius = Radius;
        Capsule.Length = BoneLength - Radius * 2.0f; // Length is the cylindrical part
        if (Capsule.Length < 0.01f)
        {
            Capsule.Length = 0.01f;
        }

        NewSetup.AggGeom.SphylElems.Add(Capsule);

        UE_LOG("[PhysicsAsset] Bone '%s': Capsule R=%.3f, L=%.3f, Center=(%.2f,%.2f,%.2f)",
               Bone.Name.c_str(), Radius, Capsule.Length,
               LocalCenter.X, LocalCenter.Y, LocalCenter.Z);

        // Add to physics asset
        const int32 BodyIndex = BodySetups.Num();
        BodySetups.Add(NewSetup);
        BoneNameToBodyIndex[NewSetup.BoneName] = BodyIndex;
    }

    UE_LOG("[PhysicsAsset] Built %d body setups from skeleton with %d bones",
           BodySetups.Num(), NumBones);
}

int32 UPhysicsAsset::FindBodyIndexByBoneName(FName BoneName) const
{
    if (const int32* Found = BoneNameToBodyIndex.Find(BoneName))
    {
        return *Found;
    }
    for (int32 i = 0; i < BodySetups.Num(); ++i)
    {
        if (BodySetups[i].BoneName == BoneName)
        {
            return i;
        }
    }
    return -1;
}

UBodySetup* UPhysicsAsset::FindBodySetupByBoneName(FName BoneName)
{
    const int32 idx = FindBodyIndexByBoneName(BoneName);
    return (idx >= 0 && idx < BodySetups.Num()) ? &BodySetups[idx] : nullptr;
}

const UBodySetup* UPhysicsAsset::FindBodySetupByBoneName(FName BoneName) const
{
    const int32 idx = FindBodyIndexByBoneName(BoneName);
    return (idx >= 0 && idx < BodySetups.Num()) ? &BodySetups[idx] : nullptr;
}

void UPhysicsAsset::RebuildNameToIndexMap()
{
    BoneNameToBodyIndex.clear();
    for (int32 i = 0; i < BodySetups.Num(); ++i)
    {
        const FName& Name = BodySetups[i].BoneName;
        if (Name.IsValid())
        {
            // Last write wins if duplicates exist
            BoneNameToBodyIndex[Name] = i;
        }
    }
}

int32 UPhysicsAsset::AddBodySetup(const UBodySetup& NewSetup)
{
    const int32 idx = BodySetups.Num();
    BodySetups.Add(NewSetup);
    if (NewSetup.BoneName.IsValid())
    {
        BoneNameToBodyIndex[NewSetup.BoneName] = idx;
    }
    return idx;
}

bool UPhysicsAsset::RemoveBodyByBoneName(FName BoneName)
{
    const int32 idx = FindBodyIndexByBoneName(BoneName);
    if (idx < 0) return false;
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

    for (const UBodySetup& BS : BodySetups)
    {
        const FAABB B = BS.CalcAABB(ComponentTM, Scale);
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

    for (const UBodySetup& BS : BodySetups)
    {
        FTransform BoneTM; // identity default
        if (const FTransform* Found = BoneWorldTMs.Find(BS.BoneName))
        {
            BoneTM = *Found;
        }
        const FAABB B = BS.CalcAABB(BoneTM, Scale);
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
