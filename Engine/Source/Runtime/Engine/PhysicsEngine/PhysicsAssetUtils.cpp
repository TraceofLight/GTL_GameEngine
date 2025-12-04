#include "pch.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsAsset.h"
#include "BodySetup.h"
#include "CapsuleElem.h"
#include "PhysicsConstraintSetup.h"
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

// 관절 제한값 구조체
struct FJointLimitPreset
{
	float Swing1;
	float Swing2;
	float Twist;
};

// 본 이름 기반 관절 제한값 결정 (인체 해부학 기반)
static FJointLimitPreset GetJointLimitsByBoneName(const FString& BoneName)
{
	FString UpperName = BoneName;
	std::transform(UpperName.begin(), UpperName.end(), UpperName.begin(), ::tolower);

	// Spine (척추) - 제한적인 움직임
	if (UpperName.find("spine") != FString::npos)
	{
		return { 20.0f, 20.0f, 20.0f };
	}

	// Neck (목) - 고개 돌리기 가능
	if (UpperName.find("neck") != FString::npos)
	{
		return { 30.0f, 30.0f, 45.0f };
	}

	// Head (머리) - 목과 유사
	if (UpperName.find("head") != FString::npos)
	{
		return { 25.0f, 25.0f, 30.0f };
	}

	// Shoulder/Clavicle (어깨/쇄골) - 넓은 범위
	if (UpperName.find("shoulder") != FString::npos || UpperName.find("clavicle") != FString::npos)
	{
		return { 30.0f, 30.0f, 15.0f };
	}

	// UpperArm/Arm (상완) - 어깨 관절, 넓은 범위
	if (UpperName.find("upperarm") != FString::npos ||
		(UpperName.find("arm") != FString::npos && UpperName.find("fore") == FString::npos))
	{
		return { 90.0f, 60.0f, 80.0f };
	}

	// ForeArm/LowerArm (전완/팔꿈치) - 한 방향만 굽힘
	if (UpperName.find("forearm") != FString::npos || UpperName.find("lowerarm") != FString::npos ||
		UpperName.find("elbow") != FString::npos)
	{
		return { 5.0f, 120.0f, 10.0f };
	}

	// Hand/Wrist (손/손목) - 회전 자유
	if (UpperName.find("hand") != FString::npos || UpperName.find("wrist") != FString::npos)
	{
		return { 45.0f, 45.0f, 80.0f };
	}

	// Finger (손가락) - 한 방향 굽힘
	if (UpperName.find("finger") != FString::npos || UpperName.find("thumb") != FString::npos ||
		UpperName.find("index") != FString::npos || UpperName.find("middle") != FString::npos ||
		UpperName.find("ring") != FString::npos || UpperName.find("pinky") != FString::npos)
	{
		return { 5.0f, 90.0f, 10.0f };
	}

	// UpLeg/Thigh (허벅지/고관절) - 다리 벌림
	if (UpperName.find("upleg") != FString::npos || UpperName.find("thigh") != FString::npos ||
		UpperName.find("hip") != FString::npos)
	{
		return { 60.0f, 45.0f, 30.0f };
	}

	// Leg/Knee (정강이/무릎) - 한 방향만 굽힘
	if ((UpperName.find("leg") != FString::npos && UpperName.find("up") == FString::npos) ||
		UpperName.find("knee") != FString::npos || UpperName.find("calf") != FString::npos ||
		UpperName.find("shin") != FString::npos)
	{
		return { 5.0f, 120.0f, 5.0f };
	}

	// Foot/Ankle (발/발목) - 제한적
	if (UpperName.find("foot") != FString::npos || UpperName.find("ankle") != FString::npos)
	{
		return { 30.0f, 30.0f, 15.0f };
	}

	// Toe (발가락) - 약간의 굽힘
	if (UpperName.find("toe") != FString::npos)
	{
		return { 5.0f, 30.0f, 5.0f };
	}

	// 기본값 (알 수 없는 본)
	return { 45.0f, 45.0f, 30.0f };
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

    UE_LOG("PhysicsAssetUtils: CalcBoneVertInfos: %d vertices for %d bones", Vertices.Num(), NumBones);
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

bool FPhysicsAssetUtils::CreateCollisionFromBoneInternal(UBodySetup* BodySetup, const FBoneVertInfo& Info, const FMatrix& InverseBindPose, const std::string& BoneName)
{
    if (!BodySetup)
    {
        return false;
    }

    if (Info.Positions.Num() == 0)
    {
        UE_LOG("PhysicsAssetUtils: CreateCollision: No vertex data for '%s'", BoneName.c_str());
        return false;
    }

    // Step 1: Compute eigenvector from model-space vertices (like UE does)
    const FMatrix CovarianceMatrix = ComputeCovarianceMatrix(Info);
    FVector ZAxis = ComputeEigenVector(CovarianceMatrix);

    // Build rotation matrix from eigenvector (Z-axis aligned with dominant direction)
    FVector XAxis, YAxis;
    if (ZAxis.SizeSquared() > 0.001f)
    {
        ZAxis = ZAxis.GetSafeNormal();
        // Find perpendicular axes
        if (FMath::Abs(ZAxis.Z) < 0.9f)
        {
            XAxis = FVector::Cross(FVector(0, 0, 1), ZAxis).GetSafeNormal();
        }
        else
        {
            XAxis = FVector::Cross(FVector(1, 0, 0), ZAxis).GetSafeNormal();
        }
        YAxis = FVector::Cross(ZAxis, XAxis).GetSafeNormal();
    }
    else
    {
        XAxis = FVector(1, 0, 0);
        YAxis = FVector(0, 1, 0);
        ZAxis = FVector(0, 0, 1);
    }

    // ElementTransform rotates from world to eigenvector-aligned space
    // Build rotation matrix with XAxis, YAxis, ZAxis as rows (row-major)
    FMatrix ElemTM(
        XAxis.X, XAxis.Y, XAxis.Z, 0,
        YAxis.X, YAxis.Y, YAxis.Z, 0,
        ZAxis.X, ZAxis.Y, ZAxis.Z, 0,
        0, 0, 0, 1
    );
    FTransform ElementTransform(ElemTM);
    FTransform InverseTransform = ElementTransform.Inverse();

    // Step 2: Compute AABB in eigenvector-aligned space (like UE lines 720-724)
    // This gives us a tighter bounding box aligned to the principal axes
    FVector BoxMin(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector BoxMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);
    for (const FVector& Pos : Info.Positions)
    {
        // Transform vertex to eigenvector-aligned space using inverse transform
        FVector AlignedPos = InverseTransform.TransformPosition(Pos);
        BoxMin.X = FMath::Min(BoxMin.X, AlignedPos.X);
        BoxMin.Y = FMath::Min(BoxMin.Y, AlignedPos.Y);
        BoxMin.Z = FMath::Min(BoxMin.Z, AlignedPos.Z);
        BoxMax.X = FMath::Max(BoxMax.X, AlignedPos.X);
        BoxMax.Y = FMath::Max(BoxMax.Y, AlignedPos.Y);
        BoxMax.Z = FMath::Max(BoxMax.Z, AlignedPos.Z);
    }

    FVector BoxCenter = (BoxMax + BoxMin) * 0.5f;
    FVector BoxExtent = (BoxMax - BoxMin) * 0.5f;

    // Skip if too small
    float MaxExtent = FMath::Max(FMath::Max(BoxExtent.X, BoxExtent.Y), BoxExtent.Z);
    if (MaxExtent < MinPrimSize)
    {
        UE_LOG("PhysicsAssetUtils: CreateCollision: Bone '%s' too small", BoneName.c_str());
        return false;
    }

    // Step 3: Rotate capsule to align with LARGEST extent
    // PhysX capsules are X-axis aligned
    // We need to find which axis of the eigenvector-aligned AABB is largest,
    // then rotate so that axis becomes the capsule's X-axis
    FQuat AdditionalRotation = FQuat::Identity();
    float Radius;
    float CapsuleLength;

    if (BoxExtent.X > BoxExtent.Z && BoxExtent.X > BoxExtent.Y)
    {
        // X is biggest in eigenvector space - rotate so X becomes capsule axis
        // No additional rotation needed since PhysX capsule is already X-aligned
        Radius = FMath::Max(BoxExtent.Y, BoxExtent.Z);
        CapsuleLength = FMath::Max(BoxExtent.X * 2.0f - Radius * 2.0f, MinPrimSize);
    }
    else if (BoxExtent.Y > BoxExtent.Z && BoxExtent.Y > BoxExtent.X)
    {
        // Y is biggest - rotate Y-axis into X-axis (rotate -90 degrees around Z)
        AdditionalRotation = FQuat::FromAxisAngle(FVector(0, 0, 1), -PI * 0.5f);
        Radius = FMath::Max(BoxExtent.X, BoxExtent.Z);
        CapsuleLength = FMath::Max(BoxExtent.Y * 2.0f - Radius * 2.0f, MinPrimSize);
    }
    else
    {
        // Z is biggest - rotate Z-axis into X-axis (rotate 90 degrees around Y)
        AdditionalRotation = FQuat::FromAxisAngle(FVector(0, 1, 0), PI * 0.5f);
        Radius = FMath::Max(BoxExtent.X, BoxExtent.Y);
        CapsuleLength = FMath::Max(BoxExtent.Z * 2.0f - Radius * 2.0f, MinPrimSize);
    }

    Radius = FMath::Max(Radius, MinPrimSize);

    // Combine: first apply eigenvector rotation, then the additional rotation to align capsule axis
    FQuat EigenQuat = ElementTransform.Rotation;
    FQuat ModelSpaceRotation = EigenQuat * AdditionalRotation;
    ModelSpaceRotation.Normalize();

    // Transform the box center back to model space
    FVector ModelSpaceCenter = ElementTransform.TransformPosition(BoxCenter);

    // Transform to bone-local space
    // We need to use TransformVector (rotation only) for the offset from bone origin,
    // not TransformPosition which would add the inverse bind pose translation
    FTransform InvBindTransform(InverseBindPose);

    // Get bone position in model space (from the bind pose, which is the inverse of InverseBindPose)
    FMatrix BindPose = InverseBindPose.Inverse();
    FVector BoneModelPos(BindPose.M[3][0], BindPose.M[3][1], BindPose.M[3][2]);

    // Calculate offset from bone in model space, then rotate to bone-local space
    FVector OffsetFromBone = ModelSpaceCenter - BoneModelPos;
    FVector LocalCenter = InvBindTransform.Rotation.RotateVector(OffsetFromBone);

    // Transform rotation from model-space to bone-local space
    FQuat FinalRotation = InvBindTransform.Rotation * ModelSpaceRotation;
    FinalRotation.Normalize();

    // Convert final quaternion to Euler angles (ZYX order)
    float SingularityTest = 2.0f * (FinalRotation.W * FinalRotation.Y - FinalRotation.Z * FinalRotation.X);
    float Yaw, Pitch, Roll;
    if (FMath::Abs(SingularityTest) < 0.9999f)
    {
        Roll = std::atan2(2.0f * (FinalRotation.W * FinalRotation.X + FinalRotation.Y * FinalRotation.Z),
                          1.0f - 2.0f * (FinalRotation.X * FinalRotation.X + FinalRotation.Y * FinalRotation.Y));
        Pitch = std::asin(SingularityTest);
        Yaw = std::atan2(2.0f * (FinalRotation.W * FinalRotation.Z + FinalRotation.X * FinalRotation.Y),
                         1.0f - 2.0f * (FinalRotation.Y * FinalRotation.Y + FinalRotation.Z * FinalRotation.Z));
    }
    else
    {
        // Gimbal lock
        Roll = 0.0f;
        Pitch = (SingularityTest > 0) ? (PI * 0.5f) : (-PI * 0.5f);
        Yaw = std::atan2(-2.0f * (FinalRotation.X * FinalRotation.Y - FinalRotation.W * FinalRotation.Z),
                         1.0f - 2.0f * (FinalRotation.X * FinalRotation.X + FinalRotation.Z * FinalRotation.Z));
    }
    FVector EulerRot = FVector(RadiansToDegrees(Roll), RadiansToDegrees(Pitch), RadiansToDegrees(Yaw));

    FKCapsuleElem Capsule;
    Capsule.Center = LocalCenter;
    Capsule.Rotation = EulerRot;
    Capsule.Radius = Radius;
    Capsule.Length = CapsuleLength;

    BodySetup->AggGeom.SphylElems.Add(Capsule);

    UE_LOG("PhysicsAssetUtils: CreateCapsule: '%s' R=%.3f L=%.3f",
           BoneName.c_str(), Capsule.Radius, Capsule.Length);

    return true;
}

bool FPhysicsAssetUtils::CreateFromSkeletalMesh(UPhysicsAsset* PhysicsAsset, const USkeletalMesh* SkeletalMesh, const FPhysAssetCreateParams& Params)
{
    if (!PhysicsAsset)
    {
        UE_LOG("PhysicsAssetUtils: CreateFromSkeletalMesh: PhysicsAsset is null");
        return false;
    }

    if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
    {
        UE_LOG("PhysicsAssetUtils: CreateFromSkeletalMesh: Invalid mesh or skeleton");
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

                UE_LOG("PhysicsAssetUtils: MergeBone: '%s' (%.3f) -> '%s'",
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

        // Create BodySetup for this bone
        UBodySetup* NewSetup = NewObject<UBodySetup>();
        NewSetup->BoneName = FName(Bone.Name);
        NewSetup->CollisionTraceFlag = ECollisionTraceFlag::UseDefault;

        // Use vertex data to create collision via CreateCollisionFromBoneInternal
        const FBoneVertInfo& VertInfo = BoneVertInfos[BoneIdx];
        bool bCreatedFromVerts = false;

        if (VertInfo.Positions.Num() > 0)
        {
            // Use the eigenvector-based collision creation from vertex data
            // Pass InverseBindPose to transform vertices from model-space to bone-local space
            bCreatedFromVerts = CreateCollisionFromBoneInternal(NewSetup, VertInfo, Bone.InverseBindPose, Bone.Name);
        }

        if (!bCreatedFromVerts)
        {
            // Fallback: create capsule from bone hierarchy when no vertex data or creation failed
            FVector EndpointInBoneLocal = Bone.InverseBindPose.TransformPosition(FurthestEndpoint);
            FVector LocalCenter = EndpointInBoneLocal * 0.5f;
            FVector BoneDir = EndpointInBoneLocal.GetSafeNormal();

            FVector EulerRot(0, 0, 0);
            if (BoneDir.SizeSquared() > 0.001f)
            {
                float Pitch = std::atan2(-BoneDir.Z, std::sqrt(BoneDir.X * BoneDir.X + BoneDir.Y * BoneDir.Y));
                float Yaw = std::atan2(BoneDir.Y, BoneDir.X);
                EulerRot = FVector(0, RadiansToDegrees(Pitch), RadiansToDegrees(Yaw));
            }

            float Radius = FMath::Clamp(BoneLength * 0.15f, Params.MinWeldSize * 2, Params.MinBoneSize * 5);
            float CapsuleLength = FMath::Max(BoneLength - Radius * 2.0f, Params.MinWeldSize);

            FKCapsuleElem Capsule;
            Capsule.Center = LocalCenter;
            Capsule.Rotation = EulerRot;
            Capsule.Radius = Radius;
            Capsule.Length = CapsuleLength;

            NewSetup->AggGeom.SphylElems.Add(Capsule);

            UE_LOG("PhysicsAsset: Build: Bone '%s' Capsule R=%.3f L=%.3f",
                   Bone.Name.c_str(), Capsule.Radius, Capsule.Length);
        }
        else if (MergedData && !MergedData->MergedBoneIndices.IsEmpty())
        {
            UE_LOG("PhysicsAsset: Build: Bone '%s' merged %d bones",
                   Bone.Name.c_str(), MergedData->MergedBoneIndices.Num());
        }

        PhysicsAsset->AddBodySetup(NewSetup);
    }

    UE_LOG("PhysicsAsset: Build: %d bodies from %d bones",
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
        UE_LOG("PhysicsAsset: CreateBody: Already exists '%s'", BoneName.ToString().c_str());
        return nullptr;
    }

    // Create new body setup
    UBodySetup* NewSetup = NewObject<UBodySetup>();
    NewSetup->BoneName = BoneName;
    NewSetup->CollisionTraceFlag = ECollisionTraceFlag::UseDefault;

    PhysicsAsset->AddBodySetup(NewSetup);

    return NewSetup;
}

bool FPhysicsAssetUtils::CreateConstraintsForRagdoll(UPhysicsAsset* PhysicsAsset, const USkeletalMesh* SkeletalMesh)
{
	if (!PhysicsAsset)
	{
		UE_LOG("PhysicsAsset: CreateConstraints: PhysicsAsset is null");
		return false;
	}

	if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
	{
		UE_LOG("PhysicsAsset: CreateConstraints: Invalid skeletal mesh");
		return false;
	}

	const FSkeleton* Skeleton = SkeletalMesh->GetSkeleton();
	const TArray<FBone>& Bones = Skeleton->Bones;
	const int32 NumBones = Bones.Num();

	// Component space에서 본 Transform 계산 (Joint 위치 계산용)
	TArray<FTransform> BoneTransforms;
	BoneTransforms.SetNum(NumBones);
	for (int32 i = 0; i < NumBones; ++i)
	{
		BoneTransforms[i] = FTransform(Bones[i].BindPose);
	}

	int32 ConstraintCount = 0;

	// 각 본에 대해 Body가 있는 가장 가까운 조상 본과 연결하는 Constraint 생성
	for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
	{
		const FBone& Bone = Bones[BoneIdx];

		// 이 본에 Body가 없으면 스킵
		FName ChildBoneName = FName(Bone.Name);
		int32 ChildBodyIdx = PhysicsAsset->FindBodyIndexByBoneName(ChildBoneName);
		if (ChildBodyIdx == -1)
		{
			// Body가 없는 본은 로그 안 함 (너무 많음)
			continue;
		}

		// Body가 있는 가장 가까운 조상 본 찾기
		int32 AncestorBoneIdx = Bone.ParentIndex;
		int32 AncestorBodyIdx = -1;
		FName AncestorBoneName;

		while (AncestorBoneIdx >= 0)
		{
			AncestorBoneName = FName(Bones[AncestorBoneIdx].Name);
			AncestorBodyIdx = PhysicsAsset->FindBodyIndexByBoneName(AncestorBoneName);

			if (AncestorBodyIdx != -1)
			{
				// Body가 있는 조상 찾음
				break;
			}

			// 더 위로 올라가기
			AncestorBoneIdx = Bones[AncestorBoneIdx].ParentIndex;
		}

		// 루트까지 갔는데도 Body가 있는 조상이 없으면 스킵 (루트 본)
		if (AncestorBodyIdx == -1)
		{
			continue;
		}

		// 이미 Constraint가 있으면 스킵
		if (PhysicsAsset->FindConstraintByBoneNames(AncestorBoneName, ChildBoneName))
		{
			continue;
		}

		// Joint 위치 계산: Child 본 위치를 Ancestor Body의 로컬 좌표계로 변환
		// Body는 본의 ComponentSpace Transform으로 생성되므로,
		// Child의 ComponentSpace 위치를 Ancestor Transform의 역변환으로 로컬 좌표로 변환
		const FTransform& AncestorTransform = BoneTransforms[AncestorBoneIdx];
		const FTransform& ChildTransform = BoneTransforms[BoneIdx];

		// Child의 Component Space 위치를 Ancestor의 로컬 좌표계로 변환
		// 역변환: (ChildPos - AncestorPos)를 Ancestor 회전의 역으로 회전
		FVector ChildWorldPos = ChildTransform.Translation;
		FVector AncestorWorldPos = AncestorTransform.Translation;
		FVector Offset = ChildWorldPos - AncestorWorldPos;
		FVector ChildPosInAncestorLocal = AncestorTransform.Rotation.Inverse().RotateVector(Offset);

		// Constraint Setup 생성
		UPhysicsConstraintSetup* NewConstraint = NewObject<UPhysicsConstraintSetup>();
		NewConstraint->ConstraintBone1 = AncestorBoneName;	// Ancestor = Body1
		NewConstraint->ConstraintBone2 = ChildBoneName;		// Child = Body2
		NewConstraint->BodyIndex1 = AncestorBodyIdx;
		NewConstraint->BodyIndex2 = ChildBodyIdx;

		// 기본 Ragdoll 설정 (적당한 각도 제한)
		NewConstraint->LinearXMotion = ELinearConstraintMotion::Locked;
		NewConstraint->LinearYMotion = ELinearConstraintMotion::Locked;
		NewConstraint->LinearZMotion = ELinearConstraintMotion::Locked;
		NewConstraint->LinearLimit = 0.0f;

		NewConstraint->Swing1Motion = EAngularConstraintMotion::Limited;
		NewConstraint->Swing2Motion = EAngularConstraintMotion::Limited;
		NewConstraint->TwistMotion = EAngularConstraintMotion::Limited;

		// 본 이름 기반 관절 제한값 적용
		FJointLimitPreset Limits = GetJointLimitsByBoneName(ChildBoneName.ToString());
		NewConstraint->Swing1LimitAngle = Limits.Swing1;
		NewConstraint->Swing2LimitAngle = Limits.Swing2;
		NewConstraint->TwistLimitAngle = Limits.Twist;

		// Soft Limit 설정 - Joint 안정성 향상
		NewConstraint->bSoftSwingLimit = true;
		NewConstraint->SwingStiffness = 50.0f;
		NewConstraint->SwingDamping = 5.0f;

		NewConstraint->bSoftTwistLimit = true;
		NewConstraint->TwistStiffness = 50.0f;
		NewConstraint->TwistDamping = 5.0f;

		// Joint 위치 설정
		// Body1 (Ancestor): Child 본 위치 (Ancestor 로컬 좌표계)
		// Body2 (Child): 원점 (본의 시작점)
		NewConstraint->ConstraintPositionInBody1 = ChildPosInAncestorLocal;
		NewConstraint->ConstraintPositionInBody2 = FVector(0, 0, 0);
		NewConstraint->ConstraintRotationInBody1 = FVector(0, 0, 0);
		NewConstraint->ConstraintRotationInBody2 = FVector(0, 0, 0);

		PhysicsAsset->AddConstraintSetup(NewConstraint);
		++ConstraintCount;
	}

	return ConstraintCount > 0;
}
