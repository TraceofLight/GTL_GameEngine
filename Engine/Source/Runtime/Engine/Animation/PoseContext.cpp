#include "pch.h"
#include "PoseContext.h"
#include "AnimationRuntime.h"
#include "Source/Runtime/Core/Misc/VertexData.h"

/**
 * @brief 스켈레톤으로 포즈 초기화
 */
void FPoseContext::Initialize(const FSkeleton* InSkeleton)
{
	if (!InSkeleton)
	{
		return;
	}

	Skeleton = InSkeleton;
	const int32 NumBones = Skeleton->Bones.Num();
	LocalSpacePose.SetNum(NumBones);

	// 모든 본을 Identity Transform으로 초기화
	for (int32 i = 0; i < NumBones; ++i)
	{
		LocalSpacePose[i] = FTransform();
	}
}

/**
 * @brief T-Pose(바인드 포즈)로 리셋
 *
 * Phase 1.5: 캐시된 RefLocalPose를 사용하여 성능 최적화
 */
void FPoseContext::ResetToRefPose()
{
	if (!Skeleton)
	{
		return;
	}

	// Phase 1.5: 캐시된 RefPose 사용 (빠름!)
	if (Skeleton->bCacheInitialized && Skeleton->RefLocalPose.Num() > 0)
	{
		LocalSpacePose = Skeleton->RefLocalPose;
		InvalidateComponentSpace();
		return;
	}

	// 캐시가 없으면 기존 방식으로 계산 (느림)
	const int32 NumBones = Skeleton->Bones.Num();

	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FBone& ThisBone = Skeleton->Bones[BoneIndex];
		const int32 ParentIndex = ThisBone.ParentIndex;

		FMatrix LocalBindMatrix;

		if (ParentIndex == -1) // 루트 본
		{
			LocalBindMatrix = ThisBone.BindPose;
		}
		else // 자식 본
		{
			const FMatrix& ParentInverseBindPose = Skeleton->Bones[ParentIndex].InverseBindPose;
			LocalBindMatrix = ThisBone.BindPose * ParentInverseBindPose;
		}

		// 계산된 로컬 행렬을 FTransform으로 변환
		LocalSpacePose[BoneIndex] = FTransform(LocalBindMatrix);
	}

	InvalidateComponentSpace();
}

/**
 * @brief 다른 포즈 데이터를 복사
 */
void FPoseContext::CopyPose(const FPoseContext& SourcePose)
{
	if (!SourcePose.IsValid())
	{
		return;
	}

	Skeleton = SourcePose.Skeleton;
	LocalSpacePose = SourcePose.LocalSpacePose;

	// ComponentSpace도 복사 (Phase 1.5)
	ComponentSpacePose = SourcePose.ComponentSpacePose;
	bComponentSpaceValid = SourcePose.bComponentSpaceValid;
}

// ===== Phase 1.5: ComponentSpace 변환 메서드 구현 =====

/**
 * @brief LocalSpace → ComponentSpace 변환
 */
void FPoseContext::ConvertToComponentSpace()
{
	if (!IsValid())
	{
		return;
	}

	// FAnimationRuntime 유틸리티 함수 사용
	FAnimationRuntime::ConvertLocalPoseToComponentSpace(
		Skeleton,
		LocalSpacePose,
		ComponentSpacePose
	);

	bComponentSpaceValid = true;
}

/**
 * @brief ComponentSpace → LocalSpace 역변환
 */
void FPoseContext::ConvertToLocalSpace()
{
	if (!IsValid() || !bComponentSpaceValid)
	{
		return;
	}

	// FAnimationRuntime 유틸리티 함수 사용
	FAnimationRuntime::ConvertComponentPoseToLocalSpace(
		Skeleton,
		ComponentSpacePose,
		LocalSpacePose
	);
}
