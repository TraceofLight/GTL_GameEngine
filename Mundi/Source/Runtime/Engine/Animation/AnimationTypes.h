#pragma once
#include "Vector.h"
#include "UEContainer.h"
#include "Archive.h"
#include "Name.h"

/**
 * @brief Animation Notify 이벤트
 * @details 특정 시간에 트리거되는 Animation 이벤트 (예: 발소리, 파티클 이펙트)
 *
 * @param TriggerTime 트리거 시간 (초 단위, Animation 시작 기준)
 * @param Duration 지속 시간 (초 단위, State Notify용)
 * @param NotifyName Notify 이름
 * @param TriggerWeightThreshold Notify 가중치 임계값 (블렌딩 시 이 값 이상일 때만 트리거)
 */
struct FAnimNotifyEvent
{
	float TriggerTime;
	float Duration;
	FName NotifyName;
	float TriggerWeightThreshold;
	int32 TrackIndex;  // Notify가 속한 Track 인덱스 (0부터 시작)

	FAnimNotifyEvent()
		: TriggerTime(0.0f)
		, Duration(0.0f)
		, TriggerWeightThreshold(0.0f)
		, TrackIndex(0)
	{
	}

	FAnimNotifyEvent(float InTriggerTime, FName InNotifyName, float InDuration = 0.0f, int32 InTrackIndex = 0)
		: TriggerTime(InTriggerTime)
		, Duration(InDuration)
		, NotifyName(InNotifyName)
		, TriggerWeightThreshold(0.0f)
		, TrackIndex(InTrackIndex)
	{
	}

	// Operators
	bool operator<(const FAnimNotifyEvent& Other) const
	{
		return TriggerTime < Other.TriggerTime;
	}

	bool operator==(const FAnimNotifyEvent& Other) const
	{
		return TriggerTime == Other.TriggerTime && NotifyName == Other.NotifyName;
	}
};

/**
 * 프레임레이트 정보
 * 애니메이션의 재생 속도를 정의합니다.
 */
struct FFrameRate
{
	int32 Numerator;   // 분자 (예: 30)
	int32 Denominator; // 분모 (예: 1) → 30 FPS

	FFrameRate()
		: Numerator(30), Denominator(1)
	{
	}

	FFrameRate(int32 InNumerator, int32 InDenominator)
		: Numerator(InNumerator), Denominator(InDenominator)
	{
	}

	float AsDecimal() const
	{
		return Denominator > 0 ? static_cast<float>(Numerator) / static_cast<float>(Denominator) : 0.0f;
	}

	friend FArchive& operator<<(FArchive& Ar, FFrameRate& FrameRate)
	{
		Ar << FrameRate.Numerator;
		Ar << FrameRate.Denominator;
		return Ar;
	}
};

/**
 * Raw 애니메이션 시퀀스 트랙
 * 본 하나에 대한 위치, 회전, 스케일 키프레임을 저장합니다.
 */
struct FRawAnimSequenceTrack
{
	TArray<FVector> PosKeys;   // 위치 키프레임
	TArray<FQuat>   RotKeys;   // 회전 키프레임 (Quaternion)
	TArray<FVector> ScaleKeys; // 스케일 키프레임

	FRawAnimSequenceTrack()
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FRawAnimSequenceTrack& Track)
	{
		if (Ar.IsSaving())
		{
			// PosKeys 저장
			uint32 PosKeyCount = static_cast<uint32>(Track.PosKeys.Num());
			Ar << PosKeyCount;
			for (FVector& Key : Track.PosKeys)
			{
				Ar << Key;
			}

			// RotKeys 저장
			uint32 RotKeyCount = static_cast<uint32>(Track.RotKeys.Num());
			Ar << RotKeyCount;
			for (FQuat& Key : Track.RotKeys)
			{
				Ar << Key;
			}

			// ScaleKeys 저장
			uint32 ScaleKeyCount = static_cast<uint32>(Track.ScaleKeys.Num());
			Ar << ScaleKeyCount;
			for (FVector& Key : Track.ScaleKeys)
			{
				Ar << Key;
			}
		}
		else if (Ar.IsLoading())
		{
			// PosKeys 로드
			uint32 PosKeyCount = 0;
			Ar << PosKeyCount;
			Track.PosKeys.resize(PosKeyCount);
			for (uint32 i = 0; i < PosKeyCount; ++i)
			{
				Ar << Track.PosKeys[i];
			}

			// RotKeys 로드
			uint32 RotKeyCount = 0;
			Ar << RotKeyCount;
			Track.RotKeys.resize(RotKeyCount);
			for (uint32 i = 0; i < RotKeyCount; ++i)
			{
				Ar << Track.RotKeys[i];
			}

			// ScaleKeys 로드
			uint32 ScaleKeyCount = 0;
			Ar << ScaleKeyCount;
			Track.ScaleKeys.resize(ScaleKeyCount);
			for (uint32 i = 0; i < ScaleKeyCount; ++i)
			{
				Ar << Track.ScaleKeys[i];
			}
		}
		return Ar;
	}
};

/**
 * 본 애니메이션 트랙
 * 본 이름과 해당 본의 애니메이션 데이터를 저장합니다.
 */
struct FBoneAnimationTrack
{
	FString BoneName;                      // 본 이름
	FRawAnimSequenceTrack InternalTrack;   // 실제 키프레임 데이터

	FBoneAnimationTrack()
	{
	}

	FBoneAnimationTrack(const FString& InBoneName)
		: BoneName(InBoneName)
	{
	}

	friend FArchive& operator<<(FArchive& Ar, FBoneAnimationTrack& Track)
	{
		if (Ar.IsSaving())
		{
			Serialization::WriteString(Ar, Track.BoneName);
			Ar << Track.InternalTrack;
		}
		else if (Ar.IsLoading())
		{
			Serialization::ReadString(Ar, Track.BoneName);
			Ar << Track.InternalTrack;
		}
		return Ar;
	}
};
