#include "pch.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsAsset.h"
#include "BodySetup.h"
#include "CapsuleElem.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"

namespace
{
    /**
     * Calculate the "length" of a bone based on the distance to its first child.
     * Returns 0 if the bone has no children.
     */
    float CalcBoneLength(int32 BoneIndex, const TArray<FVector>& BonePositions, const TArray<TArray<int32>>& BoneChildren)
    {
        const TArray<int32>& Children = BoneChildren[BoneIndex];
        if (Children.IsEmpty())
        {
            return 0.0f;
        }

        // Use distance to first child as the bone length
        const FVector& BonePos = BonePositions[BoneIndex];
        const FVector& ChildPos = BonePositions[Children[0]];
        return (ChildPos - BonePos).Size();
    }

    /**
     * Structure to track merged bone data for a parent bone.
     * When small bones are merged into their parent, we track:
     * - Which bone indices were merged
     * - The accumulated length (for sizing the final capsule)
     */
    struct FMergedBoneData
    {
        TArray<int32> MergedBoneIndices;  // Bones that were merged into this one
        float AccumulatedLength = 0.0f;    // Total length of merged bones
    };
}

bool FPhysicsAssetUtils::CreateFromSkeletalMesh(UPhysicsAsset* PhysicsAsset, const USkeletalMesh* SkeletalMesh, const FPhysAssetCreateParams& Params)
{
    if (!PhysicsAsset)
    {
        UE_LOG("[PhysicsAssetUtils] CreateFromSkeletalMesh: PhysicsAsset is null");
        return false;
    }

    if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
    {
        UE_LOG("[PhysicsAssetUtils] CreateFromSkeletalMesh: Invalid skeletal mesh or skeleton");
        return false;
    }

    // Clear existing data
    PhysicsAsset->BodySetups.Empty();
    PhysicsAsset->BoneNameToBodyIndex.clear();
    PhysicsAsset->SourceSkeletalPath = SkeletalMesh->GetPathFileName();

    return CreateFromSkeletalMeshInternal(PhysicsAsset, SkeletalMesh, Params);
}

bool FPhysicsAssetUtils::CreateFromSkeletalMeshInternal(UPhysicsAsset* PhysicsAsset, const USkeletalMesh* SkeletalMesh, const FPhysAssetCreateParams& Params)
{
    const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    const TArray<FBone>& Bones = Skeleton->Bones;
    const int32 NumBones = Bones.Num();

    // Build children list for each bone
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
        const FMatrix& BindPose = Bones[i].BindPose;
        BonePositions[i] = FVector(BindPose.M[3][0], BindPose.M[3][1], BindPose.M[3][2]);
    }

    // ==================== BONE MERGING LOGIC ====================
    // Strategy (from UE's CreateFromSkeletalMeshInternal):
    // 1. Calculate size for each bone (bone length)
    // 2. Work from leaves up - if bone is too small, merge into parent
    // 3. Track accumulated merged sizes
    // 4. Only create bodies for bones big enough after merging

    // Track merged sizes (starts with each bone's own length)
    TArray<float> MergedSizes;
    MergedSizes.SetNum(NumBones);
    for (int32 i = 0; i < NumBones; ++i)
    {
        MergedSizes[i] = CalcBoneLength(i, BonePositions, BoneChildren);
    }

    // Map of parent bone index -> merged bone data
    TMap<int32, FMergedBoneData> BoneToMergedData;

    // Process bones from leaves to root (reverse order ensures children processed before parents)
    for (int32 BoneIdx = NumBones - 1; BoneIdx >= 0; --BoneIdx)
    {
        const float MyMergedSize = MergedSizes[BoneIdx];

        // If bone is too small to make a body for, but big enough to weld, merge with parent
        if (MyMergedSize < Params.MinBoneSize && MyMergedSize >= Params.MinWeldSize)
        {
            const int32 ParentIndex = Bones[BoneIdx].ParentIndex;
            if (ParentIndex >= 0 && ParentIndex < NumBones)
            {
                // Add this bone's size to parent's merged size
                MergedSizes[ParentIndex] += MyMergedSize;

                // Track this bone as merged into parent
                FMergedBoneData& ParentMergedData = BoneToMergedData[ParentIndex];
                ParentMergedData.MergedBoneIndices.Add(BoneIdx);
                ParentMergedData.AccumulatedLength += MyMergedSize;

                // If this bone had bones merged into it, transfer them to parent too
                if (FMergedBoneData* MyMergedData = BoneToMergedData.Find(BoneIdx))
                {
                    ParentMergedData.MergedBoneIndices.Append(MyMergedData->MergedBoneIndices);
                    ParentMergedData.AccumulatedLength += MyMergedData->AccumulatedLength;
                    BoneToMergedData.Remove(BoneIdx);
                }

                UE_LOG("[PhysicsAssetUtils] Merging bone '%s' (size=%.3f) into parent '%s'",
                       Bones[BoneIdx].Name.c_str(), MyMergedSize, Bones[ParentIndex].Name.c_str());
            }
        }
    }

    // Ensure there's a single root body - find if we need to force a root
    int32 ForcedRootBoneIndex = -1;
    int32 FirstParentBoneIndex = -1;

    for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
    {
        if (MergedSizes[BoneIndex] >= Params.MinBoneSize || Params.bBodyForAll)
        {
            const int32 ParentBoneIndex = Bones[BoneIndex].ParentIndex;
            if (ParentBoneIndex < 0)
            {
                // Already have a root body
                break;
            }
            else if (FirstParentBoneIndex < 0)
            {
                FirstParentBoneIndex = ParentBoneIndex;
            }
            else if (ParentBoneIndex == FirstParentBoneIndex)
            {
                // Two "root" bodies share a parent - force that parent as root
                ForcedRootBoneIndex = ParentBoneIndex;
                break;
            }
        }
    }

    // Lambda to determine if we should make a body for this bone
    auto ShouldMakeBone = [&](int32 BoneIndex) -> bool
    {
        if (Params.bBodyForAll)
        {
            return true;
        }
        if (MergedSizes[BoneIndex] >= Params.MinBoneSize)
        {
            return true;
        }
        if (BoneIndex == ForcedRootBoneIndex)
        {
            return true;
        }
        return false;
    };

    // ==================== CREATE BODIES ====================
    for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
    {
        if (!ShouldMakeBone(BoneIdx))
        {
            continue;
        }

        const FBone& Bone = Bones[BoneIdx];
        const TArray<int32>& Children = BoneChildren[BoneIdx];

        // Skip bones with no children (leaf bones) - unless they have merged children
        const FMergedBoneData* MergedData = BoneToMergedData.Find(BoneIdx);
        if (Children.IsEmpty() && !MergedData)
        {
            continue;
        }

        // Calculate capsule parameters
        const FVector& BonePos = BonePositions[BoneIdx];

        // Find the furthest endpoint for this bone's capsule
        // This could be the first child, or a merged bone's endpoint
        FVector FurthestEndpoint = BonePos;
        float MaxDistance = 0.0f;

        // Check first child
        if (!Children.IsEmpty())
        {
            FVector ChildPos = BonePositions[Children[0]];
            float Dist = (ChildPos - BonePos).Size();
            if (Dist > MaxDistance)
            {
                MaxDistance = Dist;
                FurthestEndpoint = ChildPos;
            }
        }

        // Check merged bones - find the furthest endpoint among them
        if (MergedData)
        {
            for (int32 MergedIdx : MergedData->MergedBoneIndices)
            {
                // Check merged bone's children
                const TArray<int32>& MergedChildren = BoneChildren[MergedIdx];
                for (int32 MergedChildIdx : MergedChildren)
                {
                    FVector MergedChildPos = BonePositions[MergedChildIdx];
                    float Dist = (MergedChildPos - BonePos).Size();
                    if (Dist > MaxDistance)
                    {
                        MaxDistance = Dist;
                        FurthestEndpoint = MergedChildPos;
                    }
                }
            }
        }

        float BoneLength = MaxDistance;

        // Skip if no valid length
        if (BoneLength < Params.MinWeldSize)
        {
            continue;
        }

        // Heuristic radius: ~15% of bone length, with reasonable min/max
        float Radius = FMath::Clamp(BoneLength * 0.15f, Params.MinWeldSize * 2, Params.MinBoneSize * 5);

        // Transform the endpoint from model-space to this bone's local space
        FVector EndpointInBoneLocal = Bone.InverseBindPose.TransformPosition(FurthestEndpoint);

        // Capsule center is at the midpoint
        FVector LocalCenter = EndpointInBoneLocal * 0.5f;

        // Bone direction in bone-local space
        FVector BoneDir = EndpointInBoneLocal.GetSafeNormal();

        // Compute Euler angles to rotate from X-axis to bone direction
        FVector EulerRot(0, 0, 0);
        if (BoneDir.SizeSquared() > 0.001f)
        {
            float Pitch = std::atan2(-BoneDir.Z, std::sqrt(BoneDir.X * BoneDir.X + BoneDir.Y * BoneDir.Y));
            float Yaw = std::atan2(BoneDir.Y, BoneDir.X);
            EulerRot = FVector(0, RadiansToDegrees(Pitch), RadiansToDegrees(Yaw));
        }

        // Create BodySetup with capsule
        UBodySetup* NewSetup = NewObject<UBodySetup>();
        NewSetup->BoneName = FName(Bone.Name);
        NewSetup->CollisionTraceFlag = ECollisionTraceFlag::UseDefault;

        FKCapsuleElem Capsule;
        Capsule.Center = LocalCenter;
        Capsule.Rotation = EulerRot;
        Capsule.Radius = Radius;
        Capsule.Length = BoneLength - Radius * 2.0f;
        if (Capsule.Length < Params.MinWeldSize)
        {
            Capsule.Length = Params.MinWeldSize;
        }

        NewSetup->AggGeom.SphylElems.Add(Capsule);

        if (MergedData && !MergedData->MergedBoneIndices.IsEmpty())
        {
            UE_LOG("[PhysicsAssetUtils] Bone '%s' (merged %d bones): Capsule R=%.3f, L=%.3f",
                   Bone.Name.c_str(), MergedData->MergedBoneIndices.Num(), Capsule.Radius, Capsule.Length);
        }
        else
        {
            UE_LOG("[PhysicsAssetUtils] Bone '%s': Capsule R=%.3f, L=%.3f, Center=(%.2f,%.2f,%.2f)",
                   Bone.Name.c_str(), Capsule.Radius, Capsule.Length,
                   LocalCenter.X, LocalCenter.Y, LocalCenter.Z);
        }

        PhysicsAsset->AddBodySetup(NewSetup);
    }

    UE_LOG("[PhysicsAssetUtils] Built %d body setups from skeleton with %d bones",
           PhysicsAsset->BodySetups.Num(), NumBones);

    return PhysicsAsset->BodySetups.Num() > 0;
}

UBodySetup* FPhysicsAssetUtils::CreateBodySetupForBone(UPhysicsAsset* PhysicsAsset, FName BoneName)
{
    if (!PhysicsAsset)
    {
        return nullptr;
    }

    // Check if body already exists for this bone
    if (PhysicsAsset->FindBodySetupByBoneName(BoneName))
    {
        UE_LOG("[PhysicsAssetUtils] Body already exists for bone '%s'", BoneName.ToString().c_str());
        return nullptr;
    }

    // Create new body setup
    UBodySetup* NewSetup = NewObject<UBodySetup>();
    NewSetup->BoneName = BoneName;
    NewSetup->CollisionTraceFlag = ECollisionTraceFlag::UseDefault;

    PhysicsAsset->AddBodySetup(NewSetup);

    return NewSetup;
}
