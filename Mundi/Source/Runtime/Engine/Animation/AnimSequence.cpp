#include "pch.h"
#include "AnimSequence.h"
#include "AnimDataModel.h"

IMPLEMENT_CLASS(UAnimSequence)

UAnimSequence::UAnimSequence()
{
}

UAnimSequence::~UAnimSequence()
{
	// DataModel 삭제 (FBX 임포트 시 생성한 DataModel)
	if (DataModel)
	{
		ObjectFactory::DeleteObject(DataModel);
		DataModel = nullptr;
	}
}

bool UAnimSequence::GetBoneTransformAtTime(const FString& BoneName, float Time, FVector& OutPosition, FQuat& OutRotation, FVector& OutScale) const
{
	if (!DataModel)
	{
		return false;
	}

	// 본 트랙 찾기
	const FBoneAnimationTrack* Track = DataModel->GetBoneTrackByName(BoneName);
	if (!Track)
	{
		return false;
	}

	// 시간을 프레임으로 변환
	float FrameRate = DataModel->GetFrameRate().AsDecimal();
	float FrameFloat = Time * FrameRate;
	int32 Frame0 = static_cast<int32>(FrameFloat);
	int32 Frame1 = Frame0 + 1;
	float Alpha = FrameFloat - static_cast<float>(Frame0);

	// 프레임 범위 체크
	int32 MaxFrame = DataModel->GetNumberOfFrames() - 1;
	Frame0 = FMath::Clamp(Frame0, 0, MaxFrame);
	Frame1 = FMath::Clamp(Frame1, 0, MaxFrame);

	// 보간
	OutPosition = InterpolatePosition(Track->InternalTrack.PosKeys, Alpha, Frame0, Frame1);
	OutRotation = InterpolateRotation(Track->InternalTrack.RotKeys, Alpha, Frame0, Frame1);
	OutScale = InterpolateScale(Track->InternalTrack.ScaleKeys, Alpha, Frame0, Frame1);

	return true;
}

bool UAnimSequence::GetBoneTransformAtFrame(const FString& BoneName, int32 Frame, FVector& OutPosition, FQuat& OutRotation, FVector& OutScale) const
{
	if (!DataModel)
	{
		return false;
	}

	// 본 트랙 찾기
	const FBoneAnimationTrack* Track = DataModel->GetBoneTrackByName(BoneName);
	if (!Track)
	{
		return false;
	}

	// 프레임 범위 체크
	int32 MaxFrame = DataModel->GetNumberOfFrames() - 1;
	Frame = FMath::Clamp(Frame, 0, MaxFrame);

	// 키프레임 데이터 가져오기
	if (Frame < Track->InternalTrack.PosKeys.Num())
	{
		OutPosition = Track->InternalTrack.PosKeys[Frame];
	}
	else
	{
		OutPosition = FVector::Zero();
	}

	if (Frame < Track->InternalTrack.RotKeys.Num())
	{
		OutRotation = Track->InternalTrack.RotKeys[Frame];
	}
	else
	{
		OutRotation = FQuat::Identity();
	}

	if (Frame < Track->InternalTrack.ScaleKeys.Num())
	{
		OutScale = Track->InternalTrack.ScaleKeys[Frame];
	}
	else
	{
		OutScale = FVector::One();
	}

	return true;
}

FVector UAnimSequence::InterpolatePosition(const TArray<FVector>& Keys, float Alpha, int32 Frame0, int32 Frame1) const
{
	if (Keys.Num() == 0)
	{
		return FVector::Zero();
	}

	if (Frame0 >= Keys.Num())
	{
		return Keys[Keys.Num() - 1];
	}

	if (Frame1 >= Keys.Num() || Frame0 == Frame1)
	{
		return Keys[Frame0];
	}

	return FVector::Lerp(Keys[Frame0], Keys[Frame1], Alpha);
}

FQuat UAnimSequence::InterpolateRotation(const TArray<FQuat>& Keys, float Alpha, int32 Frame0, int32 Frame1) const
{
	if (Keys.Num() == 0)
	{
		return FQuat::Identity();
	}

	if (Frame0 >= Keys.Num())
	{
		return Keys[Keys.Num() - 1];
	}

	if (Frame1 >= Keys.Num() || Frame0 == Frame1)
	{
		return Keys[Frame0];
	}

	return FQuat::Slerp(Keys[Frame0], Keys[Frame1], Alpha);
}

FVector UAnimSequence::InterpolateScale(const TArray<FVector>& Keys, float Alpha, int32 Frame0, int32 Frame1) const
{
	if (Keys.Num() == 0)
	{
		return FVector::One();
	}

	if (Frame0 >= Keys.Num())
	{
		return Keys[Keys.Num() - 1];
	}

	if (Frame1 >= Keys.Num() || Frame0 == Frame1)
	{
		return Keys[Frame0];
	}

	return FVector::Lerp(Keys[Frame0], Keys[Frame1], Alpha);
}
