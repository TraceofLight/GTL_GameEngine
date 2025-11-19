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

	// Skeleton 설정 (복사본 생성)
	void SetSkeleton(const FSkeleton& InSkeleton);

private:
	FSkeleton* Skeleton;                              // 이 애니메이션이 소유하는 스켈레톤 (복사본)

public:
	// 애니메이션 데이터
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

	// Serialization
	friend FArchive& operator<<(FArchive& Ar, UAnimDataModel& Model)
	{
		if (Ar.IsSaving())
		{
			// BoneAnimationTracks 배열 저장
			uint32 TrackCount = static_cast<uint32>(Model.BoneAnimationTracks.Num());
			Ar << TrackCount;
			for (FBoneAnimationTrack& Track : Model.BoneAnimationTracks)
			{
				Ar << Track;
			}

			// 메타데이터 저장
			Ar << Model.PlayLength;
			Ar << Model.FrameRate;
			Ar << Model.NumberOfFrames;
			Ar << Model.NumberOfKeys;
		}
		else if (Ar.IsLoading())
		{
			// BoneAnimationTracks 배열 로드
			uint32 TrackCount = 0;
			Ar << TrackCount;
			Model.BoneAnimationTracks.resize(TrackCount);
			for (uint32 i = 0; i < TrackCount; ++i)
			{
				Ar << Model.BoneAnimationTracks[i];
			}

			// 메타데이터 로드
			Ar << Model.PlayLength;
			Ar << Model.FrameRate;
			Ar << Model.NumberOfFrames;
			Ar << Model.NumberOfKeys;
		}
		return Ar;
	}
};
