#pragma once

#include "PoseContext.h"

/**
 * @brief 애니메이션 블렌딩 유틸리티 클래스
 *
 * 정적 함수들로 구성된 유틸리티 클래스.
 * 여러 애니메이션 포즈를 블렌딩하는 핵심 기능을 제공.
 */
class FAnimationRuntime
{
public:
	/**
	 * @brief 두 포즈를 선형 보간(LERP/SLERP)으로 블렌딩
	 *
	 * Position과 Scale은 선형 보간(LERP), Rotation은 구면 선형 보간(SLERP) 사용.
	 * BlendAlpha = 0일 때 PoseA, BlendAlpha = 1일 때 PoseB.
	 *
	 * @param PoseA 첫 번째 포즈 (BlendAlpha=0 일 때 결과)
	 * @param PoseB 두 번째 포즈 (BlendAlpha=1 일 때 결과)
	 * @param BlendAlpha 블렌딩 가중치 (0.0 ~ 1.0)
	 * @param OutPose 출력 포즈 (블렌딩 결과)
	 */
	static void BlendTwoPosesTogether(
		const FPoseContext& PoseA,
		const FPoseContext& PoseB,
		float BlendAlpha,
		FPoseContext& OutPose);

	/**
	 * @brief 여러 포즈를 가중치 배열로 블렌딩
	 *
	 * 여러 애니메이션을 동시에 블렌딩할 때 사용.
	 * 예: Blend Space (2D/3D 파라미터 기반 블렌딩)
	 *
	 * @param SourcePoses 입력 포즈 배열
	 * @param BlendWeights 각 포즈의 가중치 배열 (합이 1.0이어야 함)
	 * @param OutPose 출력 포즈 (블렌딩 결과)
	 */
	static void BlendPosesTogetherPerBone(
		const TArray<FPoseContext>& SourcePoses,
		const TArray<float>& BlendWeights,
		FPoseContext& OutPose);

	/**
	 * @brief 단일 본의 트랜스폼을 블렌딩
	 *
	 * 내부 헬퍼 함수. 두 트랜스폼을 블렌딩.
	 *
	 * @param TransformA 첫 번째 트랜스폼
	 * @param TransformB 두 번째 트랜스폼
	 * @param BlendAlpha 블렌딩 가중치 (0.0 ~ 1.0)
	 * @return 블렌딩된 트랜스폼
	 */
	static FTransform BlendTransform(
		const FTransform& TransformA,
		const FTransform& TransformB,
		float BlendAlpha);

	// ===== Phase 1.5: ComponentSpace 변환 유틸리티 =====

	/**
	 * @brief LocalSpace 포즈를 ComponentSpace로 변환
	 *
	 * LocalSpace: 부모 본 기준 상대 변환
	 * ComponentSpace: 컴포넌트(루트) 기준 절대 변환
	 * IK, LayeredBlend 등에서 필수적으로 사용.
	 *
	 * @param Skeleton 스켈레톤 (본 계층 정보)
	 * @param LocalPose 로컬 스페이스 포즈
	 * @param OutComponentPose 출력 컴포넌트 스페이스 포즈
	 */
	static void ConvertLocalPoseToComponentSpace(
		const FSkeleton* Skeleton,
		const TArray<FTransform>& LocalPose,
		TArray<FTransform>& OutComponentPose);

	/**
	 * @brief ComponentSpace 포즈를 LocalSpace로 역변환
	 *
	 * IK 작업 후 다시 LocalSpace로 변환할 때 사용.
	 *
	 * @param Skeleton 스켈레톤 (본 계층 정보)
	 * @param ComponentPose 컴포넌트 스페이스 포즈
	 * @param OutLocalPose 출력 로컬 스페이스 포즈
	 */
	static void ConvertComponentPoseToLocalSpace(
		const FSkeleton* Skeleton,
		const TArray<FTransform>& ComponentPose,
		TArray<FTransform>& OutLocalPose);
};
