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
