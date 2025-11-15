#include "pch.h"
#include "AnimationRuntime.h"
#include "Source/Runtime/Core/Misc/VertexData.h"

// ===== Phase 1.5: ComponentSpace 변환 함수 구현 =====

/**
 * @brief 두 포즈를 선형 보간으로 블렌딩
 */
void FAnimationRuntime::BlendTwoPosesTogether(
	const FPoseContext& PoseA,
	const FPoseContext& PoseB,
	float BlendAlpha,
	FPoseContext& OutPose)
{
	// 유효성 검사
	if (!PoseA.IsValid() || !PoseB.IsValid())
	{
		// 유효하지 않은 포즈가 있으면 중단
		return;
	}

	// 스켈레톤 호환성 체크
	if (PoseA.Skeleton != PoseB.Skeleton)
	{
		// 다른 스켈레톤의 포즈는 블렌딩 불가
		return;
	}

	// 본 개수가 다르면 중단
	const int32 NumBonesA = PoseA.GetNumBones();
	const int32 NumBonesB = PoseB.GetNumBones();
	if (NumBonesA != NumBonesB)
	{
		return;
	}

	// BlendAlpha를 0~1 범위로 클램핑
	BlendAlpha = FMath::Clamp(BlendAlpha, 0.0f, 1.0f);

	// 출력 포즈 초기화
	OutPose.Initialize(PoseA.Skeleton);
	OutPose.LocalSpacePose.SetNum(NumBonesA);

	// 각 본마다 블렌딩 수행
	for (int32 BoneIndex = 0; BoneIndex < NumBonesA; ++BoneIndex)
	{
		const FTransform& TransformA = PoseA.LocalSpacePose[BoneIndex];
		const FTransform& TransformB = PoseB.LocalSpacePose[BoneIndex];

		// 블렌딩된 트랜스폼 계산
		OutPose.LocalSpacePose[BoneIndex] = BlendTransform(TransformA, TransformB, BlendAlpha);
	}
}

/**
 * @brief 여러 포즈를 가중치 배열로 블렌딩
 */
void FAnimationRuntime::BlendPosesTogetherPerBone(
	const TArray<FPoseContext>& SourcePoses,
	const TArray<float>& BlendWeights,
	FPoseContext& OutPose)
{
	// 입력 검증
	if (SourcePoses.Num() == 0 || SourcePoses.Num() != BlendWeights.Num())
	{
		return;
	}

	// 첫 번째 포즈로 출력 초기화
	if (!SourcePoses[0].IsValid())
	{
		return;
	}

	const FSkeleton* TargetSkeleton = SourcePoses[0].Skeleton;
	const int32 NumBones = SourcePoses[0].GetNumBones();

	OutPose.Initialize(TargetSkeleton);
	OutPose.LocalSpacePose.SetNum(NumBones);

	// 모든 본을 Identity로 초기화
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		OutPose.LocalSpacePose[BoneIndex] = FTransform();
	}

	// 가중치 합 계산
	float TotalWeight = 0.0f;
	for (float Weight : BlendWeights)
	{
		TotalWeight += Weight;
	}

	// 가중치 합이 0이면 중단
	if (TotalWeight < 0.0001f)
	{
		return;
	}

	// 각 본마다 가중치 블렌딩 수행
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		FVector BlendedPosition = FVector::Zero();
		FQuat BlendedRotation = FQuat::Identity();
		FVector BlendedScale = FVector::Zero();

		// 모든 포즈에 대해 가중치 적용
		for (int32 PoseIndex = 0; PoseIndex < SourcePoses.Num(); ++PoseIndex)
		{
			const FPoseContext& SourcePose = SourcePoses[PoseIndex];
			if (!SourcePose.IsValid() || SourcePose.GetNumBones() != NumBones)
			{
				continue;
			}

			const FTransform& SourceTransform = SourcePose.LocalSpacePose[BoneIndex];
			const float NormalizedWeight = BlendWeights[PoseIndex] / TotalWeight;

			// Position: 가중 평균
			BlendedPosition += SourceTransform.Translation * NormalizedWeight;

			// Rotation: 첫 번째 쿼터니언을 기준으로 SLERP
			if (PoseIndex == 0)
			{
				BlendedRotation = SourceTransform.Rotation;
			}
			else
			{
				// 누적 SLERP (이전 결과와 현재 쿼터니언을 블렌딩)
				BlendedRotation = FQuat::Slerp(BlendedRotation, SourceTransform.Rotation, NormalizedWeight);
			}

			// Scale: 가중 평균
			BlendedScale += SourceTransform.Scale3D * NormalizedWeight;
		}

		// 정규화된 쿼터니언으로 설정
		BlendedRotation.Normalize();

		// 블렌딩 결과 저장
		OutPose.LocalSpacePose[BoneIndex] = FTransform(
			BlendedPosition,
			BlendedRotation,
			BlendedScale
		);
	}
}

/**
 * @brief 단일 본의 트랜스폼을 블렌딩
 */
FTransform FAnimationRuntime::BlendTransform(
	const FTransform& TransformA,
	const FTransform& TransformB,
	float BlendAlpha)
{
	// BlendAlpha 클램핑
	BlendAlpha = FMath::Clamp(BlendAlpha, 0.0f, 1.0f);

	// Position: 선형 보간 (LERP)
	FVector BlendedPosition = FMath::Lerp(
		TransformA.Translation,
		TransformB.Translation,
		BlendAlpha
	);

	// Rotation: 구면 선형 보간 (SLERP)
	// SLERP는 회전을 자연스럽게 보간하기 위한 필수 기법
	FQuat BlendedRotation = FQuat::Slerp(
		TransformA.Rotation,
		TransformB.Rotation,
		BlendAlpha
	);

	// 쿼터니언 정규화 (수치 오류 방지)
	BlendedRotation.Normalize();

	// Scale: 선형 보간 (LERP)
	FVector BlendedScale = FMath::Lerp(
		TransformA.Scale3D,
		TransformB.Scale3D,
		BlendAlpha
	);

	// 블렌딩된 트랜스폼 반환
	return FTransform(
		BlendedPosition,
		BlendedRotation,
		BlendedScale
	);
}

// ===== Phase 1.5: ComponentSpace 변환 함수 =====

/**
 * @brief LocalSpace 포즈를 ComponentSpace로 변환
 */
void FAnimationRuntime::ConvertLocalPoseToComponentSpace(
	const FSkeleton* Skeleton,
	const TArray<FTransform>& LocalPose,
	TArray<FTransform>& OutComponentPose)
{
	if (!Skeleton || LocalPose.Num() == 0)
	{
		return;
	}

	const int32 NumBones = LocalPose.Num();
	OutComponentPose.SetNum(NumBones);

	// 캐시된 ParentIndices 사용 (Phase 1.5 최적화)
	const TArray<int32>& ParentIndices = Skeleton->bCacheInitialized
		? Skeleton->ParentIndices
		: TArray<int32>();

	// 각 본을 순회하며 ComponentSpace 계산
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FTransform& LocalTransform = LocalPose[BoneIndex];

		// 부모 인덱스 가져오기 (캐시 우선)
		int32 ParentIndex = -1;
		if (Skeleton->bCacheInitialized && ParentIndices.Num() > BoneIndex)
		{
			ParentIndex = ParentIndices[BoneIndex];
		}
		else if (BoneIndex < Skeleton->Bones.Num())
		{
			ParentIndex = Skeleton->Bones[BoneIndex].ParentIndex;
		}

		if (ParentIndex == -1) // 루트 본
		{
			// 루트 본은 LocalSpace = ComponentSpace
			OutComponentPose[BoneIndex] = LocalTransform;
		}
		else // 자식 본
		{
			// ComponentSpace = ParentComponentSpace * LocalSpace
			const FTransform& ParentComponentTransform = OutComponentPose[ParentIndex];
			OutComponentPose[BoneIndex] = ParentComponentTransform.GetWorldTransform(LocalTransform);
		}
	}
}

/**
 * @brief ComponentSpace 포즈를 LocalSpace로 역변환
 */
void FAnimationRuntime::ConvertComponentPoseToLocalSpace(
	const FSkeleton* Skeleton,
	const TArray<FTransform>& ComponentPose,
	TArray<FTransform>& OutLocalPose)
{
	if (!Skeleton || ComponentPose.Num() == 0)
	{
		return;
	}

	const int32 NumBones = ComponentPose.Num();
	OutLocalPose.SetNum(NumBones);

	// 캐시된 ParentIndices 사용
	const TArray<int32>& ParentIndices = Skeleton->bCacheInitialized
		? Skeleton->ParentIndices
		: TArray<int32>();

	// 각 본을 순회하며 LocalSpace 계산
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		const FTransform& ComponentTransform = ComponentPose[BoneIndex];

		// 부모 인덱스 가져오기
		int32 ParentIndex = -1;
		if (Skeleton->bCacheInitialized && ParentIndices.Num() > BoneIndex)
		{
			ParentIndex = ParentIndices[BoneIndex];
		}
		else if (BoneIndex < Skeleton->Bones.Num())
		{
			ParentIndex = Skeleton->Bones[BoneIndex].ParentIndex;
		}

		if (ParentIndex == -1) // 루트 본
		{
			// 루트 본은 LocalSpace = ComponentSpace
			OutLocalPose[BoneIndex] = ComponentTransform;
		}
		else // 자식 본
		{
			// LocalSpace = ComponentSpace * ParentComponentSpace^-1
			const FTransform& ParentComponentTransform = ComponentPose[ParentIndex];
			OutLocalPose[BoneIndex] = ParentComponentTransform.GetRelativeTransform(ComponentTransform);
		}
	}
}
