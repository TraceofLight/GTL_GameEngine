#pragma once

#include "Source/Runtime/Core/Misc/VertexData.h"
#include "Source/Runtime/Core/Containers/UEContainer.h"

/**
 * @brief Mixamo 스켈레톤 Chain Mapping 유틸리티
 *
 * Mixamo 계열 스켈레톤(mixamorig:, mixamorig9:, mixamorig12:, 접두사 없음 등)을
 * 표준 Canonical Skeleton으로 통일하기 위한 매핑 시스템
 */
class FMixamoChainMapper
{
public:
	/**
	 * @brief Canonical(표준) Mixamo 본 이름 목록
	 * X Bot 기준으로 정의된 표준 본 이름들
	 */
	static const TArray<FString>& GetCanonicalBoneNames();

	/**
	 * @brief 스켈레톤이 Mixamo 계열인지 감지
	 * @param Skeleton 검사할 스켈레톤
	 * @return Mixamo 계열이면 true
	 */
	static bool IsMixamoSkeleton(const FSkeleton& Skeleton);

	/**
	 * @brief 본 이름에서 접두사 제거 (mixamorig:, mixamorig9: 등)
	 * @param BoneName 원본 본 이름
	 * @return 접두사가 제거된 Canonical 이름
	 */
	static FString RemovePrefix(const FString& BoneName);

	/**
	 * @brief Source 스켈레톤을 Canonical 스켈레톤으로 매핑
	 * @param SourceSkeleton 원본 스켈레톤 (Mixamo 변종)
	 * @param OutCanonicalToSource Canonical Index -> Source Bone Index 매핑 (출력)
	 * @param OutSourceToCanonical Source Bone Index -> Canonical Index 매핑 (출력)
	 * @return 매핑 성공 여부
	 */
	static bool CreateMapping(
		const FSkeleton& SourceSkeleton,
		TArray<int32>& OutCanonicalToSource,
		TArray<int32>& OutSourceToCanonical
	);

	/**
	 * @brief Source 스켈레톤을 Canonical 스켈레톤으로 변환
	 * @param SourceSkeleton 원본 스켈레톤 (Mixamo 변종)
	 * @return Canonical 스켈레톤
	 */
	static FSkeleton ConvertToCanonicalSkeleton(const FSkeleton& SourceSkeleton);

	/**
	 * @brief Canonical Index가 유효한지 확인
	 * @param CanonicalIndex 확인할 인덱스
	 * @return 유효하면 true
	 */
	static bool IsValidCanonicalIndex(int32 CanonicalIndex);

	/**
	 * @brief Canonical 본 이름으로 인덱스 찾기
	 * @param CanonicalName 찾을 본 이름
	 * @return 인덱스 (없으면 INDEX_NONE)
	 */
	static int32 FindCanonicalBoneIndex(const FString& CanonicalName);

	/**
	 * @brief 표준 Canonical Mixamo 스켈레톤 생성
	 * @return Identity Transform을 가진 표준 스켈레톤
	 */
	static FSkeleton* CreateCanonicalSkeleton();

private:
	/** @brief Canonical 본 이름 목록 (Lazy Initialization) */
	static TArray<FString> CanonicalBoneNames;

	/** @brief Canonical 본 이름 -> 인덱스 맵 (빠른 검색용) */
	static TMap<FString, int32> CanonicalBoneIndexMap;

	/** @brief 초기화 플래그 */
	static bool bIsInitialized;

	/** @brief Canonical 본 이름 목록 초기화 */
	static void InitializeCanonicalBones();

	/**
	 * @brief 누락된 본의 부모 인덱스 찾기 (손가락/발가락 끝 처리)
	 * @param CanonicalIdx 누락된 본의 Canonical 인덱스
	 * @return 부모 Canonical 인덱스 (없으면 INDEX_NONE)
	 */
	static int32 FindParentForMissingBone(int32 CanonicalIdx);
};
