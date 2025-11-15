#pragma once
#include "Object.h"
#include "AnimationTypes.h"

struct FSkeleton;

/**
 * UAnimDataModel
 * 애니메이션의 실제 데이터를 저장하는 클래스
 * 본별 키프레임 데이터와 메타데이터를 포함합니다.
 */
class UAnimDataModel : public UObject
{
public:
	DECLARE_CLASS(UAnimDataModel, UObject)

	UAnimDataModel();
	virtual ~UAnimDataModel() override;

	// 애니메이션 데이터
	FSkeleton* Skeleton;                              // 이 애니메이션이 사용하는 스켈레톤
	TArray<FBoneAnimationTrack> BoneAnimationTracks; // 본별 애니메이션 트랙
	float PlayLength;                                 // 애니메이션 재생 시간 (초)
	FFrameRate FrameRate;                            // 프레임레이트
	int32 NumberOfFrames;                            // 총 프레임 수
	int32 NumberOfKeys;                              // 총 키프레임 수

	// Getter 메서드
	FSkeleton* GetSkeleton() const { return Skeleton; }
	const TArray<FBoneAnimationTrack>& GetBoneAnimationTracks() const { return BoneAnimationTracks; }
	float GetPlayLength() const { return PlayLength; }
	FFrameRate GetFrameRate() const { return FrameRate; }
	int32 GetNumberOfFrames() const { return NumberOfFrames; }
	int32 GetNumberOfKeys() const { return NumberOfKeys; }

	// 본 인덱스로 트랙 가져오기
	const FBoneAnimationTrack* GetBoneTrackByIndex(int32 BoneIndex) const
	{
		if (BoneIndex >= 0 && BoneIndex < BoneAnimationTracks.Num())
		{
			return &BoneAnimationTracks[BoneIndex];
		}
		return nullptr;
	}

	// 본 이름으로 트랙 가져오기
	const FBoneAnimationTrack* GetBoneTrackByName(const FString& BoneName) const
	{
		for (const FBoneAnimationTrack& Track : BoneAnimationTracks)
		{
			if (Track.BoneName == BoneName)
			{
				return &Track;
			}
		}
		return nullptr;
	}
};
