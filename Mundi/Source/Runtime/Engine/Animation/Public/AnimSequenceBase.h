#pragma once

/**
 * @brief Animation 시퀀스의 추상 기본 클래스
 * @details 재생 및 평가하여 포즈를 생성할 수 있는 Animation 데이터의 기본 클래스
 *
 * @var Notifies Animation Notify 배열 (시간 순 정렬됨)
 * @var SequenceLength Animation 길이 (초 단위)
 * @var RateScale 재생 속도 스케일 (1.0 = 정상 속도)
 * @var bLoop 루프 재생 여부
 */
UCLASS()
class UAnimSequenceBase :
	public UObject
{
	DECLARE_CLASS(UAnimSequenceBase, UObject)

public:
	UAnimSequenceBase();
	~UAnimSequenceBase() override = default;

	void SortNotifies();
	void GetAnimNotifiesFromDeltaPositions(float PreviousTime, float CurrentTime, TArray<const FAnimNotifyEvent*>& OutNotifies) const;
	void AddNotify(const FAnimNotifyEvent& NewNotify);
	void ClearNotifies();

	// Getters
	virtual float GetPlayLength() const { return SequenceLength; }
	bool IsLooping() const { return bLoop; }

private:
	TArray<FAnimNotifyEvent> Notifies;
	float SequenceLength;
	float RateScale;
	bool bLoop;
};
