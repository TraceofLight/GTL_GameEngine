#pragma once
#include "AnimSequenceBase.h"

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

	// 특정 시간의 본 Transform 샘플링
	bool GetBoneTransformAtTime(const FString& BoneName, float Time, FVector& OutPosition, FQuat& OutRotation, FVector& OutScale) const;

	// 특정 프레임의 본 Transform 샘플링
	bool GetBoneTransformAtFrame(const FString& BoneName, int32 Frame, FVector& OutPosition, FQuat& OutRotation, FVector& OutScale) const;

	// Skeleton 정보 가져오기
	const FSkeleton* GetSkeleton() const { return DataModel ? DataModel->GetSkeleton() : nullptr; }

private:
	// 보간 헬퍼 함수
	FVector InterpolatePosition(const TArray<FVector>& Keys, float Alpha, int32 Frame0, int32 Frame1) const;
	FQuat InterpolateRotation(const TArray<FQuat>& Keys, float Alpha, int32 Frame0, int32 Frame1) const;
	FVector InterpolateScale(const TArray<FVector>& Keys, float Alpha, int32 Frame0, int32 Frame1) const;
};
