#include "pch.h"
#include "AnimSequenceBase.h"
#include "AnimDataModel.h"
#include "AnimationTypes.h"

IMPLEMENT_CLASS(UAnimSequenceBase)

UAnimSequenceBase::UAnimSequenceBase()
	: DataModel(nullptr)
	, SequenceLength(0.0f)
	, RateScale(1.0f)
	, bLoop(true)
{
}

UAnimSequenceBase::~UAnimSequenceBase()
{
	// DataModel 정리 (메모리 누수 방지)
	if (DataModel)
	{
		ObjectFactory::DeleteObject(DataModel);
		DataModel = nullptr;
	}
}

/**
 * @brief 데이터 모델 설정 (기존 DataModel 삭제 후 새로운 것으로 교체)
 */
void UAnimSequenceBase::SetDataModel(UAnimDataModel* InDataModel)
{
	// 기존 DataModel 삭제 (메모리 누수 방지)
	if (DataModel && DataModel != InDataModel)
	{
		ObjectFactory::DeleteObject(DataModel);
		DataModel = nullptr;
	}

	DataModel = InDataModel;
}

/**
 * @brief 다른 AnimSequence로부터 DataModel 소유권 이전
 * @param Other 소유권을 이전할 AnimSequenceBase 객체
 */
void UAnimSequenceBase::TransferDataModelFrom(UAnimSequenceBase* Other)
{
	if (!Other || Other == this)
	{
		return;
	}

	// 기존 DataModel 삭제
	if (DataModel && DataModel != Other->DataModel)
	{
		ObjectFactory::DeleteObject(DataModel);
	}

	// 소유권 이전
	DataModel = Other->DataModel;
	Other->DataModel = nullptr;  // 소유권 포기 (삭제하지 않음)
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
 * @brief 특정 시간 구간의 Notify 수집 (UE 표준 로직)
 * @param PreviousTime 이전 시간
 * @param CurrentTime 현재 시간
 * @param OutNotifies 트리거된 Notify 배열 (출력)
 *
 * @details UE 표준 로직:
 *   - Forward: (PreviousPosition, CurrentPosition] (exclusive to inclusive)
 *   - Backward: [CurrentPosition, PreviousPosition) (inclusive to exclusive)
 */
void UAnimSequenceBase::GetAnimNotifiesFromDeltaPositions(float PreviousTime, float CurrentTime,
                                                          TArray<const FAnimNotifyEvent*>& OutNotifies) const
{
	const bool bPlayingForward = (CurrentTime >= PreviousTime);
	const float AnimLength = GetPlayLength();

	for (const FAnimNotifyEvent& NotifyEvent : Notifies)
	{
		float NotifyStartTime = NotifyEvent.TriggerTime;
		float NotifyEndTime = NotifyStartTime + NotifyEvent.Duration;

		if (NotifyEvent.Duration > 0.0f)
		{
			// NotifyState: 활성 구간 전체에서 반환 (Begin/Tick/End 처리는 AnimInstance에서)
			// CurrentTime이 [NotifyStartTime, NotifyEndTime) 구간에 있으면 활성화 상태
			if (bPlayingForward)
			{
				// Forward 재생: CurrentTime이 활성 구간 내에 있으면 반환
				if ((CurrentTime >= NotifyStartTime) && (CurrentTime < NotifyEndTime))
				{
					OutNotifies.push_back(&NotifyEvent);
				}
			}
			else
			{
				// Backward 재생: CurrentTime이 활성 구간 내에 있으면 반환
				if ((CurrentTime >= NotifyStartTime) && (CurrentTime < NotifyEndTime))
				{
					OutNotifies.push_back(&NotifyEvent);
				}
			}
		}
		else
		{
			// 일반 Notify: TriggerTime을 지나는 프레임에서만 반환
			if (bPlayingForward)
			{
				if ((NotifyStartTime > PreviousTime) && (NotifyStartTime <= CurrentTime))
				{
					OutNotifies.push_back(&NotifyEvent);
				}
			}
			else
			{
				if ((NotifyStartTime < PreviousTime) && (NotifyStartTime >= CurrentTime))
				{
					OutNotifies.push_back(&NotifyEvent);
				}
			}
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

/**
 * @brief PIE용 SubObject 복제 (Notifies 깊은 복사)
 */
void UAnimSequenceBase::DuplicateSubObjects()
{
	Super::DuplicateSubObjects();

	// Notifies 배열 깊은 복사 (TArray 대입 연산자가 깊은 복사 수행)
	TArray<FAnimNotifyEvent> NotifiesCopy = Notifies;
	Notifies = NotifiesCopy;

	// DataModel 복제 (UObject이므로 Duplicate() 호출)
	if (DataModel)
	{
		UAnimDataModel* NewDataModel = Cast<UAnimDataModel>(DataModel->Duplicate());
		DataModel = NewDataModel;
	}
}
