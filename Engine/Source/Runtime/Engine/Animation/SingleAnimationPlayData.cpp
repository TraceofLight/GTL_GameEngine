#include "pch.h"
#include "SingleAnimationPlayData.h"
#include "AnimSingleNodeInstance.h"

/**
 * @brief AnimInstance 초기화
 */
void FSingleAnimationPlayData::Initialize(UAnimSingleNodeInstance* Instance)
{
	if (!Instance)
	{
		return;
	}

	if (AnimToPlay)
	{
		Instance->SetAnimationAsset(AnimToPlay);
		Instance->SetPlayRate(SavedPlayRate);
		Instance->SetPosition(SavedPosition);

		if (bSavedPlaying)
		{
			Instance->Play(bSavedLooping);
		}
	}
}

/**
 * @brief AnimInstance 상태로부터 데이터 채우기
 */
void FSingleAnimationPlayData::PopulateFrom(UAnimSingleNodeInstance* Instance)
{
	if (!Instance)
	{
		return;
	}

	AnimToPlay = Cast<UAnimSequence>(Instance->GetCurrentAnimation());
	bSavedLooping = Instance->IsLooping();
	bSavedPlaying = Instance->IsPlaying();
	SavedPosition = Instance->GetCurrentTime();
	SavedPlayRate = Instance->GetPlayRate();
}

/**
 * @brief 재생 위치 유효성 검증
 */
void FSingleAnimationPlayData::ValidatePosition()
{
	if (AnimToPlay)
	{
		float AnimLength = AnimToPlay->GetPlayLength();
		if (SavedPosition < 0.0f)
		{
			SavedPosition = 0.0f;
		}
		else if (SavedPosition > AnimLength)
		{
			SavedPosition = AnimLength;
		}
	}
	else
	{
		SavedPosition = 0.0f;
	}
}
