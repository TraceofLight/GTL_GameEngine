#include "pch.h"
#include "Source/Runtime/Engine/Animation/Public/AnimSequenceBase.h"
#include "Source/Runtime/Engine/Animation/Public/AnimTypes.h"

IMPLEMENT_CLASS(UAnimSequenceBase)

UAnimSequenceBase::UAnimSequenceBase()
	: SequenceLength(0.0f)
	  , RateScale(1.0f)
	  , bLoop(true)
{
}

/**
 * @brief Notify 배열을 시간 순으로 정렬
 */
void UAnimSequenceBase::SortNotifies()
{
	Notifies.Sort([](const FAnimNotifyEvent& A, const FAnimNotifyEvent& B)
	{
		return A.TriggerTime < B.TriggerTime;
	});
}

/**
 * @brief 특정 시간 구간의 Notify 수집
 * @param PreviousTime 이전 시간
 * @param CurrentTime 현재 시간
 * @param OutNotifies 트리거된 Notify 배열 (출력)
 */
void UAnimSequenceBase::GetAnimNotifiesFromDeltaPositions(float PreviousTime, float CurrentTime,
                                                          TArray<const FAnimNotifyEvent*>& OutNotifies) const
{
	// 시간이 역방향이면 스왑
	if (PreviousTime > CurrentTime)
	{
		std::swap(PreviousTime, CurrentTime);
	}

	for (const FAnimNotifyEvent& NotifyEvent : Notifies)
	{
		// 트리거 시간이 구간 내에 있는지 확인
		if (NotifyEvent.TriggerTime >= PreviousTime && NotifyEvent.TriggerTime < CurrentTime)
		{
			OutNotifies.push_back(&NotifyEvent);
		}
	}
}

/**
 * @brief Notify 추가
 */
void UAnimSequenceBase::AddNotify(const FAnimNotifyEvent& NewNotify)
{
	Notifies.push_back(NewNotify);
	SortNotifies();
}

/**
 * @brief 모든 Notify 제거
 */
void UAnimSequenceBase::ClearNotifies()
{
	Notifies.clear();
}
