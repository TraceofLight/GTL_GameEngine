#include "pch.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsAsset.h"
#include "BodySetup.h"
#include "CapsuleElem.h"
#include "PhysicsConstraintSetup.h"
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

    // ===== 디버그: Skeleton 계층구조 출력 =====
    UE_LOG("[PhysicsAssetUtils] ===== SKELETON HIERARCHY DEBUG =====");
    for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
    {
        const FBone& Bone = Bones[BoneIdx];
        FString ParentName = (Bone.ParentIndex >= 0) ? Bones[Bone.ParentIndex].Name : "ROOT";
        UE_LOG("[PhysicsAssetUtils] Bone[%d] '%s' (Parent: '%s', Children: %d)",
               BoneIdx, Bone.Name.c_str(), ParentName.c_str(), (int)BoneChildren[BoneIdx].Num());
    }
    UE_LOG("[PhysicsAssetUtils] ===== END HIERARCHY =====");

    // Create capsule for each bone (except leaves with no children)
    for (int32 BoneIdx = 0; BoneIdx < NumBones; ++BoneIdx)
    {
        const FBone& Bone = Bones[BoneIdx];
        const TArray<int32>& Children = BoneChildren[BoneIdx];

        // Skip bones with no children (leaf bones) - they have no length
        if (Children.IsEmpty())
        {
            UE_LOG("[PhysicsAssetUtils] SKIP '%s': Leaf bone (no children)", Bone.Name.c_str());
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

        // Skip very short bones - BUT keep "hub" bones with multiple children (like Hips)
        // Hub bones are critical for connecting different body parts (spine, legs)
        const bool bIsHubBone = (Children.Num() >= 2);
        if (BoneLength < 0.01f && !bIsHubBone)
        {
            UE_LOG("[PhysicsAssetUtils] SKIP '%s': Too short (length=%.4f)", Bone.Name.c_str(), BoneLength);
            continue;
        }

        // For very short hub bones, use a minimum length for capsule calculation
        if (BoneLength < 0.01f && bIsHubBone)
        {
            UE_LOG("[PhysicsAssetUtils] HUB BONE '%s': Short but keeping (length=%.4f, children=%d)",
                   Bone.Name.c_str(), BoneLength, Children.Num());
            BoneLength = 0.05f;  // Minimum length for hub bones
            BoneVec = BoneVec.GetSafeNormal() * BoneLength;
        }

        // Heuristic radius: ~15% of bone length, with reasonable min/max
        float Radius = FMath::Clamp(BoneLength * 0.15f, 0.02f, 0.5f);

        // Capsule center is at midpoint of bone segment (in bone's local space)
        FVector LocalCenter = BoneVec * 0.5f;

        // Compute rotation to align capsule X-axis with bone direction
        // PhysX capsules are X-axis aligned by default
        FVector BoneDir = BoneVec.GetSafeNormal();

        // Compute Euler angles to rotate from X-axis to bone direction
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
               Bone.Name.c_str(), Radius, Capsule.Length,
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

bool FPhysicsAssetUtils::CreateConstraintsForRagdoll(UPhysicsAsset* PhysicsAsset, const USkeletalMesh* SkeletalMesh)
{
	if (!PhysicsAsset)
	{
		UE_LOG("[PhysicsAssetUtils] CreateConstraintsForRagdoll: PhysicsAsset is null");
		return false;
	}

	if (!SkeletalMesh || !SkeletalMesh->GetSkeleton())
	{
		UE_LOG("[PhysicsAssetUtils] CreateConstraintsForRagdoll: Invalid skeletal mesh or skeleton");
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

	// ===== 디버그: Body 목록 출력 =====
	UE_LOG("[PhysicsAssetUtils] ===== BODY LIST DEBUG =====");
	for (int32 i = 0; i < PhysicsAsset->BodySetups.Num(); ++i)
	{
		UE_LOG("[PhysicsAssetUtils] Body[%d]: '%s'",
			   i, PhysicsAsset->BodySetups[i]->BoneName.ToString().c_str());
	}
	UE_LOG("[PhysicsAssetUtils] ===== END BODY LIST =====");

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
			UE_LOG("[PhysicsAssetUtils] CONSTRAINT SKIP: '%s' has no ancestor with body (ROOT BODY)",
				   ChildBoneName.ToString().c_str());
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

		NewConstraint->Swing1LimitAngle = 45.0f;
		NewConstraint->Swing2LimitAngle = 45.0f;
		NewConstraint->TwistLimitAngle = 30.0f;

		// Joint 위치 설정
		// Body1 (Ancestor): Child 본 위치 (Ancestor 로컬 좌표계)
		// Body2 (Child): 원점 (본의 시작점)
		NewConstraint->ConstraintPositionInBody1 = ChildPosInAncestorLocal;
		NewConstraint->ConstraintPositionInBody2 = FVector(0, 0, 0);
		NewConstraint->ConstraintRotationInBody1 = FVector(0, 0, 0);
		NewConstraint->ConstraintRotationInBody2 = FVector(0, 0, 0);

		PhysicsAsset->AddConstraintSetup(NewConstraint);
		++ConstraintCount;

		UE_LOG("[PhysicsAssetUtils] Created constraint: '%s' -> '%s' (local offset: %.2f, %.2f, %.2f)",
			   AncestorBoneName.ToString().c_str(), ChildBoneName.ToString().c_str(),
			   ChildPosInAncestorLocal.X, ChildPosInAncestorLocal.Y, ChildPosInAncestorLocal.Z);
	}

	UE_LOG("[PhysicsAssetUtils] CreateConstraintsForRagdoll: Created %d constraints", ConstraintCount);

	return ConstraintCount > 0;
}
