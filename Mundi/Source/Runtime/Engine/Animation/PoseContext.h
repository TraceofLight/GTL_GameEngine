#pragma once



struct FSkeleton;

/**
 * @brief 애니메이션 포즈 데이터 컨테이너
 *
 * 특정 시점의 모든 본의 로컬 트랜스폼 스냅샷을 담는 구조체.
 * 애니메이션 블렌딩의 입력/출력으로 사용됨.
 */
struct FPoseContext
{
	/** 모든 본의 로컬 스페이스 트랜스폼 배열 (부모 기준 상대 변환) */
	TArray<FTransform> LocalSpacePose;

	/** 이 포즈가 속한 스켈레톤 (호환성 체크용) */
	const FSkeleton* Skeleton;

	// ===== Phase 1.5: ComponentSpace 지원 =====

	/** 모든 본의 컴포넌트 스페이스 트랜스폼 배열 (컴포넌트 루트 기준 절대 변환) */
	TArray<FTransform> ComponentSpacePose;

	/** ComponentSpacePose가 최신 상태인지 여부 */
	bool bComponentSpaceValid;

	/**
	 * @brief 기본 생성자
	 */
	FPoseContext()
		: Skeleton(nullptr)
		, bComponentSpaceValid(false)
	{
	}

	/**
	 * @brief 스켈레톤으로 포즈 초기화
	 * @param InSkeleton 대상 스켈레톤
	 */
	void Initialize(const FSkeleton* InSkeleton);

	/**
	 * @brief T-Pose(바인드 포즈)로 리셋
	 */
	void ResetToRefPose();

	/**
	 * @brief 다른 포즈 데이터를 복사
	 * @param SourcePose 복사할 원본 포즈
	 */
	void CopyPose(const FPoseContext& SourcePose);

	/**
	 * @brief 포즈가 유효한지 체크
	 * @return 스켈레톤이 설정되고 본 데이터가 있으면 true
	 */
	bool IsValid() const
	{
		return Skeleton != nullptr && LocalSpacePose.Num() > 0;
	}

	/**
	 * @brief 본 개수 반환
	 */
	int32 GetNumBones() const
	{
		return LocalSpacePose.Num();
	}

	// ===== Phase 1.5: ComponentSpace 변환 메서드 =====

	/**
	 * @brief LocalSpace → ComponentSpace 변환
	 */
	void ConvertToComponentSpace();

	/**
	 * @brief ComponentSpace → LocalSpace 역변환
	 */
	void ConvertToLocalSpace();

	/**
	 * @brief ComponentSpace 유효성 보장 (필요시 자동 변환)
	 */
	void EnsureComponentSpaceValid()
	{
		if (!bComponentSpaceValid)
			ConvertToComponentSpace();
	}

	/**
	 * @brief ComponentSpace 무효화 (LocalSpacePose 변경 시 호출)
	 */
	void InvalidateComponentSpace()
	{
		bComponentSpaceValid = false;
	}
};

/**
 * @brief 애니메이션 추출 컨텍스트
 *
 * 애니메이션 시퀀스에서 포즈를 추출할 때 필요한 파라미터들을 담는 구조체.
 */
struct FAnimExtractContext
{
	/** 추출할 애니메이션 시간 (초 단위) */
	float CurrentTime;

	/** 루트 모션 추출 여부 (향후 확장용) */
	bool bExtractRootMotion;

	/**
	 * @brief 생성자
	 * @param InTime 추출할 시간
	 * @param bInExtractRootMotion 루트 모션 추출 여부
	 */
	FAnimExtractContext(float InTime = 0.0f, bool bInExtractRootMotion = false)
		: CurrentTime(InTime)
		, bExtractRootMotion(bInExtractRootMotion)
	{
	}
};
