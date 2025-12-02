#include "pch.h"
#include "PhysicsAssetUtils.h"
#include "PhysicsAsset.h"
#include "BodySetup.h"
#include "CapsuleElem.h"
#include "PhysicsConstraintSetup.h"
#include "Source/Runtime/AssetManagement/SkeletalMesh.h"

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
            continue;
        }

        // For very short hub bones, use a minimum length for capsule calculation
        if (BoneLength < 0.01f && bIsHubBone)
        {
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

        // Add to physics asset
        PhysicsAsset->AddBodySetup(NewSetup);
    }

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
