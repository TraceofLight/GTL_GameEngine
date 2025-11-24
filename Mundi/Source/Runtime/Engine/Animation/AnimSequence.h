#pragma once
#include "AnimSequenceBase.h"

/**
 * @brief 애니메이션 동기화 마커
 *
 * 특정 시점(예: 발 착지, 점프 정점)을 마킹하여
 * 서로 다른 애니메이션 간의 동기화를 가능하게 합니다.
 */
struct FAnimSyncMarker
{
	FString MarkerName;     // 마커 이름 (예: "LeftFootDown", "RightFootDown")
	float Time;             // 애니메이션 내 시간 (초)

	FAnimSyncMarker()
		: Time(0.0f)
	{
	}

	FAnimSyncMarker(const FString& InName, float InTime)
		: MarkerName(InName)
		, Time(InTime)
	{
	}
};

/**
 * UAnimSequence
 * 실제 애니메이션 시퀀스 클래스
 * FBX에서 임포트한 애니메이션 데이터를 저장합니다.
 */
class UAnimSequence : public UAnimSequenceBase
{
public:
	DECLARE_CLASS(UAnimSequence, UAnimSequenceBase)

	UAnimSequence();
	virtual ~UAnimSequence() override;

	// ResourceBase 인터페이스 구현
	void Load(const FString& InFilePath, ID3D11Device* InDevice);

	// .anim 파일에서 직접 로드 (Save와 대칭)
	void LoadFromAnimFile(const FString& AnimFilePath);

	// .anim 파일로 저장
	bool Save();
	bool SaveToFile(const FString& SavePath);

	// 특정 시간의 본 Transform 샘플링
	bool GetBoneTransformAtTime(const FString& BoneName, float Time, FVector& OutPosition, FQuat& OutRotation, FVector& OutScale) const;

	// 특정 프레임의 본 Transform 샘플링
	bool GetBoneTransformAtFrame(const FString& BoneName, int32 Frame, FVector& OutPosition, FQuat& OutRotation, FVector& OutScale) const;

	// Skeleton 정보 가져오기
	const FSkeleton* GetSkeleton() const { return DataModel ? DataModel->GetSkeleton() : nullptr; }

	// ===== Sync Markers =====

	/**
	 * @brief Sync Marker 추가
	 */
	void AddSyncMarker(const FString& MarkerName, float Time);

	/**
	 * @brief Sync Marker 제거
	 */
	void RemoveSyncMarker(int32 Index);

	/**
	 * @brief 모든 Sync Marker 가져오기
	 */
	const TArray<FAnimSyncMarker>& GetSyncMarkers() const { return SyncMarkers; }

	/**
	 * @brief 특정 이름의 Sync Marker 찾기
	 */
	const FAnimSyncMarker* FindSyncMarker(const FString& MarkerName) const;

	/**
	 * @brief 주어진 시간 범위 내의 Sync Marker 찾기
	 */
	void FindSyncMarkersInRange(float StartTime, float EndTime, TArray<FAnimSyncMarker>& OutMarkers) const;

private:
	// Sync Marker 목록
	TArray<FAnimSyncMarker> SyncMarkers;

	// 보간 헬퍼 함수
	FVector InterpolatePosition(const TArray<FVector>& Keys, float Alpha, int32 Frame0, int32 Frame1) const;
	FQuat InterpolateRotation(const TArray<FQuat>& Keys, float Alpha, int32 Frame0, int32 Frame1) const;
	FVector InterpolateScale(const TArray<FVector>& Keys, float Alpha, int32 Frame0, int32 Frame1) const;
};
