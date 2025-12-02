#include "pch.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsAsset.h"
#include "BodySetup.h"
#include "CapsuleElem.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"

static const float MinPrimSize = 0.01f;
static const float MinBoneWeightThreshold = 0.1f; // Minimum weight to consider a vertex belonging to a bone

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

/**
 * Calculate vertex info for each bone based on skin weights.
 * For each bone, collects all vertices that have significant weight to that bone.
 *
 * @param SkeletalMesh  The skeletal mesh to extract vertex data from
 * @param Infos         Output array of FBoneVertInfo, one per bone
 * @param bDominantWeight If true, only assign vertex to bone with highest weight. If false, assign to all bones above threshold.
 */
static void CalcBoneVertInfos(const USkeletalMesh* SkeletalMesh, TArray<FBoneVertInfo>& Infos, bool bDominantWeight = true)
{
    const FSkeletalMeshData* MeshData = SkeletalMesh->GetSkeletalMeshData();
    if (!MeshData)
    {
        return;
    }

    const int32 NumBones = MeshData->Skeleton.Bones.Num();
    const TArray<FSkinnedVertex>& Vertices = MeshData->Vertices;

    // Initialize output array with one entry per bone
    Infos.SetNum(NumBones);

    // Process each vertex
    for (const FSkinnedVertex& Vertex : Vertices)
    {
        if (bDominantWeight)
        {
            // Find bone with highest weight
            int32 DominantBoneIndex = -1;
            float MaxWeight = 0.0f;

            for (int32 i = 0; i < 4; ++i)
            {
                if (Vertex.BoneWeights[i] > MaxWeight)
                {
                    MaxWeight = Vertex.BoneWeights[i];
                    DominantBoneIndex = static_cast<int32>(Vertex.BoneIndices[i]);
                }
            }

            // Add vertex to dominant bone
            if (DominantBoneIndex >= 0 && DominantBoneIndex < NumBones && MaxWeight > MinBoneWeightThreshold)
            {
                Infos[DominantBoneIndex].Positions.Add(Vertex.Position);
                Infos[DominantBoneIndex].Normals.Add(Vertex.Normal);
            }
        }
        else
        {
            // Add vertex to all bones with weight above threshold
            for (int32 i = 0; i < 4; ++i)
            {
                if (Vertex.BoneWeights[i] > MinBoneWeightThreshold)
                {
                    int32 BoneIndex = static_cast<int32>(Vertex.BoneIndices[i]);
                    if (BoneIndex >= 0 && BoneIndex < NumBones)
                    {
                        Infos[BoneIndex].Positions.Add(Vertex.Position);
                        Infos[BoneIndex].Normals.Add(Vertex.Normal);
                    }
                }
            }
        }
    }

    UE_LOG("[PhysicsAssetUtils] CalcBoneVertInfos: Processed %d vertices for %d bones", Vertices.Num(), NumBones);
}

FMatrix ComputeCovarianceMatrix(const FBoneVertInfo& VertInfo)
{
	if (VertInfo.Positions.Num() == 0)
	{
		return FMatrix::Identity();
	}

	const TArray<FVector>& Positions = VertInfo.Positions;

	//get average
	const int32 N = Positions.Num();
	FVector U = FVector(0, 0, 0);
	for (int32 i = 0; i < N; ++i)
	{
		U += Positions[i];
	}

	U = U / static_cast<float>(N);

	//compute error terms
	TArray<FVector> Errors;
	Errors.SetNum(N);

	for (int32 i = 0; i < N; ++i)
	{
		Errors[i] = Positions[i] - U;
	}

	FMatrix Covariance = FMatrix::Identity();
	for (int32 j = 0; j < 3; ++j)
	{
		for (int32 k = 0; k < 3; ++k)
		{
			float Cjk = 0.f;
			for (int32 i = 0; i < N; ++i)
			{
				const float* error = &Errors[i].X;
				Cjk += error[j] * error[k];
			}
			Covariance.M[j][k] = Cjk / static_cast<float>(N);
		}
	}

	return Covariance;
}

FVector ComputeEigenVector(const FMatrix& A)
{
	//using the power method: this is ok because we only need the dominate eigenvector and speed is not critical: http://en.wikipedia.org/wiki/Power_iteration
	FVector Bk = FVector(0, 0, 1);
	for (int32 i = 0; i < 32; ++i)
	{
		float Length = Bk.Size();
		if (Length > 0.f)
		{
			Bk = A.TransformVector(Bk) / Length;
		}
	}

	return Bk.GetSafeNormal();
}

bool FPhysicsAssetUtils::CreateCollisionFromBoneInternal(UBodySetup* BodySetup, const FBoneVertInfo& Info, const std::string& BoneName)
{
    if (!BodySetup)
    {
        return false;
    }

    if (Info.Positions.Num() == 0)
    {
        UE_LOG("[PhysicsAssetUtils] CreateCollisionFromBoneInternal: No vertex data for bone '%s'", BoneName.c_str());
        return false;
    }

    // Vertices are already in bone-local space (bone origin is at 0,0,0)
    // Compute AABB to get the extents of the vertex cloud
    FVector BoxMin(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector BoxMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (const FVector& Pos : Info.Positions)
    {
        BoxMin.X = FMath::Min(BoxMin.X, Pos.X);
        BoxMin.Y = FMath::Min(BoxMin.Y, Pos.Y);
        BoxMin.Z = FMath::Min(BoxMin.Z, Pos.Z);
        BoxMax.X = FMath::Max(BoxMax.X, Pos.X);
        BoxMax.Y = FMath::Max(BoxMax.Y, Pos.Y);
        BoxMax.Z = FMath::Max(BoxMax.Z, Pos.Z);
    }

    FVector BoxCenter = (BoxMin + BoxMax) * 0.5f;
    FVector BoxExtent = (BoxMax - BoxMin) * 0.5f;

    // Skip if too small
    float MaxExtent = FMath::Max(FMath::Max(BoxExtent.X, BoxExtent.Y), BoxExtent.Z);
    if (MaxExtent < MinPrimSize)
    {
        UE_LOG("[PhysicsAssetUtils] CreateCollisionFromBoneInternal: Bone '%s' too small", BoneName.c_str());
        return false;
    }

    // Use eigenvector to find the dominant axis (direction of maximum variance)
    const FMatrix CovarianceMatrix = ComputeCovarianceMatrix(Info);
    FVector LongAxis = ComputeEigenVector(CovarianceMatrix);

    // Capsule center is at the AABB center of the vertex cloud
    FVector LocalCenter = BoxCenter;

    // Compute Euler angles to rotate from X-axis (PhysX capsule default) to long axis
    FVector EulerRot(0, 0, 0);
    if (LongAxis.SizeSquared() > 0.001f)
    {
        LongAxis = LongAxis.GetSafeNormal();

        // Pitch: rotation around Y to tilt up/down toward Z
        float Pitch = std::atan2(-LongAxis.Z, std::sqrt(LongAxis.X * LongAxis.X + LongAxis.Y * LongAxis.Y));
        // Yaw: rotation around Z to align in XY plane
        float Yaw = std::atan2(LongAxis.Y, LongAxis.X);

        EulerRot = FVector(0, RadiansToDegrees(Pitch), RadiansToDegrees(Yaw));
    }

    // Compute length and radius based on AABB extents
    // Length is along the longest axis, radius is the average of the other two
    float LengthExtent = FMath::Max(FMath::Max(BoxExtent.X, BoxExtent.Y), BoxExtent.Z);
    float MinExtent = FMath::Min(FMath::Min(BoxExtent.X, BoxExtent.Y), BoxExtent.Z);
    float MidExtent = BoxExtent.X + BoxExtent.Y + BoxExtent.Z - LengthExtent - MinExtent;

    // Radius is average of the two smaller extents
    float Radius = (MinExtent + MidExtent) * 0.5f;
    Radius = FMath::Max(Radius, MinPrimSize);

    // Length is the full extent along the long axis (diameter), minus the caps
    float BoneLength = LengthExtent * 2.0f;
    float CapsuleLength = FMath::Max(BoneLength - Radius * 2.0f, MinPrimSize);

    FKCapsuleElem Capsule;
    Capsule.Center = LocalCenter;
    Capsule.Rotation = EulerRot;
    Capsule.Radius = Radius;
    Capsule.Length = CapsuleLength;

    BodySetup->AggGeom.SphylElems.Add(Capsule);

    UE_LOG("[PhysicsAssetUtils] Bone '%s': Capsule R=%.3f, L=%.3f, Center=(%.2f,%.2f,%.2f)",
           BoneName.c_str(), Capsule.Radius, Capsule.Length,
           Capsule.Center.X, Capsule.Center.Y, Capsule.Center.Z);

    return true;
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

    // Calculate vertex info for each bone from actual skinned vertex data
    TArray<FBoneVertInfo> BoneVertInfos;
    CalcBoneVertInfos(SkeletalMesh, BoneVertInfos, true); // Use dominant weight

    // ==================== BONE MERGING LOGIC ====================
    // Strategy (from UE's CreateFromSkeletalMeshInternal):
    // 1. Calculate size for each bone (bone length)
    // 2. Work from leaves up - if bone is too small, merge into parent
    // 3. Track accumulated merged sizes and merged vertex info
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

                // Merge vertex info into parent
                BoneVertInfos[ParentIndex].Positions.Append(BoneVertInfos[BoneIdx].Positions);
                BoneVertInfos[ParentIndex].Normals.Append(BoneVertInfos[BoneIdx].Normals);

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

        // Transform the endpoint from model-space to this bone's local space.
        // Use the pre-computed InverseBindPose from the bone data.
        FVector EndpointInBoneLocal = Bone.InverseBindPose.TransformPosition(FurthestEndpoint);

        // In bone-local space, the bone origin is at (0,0,0).
        // The capsule center is at the midpoint between origin and endpoint.
        FVector LocalCenter = EndpointInBoneLocal * 0.5f;

        // Bone direction in bone-local space
        FVector BoneDir = EndpointInBoneLocal.GetSafeNormal();

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

        // Compute radius from vertex data if available, otherwise use heuristic
        const FBoneVertInfo& VertInfo = BoneVertInfos[BoneIdx];
        float Radius;

        if (VertInfo.Positions.Num() > 0)
        {
            // Compute radius based on vertex spread perpendicular to bone direction
            float MaxPerpDist = 0.0f;
            for (const FVector& Pos : VertInfo.Positions)
            {
                // Transform vertex to bone-local space
                FVector LocalPos = Pos - BonePos;
                // Project onto bone direction (in model space, before InverseBindPose)
                FVector BoneDirModel = (FurthestEndpoint - BonePos).GetSafeNormal();
                float AlongBone = FVector::Dot(LocalPos, BoneDirModel);
                // Get perpendicular component
                FVector PerpComponent = LocalPos - BoneDirModel * AlongBone;
                float PerpDist = PerpComponent.Size();
                MaxPerpDist = FMath::Max(MaxPerpDist, PerpDist);
            }
            // Use perpendicular distance as radius, clamped reasonably
            Radius = FMath::Clamp(MaxPerpDist, BoneLength * 0.1f, BoneLength * 0.4f);
        }
        else
        {
            // Heuristic radius: ~15% of bone length, with reasonable min/max
            Radius = FMath::Clamp(BoneLength * 0.15f, Params.MinWeldSize * 2, Params.MinBoneSize * 5);
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
