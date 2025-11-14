#pragma once

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

	FAnimNotifyEvent()
		: TriggerTime(0.0f)
		, Duration(0.0f)
		, TriggerWeightThreshold(0.0f)
	{
	}

	FAnimNotifyEvent(float InTriggerTime, FName InNotifyName, float InDuration = 0.0f)
		: TriggerTime(InTriggerTime)
		, Duration(InDuration)
		, NotifyName(InNotifyName)
		, TriggerWeightThreshold(0.0f)
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
