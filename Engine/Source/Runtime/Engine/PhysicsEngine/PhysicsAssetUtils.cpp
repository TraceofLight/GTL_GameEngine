#include "pch.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsAsset.h"
#include "BodySetup.h"
#include "CapsuleElem.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"

bool FPhysicsAssetUtils::CreateFromSkeletalMesh(UPhysicsAsset* PhysicsAsset, const USkeletalMesh* SkeletalMesh)
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

    const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
    const TArray<FBone>& Bones = Skeleton->Bones;
    const int32 NumBones = Bones.Num();

    // Clear existing data
    PhysicsAsset->BodySetups.Empty();
    PhysicsAsset->BoneNameToBodyIndex.clear();
    PhysicsAsset->SourceSkeletalPath = SkeletalMesh->GetPathFileName();

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

        // Use the FIRST child only for bone direction/length
        // Averaging multiple children causes asymmetric results between left/right sides
        // The first child is typically the "main" bone in the chain (e.g., forearm for upper arm)
        const FVector& BonePos = BonePositions[BoneIdx];
        FVector ChildPos = BonePositions[Children[0]];
        FVector BoneVec = ChildPos - BonePos;
        float BoneLength = BoneVec.Size();

        // Skip very short bones
        if (BoneLength < 0.01f)
        {
            continue;
        }

        // Heuristic radius: ~15% of bone length, with reasonable min/max
        float Radius = FMath::Clamp(BoneLength * 0.15f, 0.02f, 0.5f);

        // Transform the child position from model-space to this bone's local space.
        // Use the pre-computed InverseBindPose from the bone data.
        FVector ChildInBoneLocal = Bone.InverseBindPose.TransformPosition(ChildPos);

        // In bone-local space, the bone origin is at (0,0,0).
        // The capsule center is at the midpoint between origin and child position.
        FVector LocalCenter = ChildInBoneLocal * 0.5f;

        // Bone direction in bone-local space
        FVector BoneDir = ChildInBoneLocal.GetSafeNormal();

        // Compute rotation to align capsule X-axis with bone direction
        // PhysX capsules are X-axis aligned by default

        // Compute Euler angles to rotate from X-axis to bone direction (in bone-local space)
        FVector EulerRot(0, 0, 0);
        if (BoneDir.SizeSquared() > 0.001f)
        {
            // Pitch: rotation around Y to align with XZ plane
            float Pitch = std::atan2(-BoneDir.Z, std::sqrt(BoneDir.X * BoneDir.X + BoneDir.Y * BoneDir.Y));
            // Yaw: rotation around Z to align with XY plane
            float Yaw = std::atan2(BoneDir.Y, BoneDir.X);

            EulerRot = FVector(0, RadiansToDegrees(Pitch), RadiansToDegrees(Yaw));
        }

        // Create BodySetup with capsule (heap allocated like UE)
        UBodySetup* NewSetup = NewObject<UBodySetup>();
        NewSetup->BoneName = FName(Bone.Name);
        NewSetup->CollisionTraceFlag = ECollisionTraceFlag::UseDefault;

        FKCapsuleElem Capsule;
        Capsule.Center = LocalCenter;
        Capsule.Rotation = EulerRot;
        Capsule.Radius = Radius;
        Capsule.Length = BoneLength - Radius * 2.0f; // Length is the cylindrical part
        if (Capsule.Length < 0.01f)
        {
            Capsule.Length = 0.01f;
        }

        NewSetup->AggGeom.SphylElems.Add(Capsule);

        UE_LOG("[PhysicsAssetUtils] Bone '%s': Capsule R=%.3f, L=%.3f, Center=(%.2f,%.2f,%.2f)",
               Bone.Name.c_str(), Capsule.Radius, Capsule.Length,
               LocalCenter.X, LocalCenter.Y, LocalCenter.Z);

        // Add to physics asset
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
