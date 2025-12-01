#include "pch.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsAsset.h"
#include "BodySetup.h"
#include "CapsuleElem.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"

static const float MinPrimSize = 0.01f;
static const float MinBoneWeightThreshold = 0.1f; // Minimum weight to consider a vertex belonging to a bone

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

    // Calculate orientation using covariance matrix and eigenvector
    // This finds the axis of maximum variance in the vertex cloud (the "long" direction)
    const FMatrix CovarianceMatrix = ComputeCovarianceMatrix(Info);
    FVector LongAxis = ComputeEigenVector(CovarianceMatrix);

    // Compute simple AABB first to get center and extents
    FVector BoxMin(FLT_MAX, FLT_MAX, FLT_MAX);
    FVector BoxMax(-FLT_MAX, -FLT_MAX, -FLT_MAX);

    for (int32 j = 0; j < Info.Positions.Num(); j++)
    {
        const FVector& Pos = Info.Positions[j];
        BoxMin.X = FMath::Min(BoxMin.X, Pos.X);
        BoxMin.Y = FMath::Min(BoxMin.Y, Pos.Y);
        BoxMin.Z = FMath::Min(BoxMin.Z, Pos.Z);
        BoxMax.X = FMath::Max(BoxMax.X, Pos.X);
        BoxMax.Y = FMath::Max(BoxMax.Y, Pos.Y);
        BoxMax.Z = FMath::Max(BoxMax.Z, Pos.Z);
    }

    FVector BoxCenter = (BoxMin + BoxMax) * 0.5f;
    FVector BoxExtent = (BoxMax - BoxMin) * 0.5f;

    // If the primitive is too small, use minimum size
    float MinRad = FMath::Min(FMath::Min(BoxExtent.X, BoxExtent.Y), BoxExtent.Z);
    if (MinRad < MinPrimSize)
    {
        BoxExtent = FVector(MinPrimSize, MinPrimSize, MinPrimSize);
    }

    // PhysX capsules are X-axis aligned by default
    // We need to compute a rotation that aligns the X-axis with the LongAxis (dominant eigenvector)
    // For Z-up left-handed system

    FKCapsuleElem Capsule;
    Capsule.Center = BoxCenter;

    // Compute rotation to align X-axis with LongAxis
    FVector DefaultAxis(1.0f, 0.0f, 0.0f); // PhysX capsule default axis

    if (LongAxis.SizeSquared() > 0.001f)
    {
        LongAxis = LongAxis.GetSafeNormal();

        // Compute rotation from X-axis to LongAxis
        float DotProduct = FVector::Dot(DefaultAxis, LongAxis);

        if (DotProduct > 0.9999f)
        {
            // Already aligned with X
            Capsule.Rotation = FVector(0, 0, 0);
        }
        else if (DotProduct < -0.9999f)
        {
            // Opposite direction - rotate 180 degrees around Z (or Y)
            Capsule.Rotation = FVector(0, 0, 180.0f);
        }
        else
        {
            // General case: compute rotation axis and angle
            // For left-handed system, cross product order matters
            FVector RotAxis = FVector::Cross(DefaultAxis, LongAxis).GetSafeNormal();
            float Angle = std::acos(FMath::Clamp(DotProduct, -1.0f, 1.0f));

            // Create quaternion from axis-angle and convert to Euler
            FQuat RotQuat = FQuat::FromAxisAngle(RotAxis, Angle);
            Capsule.Rotation = RotQuat.ToEulerZYXDeg();
        }
    }
    else
    {
        Capsule.Rotation = FVector(0, 0, 0);
    }

    // Compute radius and length based on extents projected onto LongAxis
    // The length is along LongAxis, radius is perpendicular
    float LengthExtent = FMath::Max(FMath::Max(BoxExtent.X, BoxExtent.Y), BoxExtent.Z);
    float RadiusExtent = FMath::Min(FMath::Min(BoxExtent.X, BoxExtent.Y), BoxExtent.Z);

    // Use middle value for radius if there's significant difference
    float MidExtent = BoxExtent.X + BoxExtent.Y + BoxExtent.Z - LengthExtent - RadiusExtent;
    RadiusExtent = FMath::Max(RadiusExtent, MidExtent) * 0.5f + RadiusExtent * 0.5f;

    Capsule.Radius = RadiusExtent * 1.01f;
    Capsule.Length = FMath::Max(LengthExtent * 2.0f - Capsule.Radius * 2.0f, 0.01f);

    BodySetup->AggGeom.SphylElems.Add(Capsule);

    UE_LOG("[PhysicsAssetUtils] Bone '%s': Capsule R=%.3f, L=%.3f, Center=(%.2f,%.2f,%.2f)",
           BoneName.c_str(), Capsule.Radius, Capsule.Length,
           Capsule.Center.X, Capsule.Center.Y, Capsule.Center.Z);

    return true;
}

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

    // Calculate vertex info for each bone from actual skinned vertex data
    // Note: Our vertices are in model space (bind pose)
    TArray<FBoneVertInfo> BoneVertInfos;
    CalcBoneVertInfos(SkeletalMesh, BoneVertInfos, true); // Use dominant weight

    // Create collision for each bone that has vertex data
    for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
    {
        const FBone& Bone = Bones[BoneIdx];
        const FBoneVertInfo& ModelSpaceInfo = BoneVertInfos[BoneIdx];

        // Skip bones with no vertex data
        if (ModelSpaceInfo.Positions.Num() == 0)
        {
            continue;
        }

        // Get bone position in model space (from bind pose)
        FVector BonePosition(Bone.BindPose.M[3][0], Bone.BindPose.M[3][1], Bone.BindPose.M[3][2]);

        // Transform vertices to be relative to bone position (bone-local space)
        // This is a simple translation - vertices stay in world orientation but centered on bone
        FBoneVertInfo BoneLocalInfo;
        BoneLocalInfo.Positions.SetNum(ModelSpaceInfo.Positions.Num());
        BoneLocalInfo.Normals = ModelSpaceInfo.Normals; // Normals don't need translation

        for (int32 i = 0; i < ModelSpaceInfo.Positions.Num(); ++i)
        {
            // Subtract bone position to get bone-relative position
            BoneLocalInfo.Positions[i] = ModelSpaceInfo.Positions[i] - BonePosition;
        }

        // Create BodySetup with capsule (heap allocated like UE)
        UBodySetup* NewSetup = NewObject<UBodySetup>();
        NewSetup->BoneName = FName(Bone.Name);
        NewSetup->CollisionTraceFlag = ECollisionTraceFlag::UseDefault;

        // Create collision geometry for this bone (now in bone-local space)
        if (CreateCollisionFromBoneInternal(NewSetup, BoneLocalInfo, Bone.Name))
        {
            // Add to physics asset
            PhysicsAsset->AddBodySetup(NewSetup);
        }
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
